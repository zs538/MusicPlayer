#include "AudioEngine.h"
#include "AudioOutputWorker.h"
#include "SPSCRingBuffer.h"
#include "BufferIODevice.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QDebug>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QMediaDevices>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

Q_LOGGING_CATEGORY(lcAudioEngine, "musicplayer.audio.engine")

QString AudioEngine::currentFilePath() const
{
    QMutexLocker locker(&m_nextMutex);
    return m_currentFilePath;
}

static constexpr size_t RING_BUFFER_SIZE = 2 * 1024 * 1024;  // 2MB for gapless transitions
static constexpr int POSITION_UPDATE_INTERVAL_MS = 100;
static constexpr int SESSION_SAMPLE_RATE = 48000;  // Fixed output rate for gapless playback

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
    , m_ringBuffer(std::make_unique<SPSCRingBuffer>(RING_BUFFER_SIZE))
{
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(POSITION_UPDATE_INTERVAL_MS);
    connect(m_positionTimer, &QTimer::timeout, this, &AudioEngine::updatePosition);
}

AudioEngine::~AudioEngine()
{
    stop();
}

qint64 AudioEngine::positionMs() const
{
    if (m_outputSampleRate <= 0 || !m_audioInitialized)
        return 0;
    
    // Use cached processedUSecs pushed from audio thread (thread-safe)
    qint64 processedUs = m_cachedProcessedUSecs.load();
    qint64 processedSamples = (processedUs * m_outputSampleRate) / 1000000;
    
    // Subtract track start offset and add seek offset
    qint64 trackSamples = processedSamples - m_trackStartSample;
    qint64 posMs = (trackSamples * 1000) / m_outputSampleRate + m_seekOffsetMs;
    
    return qBound(0LL, posMs, m_durationMs > 0 ? m_durationMs : posMs);
}

bool AudioEngine::play(const QString &filePath)
{
    stop();
    
    if (!openFile(filePath)) {
        setState(Error);
        return false;
    }

    qCDebug(lcAudioEngine) << "play() opened" << filePath << "durationMs=" << m_durationMs;
    
    {
        QMutexLocker locker(&m_nextMutex);
        m_currentFilePath = filePath;
    }
    
    // Clear gapless state for fresh playback
    {
        QMutexLocker locker(&m_nextMutex);
        m_nextFilePath.clear();
        m_hasNextPrepared = false;
        m_nextLocked = false;
        m_trackStartSample = 0;
        m_seekOffsetMs = 0;
        m_currentTrackSamples = 0;
        m_trackAboutToFinishEmitted = false;
    }
    
    if (!initAudioOutput(m_outputSampleRate, 2)) {
        closeFile();
        setState(Error);
        emit errorOccurred(1, "Failed to initialize audio output");
        return false;
    }
    
    startDecodeThread();

    // Resume audio output via worker (queued call to audio thread)
    QMetaObject::invokeMethod(m_audioWorker, "resume", Qt::QueuedConnection);
    setState(Playing);
    m_positionTimer->start();
    
    return true;
}

bool AudioEngine::preopenNextFile(const QString &filePath, qint64 *outDurationMs)
{
    closeNextFile();

    std::string path = filePath.toStdString();

    if (avformat_open_input(&m_nextFormatCtx, path.c_str(), nullptr, nullptr) < 0) {
        return false;
    }

    if (avformat_find_stream_info(m_nextFormatCtx, nullptr) < 0) {
        closeNextFile();
        return false;
    }

    m_nextAudioStreamIndex = -1;
    for (unsigned int i = 0; i < m_nextFormatCtx->nb_streams; i++) {
        if (m_nextFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_nextAudioStreamIndex = (int)i;
            break;
        }
    }

    if (m_nextAudioStreamIndex < 0) {
        closeNextFile();
        return false;
    }

    AVStream *audioStream = m_nextFormatCtx->streams[m_nextAudioStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!codec) {
        closeNextFile();
        return false;
    }

    m_nextCodecCtx = avcodec_alloc_context3(codec);
    if (!m_nextCodecCtx) {
        closeNextFile();
        return false;
    }

    if (avcodec_parameters_to_context(m_nextCodecCtx, audioStream->codecpar) < 0) {
        closeNextFile();
        return false;
    }

    if (avcodec_open2(m_nextCodecCtx, codec, nullptr) < 0) {
        closeNextFile();
        return false;
    }

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2);

    if (swr_alloc_set_opts2(&m_nextSwrCtx,
                            &outLayout, AV_SAMPLE_FMT_S16, SESSION_SAMPLE_RATE,
                            &m_nextCodecCtx->ch_layout, m_nextCodecCtx->sample_fmt, m_nextCodecCtx->sample_rate,
                            0, nullptr) < 0) {
        closeNextFile();
        return false;
    }

    if (swr_init(m_nextSwrCtx) < 0) {
        closeNextFile();
        return false;
    }

    qint64 durationMs = 0;
    if (m_nextFormatCtx->duration != AV_NOPTS_VALUE) {
        durationMs = m_nextFormatCtx->duration / (AV_TIME_BASE / 1000);
    }
    if (outDurationMs) {
        *outDurationMs = durationMs;
    }

    // Mark which path the preopened contexts correspond to.
    {
        QMutexLocker locker(&m_nextMutex);
        m_nextPreopenedPath = filePath;
    }
    return true;
}

void AudioEngine::pause()
{
    if (m_state == Playing) {
        if (m_audioWorker)
            QMetaObject::invokeMethod(m_audioWorker, "suspend", Qt::QueuedConnection);
        setState(Paused);
        m_positionTimer->stop();
    }
}

void AudioEngine::resume()
{
    if (m_state == Paused) {
        if (m_audioWorker)
            QMetaObject::invokeMethod(m_audioWorker, "resume", Qt::QueuedConnection);
        setState(Playing);
        m_positionTimer->start();
    }
}

void AudioEngine::stop()
{
    // Abort buffer first to unblock any waiting writes in decode thread
    m_ringBuffer->abort();
    
    stopDecodeThread();
    
    // Stop audio output thread
    if (m_audioThread && m_audioThread->isRunning()) {
        QMetaObject::invokeMethod(m_audioWorker, "stop", Qt::BlockingQueuedConnection);
        m_audioThread->quit();
        m_audioThread->wait(1000);
    }
    m_audioThread.reset();
    m_audioWorker = nullptr;
    m_audioInitialized = false;
    m_cachedProcessedUSecs = 0;

    closeNextFile();
    closeFile();
    m_ringBuffer->clear();
    m_ringBuffer->reset();  // Clear abort state for next use
    m_samplesWritten = 0;
    m_trackStartSample = 0;
    m_seekOffsetMs = 0;
    m_currentTrackSamples = 0;
    m_currentFilePath.clear();
    m_nextFilePath.clear();
    m_hasNextPrepared = false;
    m_nextLocked = false;
    m_durationMs = 0;
    
    setState(Stopped);
    m_positionTimer->stop();

    qCDebug(lcAudioEngine) << "stop() complete";
}

void AudioEngine::seek(qint64 positionMs)
{
    if (m_state == Stopped || !m_formatCtx)
        return;
    
    // Clamp to valid range
    positionMs = qBound(0LL, positionMs, m_durationMs);

    qCDebug(lcAudioEngine) << "seek() requested" << "posMs=" << positionMs
                           << "state=" << m_state
                           << "durationMs=" << m_durationMs;
    
    // CRITICAL: Seeking clears the prepared next track
    // This prevents the bug where seeking near end triggers wrong track
    {
        QMutexLocker locker(&m_nextMutex);
        m_nextFilePath.clear();
        m_hasNextPrepared = false;
        m_nextLocked = false;
    }
    
    // Abort buffer to unblock any waiting writes in decode thread
    m_ringBuffer->abort();
    
    // CRITICAL: Always stop decode thread before seeking.
    // This eliminates the race where decoder hits EOF before processing the seek.
    stopDecodeThread();
    
    // Stop audio output and clear buffers for clean seek
    bool wasPlaying = (m_state == Playing);
    if (m_audioThread && m_audioThread->isRunning()) {
        QMetaObject::invokeMethod(m_audioWorker, "stop", Qt::BlockingQueuedConnection);
        m_audioThread->quit();
        m_audioThread->wait(1000);
    }
    m_audioThread.reset();
    m_audioWorker = nullptr;
    m_audioInitialized = false;
    m_cachedProcessedUSecs = 0;
    
    m_ringBuffer->clear();
    m_ringBuffer->reset();  // Clear abort state
    m_ringBuffer->setFinished(false);
    
    // Reset sample tracking - use seek offset for position calculation
    m_samplesWritten = 0;
    m_trackStartSample = 0;
    m_currentTrackSamples = 0;
    m_seekOffsetMs = positionMs;
    m_trackAboutToFinishEmitted = false;
    
    // Seek in the file
    int64_t timestamp = (positionMs * AV_TIME_BASE) / 1000;
    int seekRet = av_seek_frame(m_formatCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(m_codecCtx);
    
    qCDebug(lcAudioEngine) << "seek() applied" << "posMs=" << positionMs
                           << "seekRet=" << seekRet;
    
    // Restart audio output
    if (wasPlaying) {
        // Recreate audio output in new thread for clean state
        initAudioOutput(m_outputSampleRate, 2);
        if (m_audioWorker) {
            QMetaObject::invokeMethod(m_audioWorker, "resume", Qt::QueuedConnection);
        }
    }

    // Always restart decode thread after seek
    startDecodeThread();
}

void AudioEngine::prepareNext(const QString &filePath)
{
    QMutexLocker locker(&m_nextMutex);
    if (m_nextLocked) {
        return;
    }
    m_nextFilePath = filePath;
    m_hasNextPrepared = !filePath.isEmpty();
}

void AudioEngine::clearNext()
{
    QMutexLocker locker(&m_nextMutex);
    if (m_nextLocked) return;  // Can't clear during transition
    
    m_nextFilePath.clear();
    m_hasNextPrepared = false;
}

void AudioEngine::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    qCDebug(lcAudioEngine) << "setVolume:" << m_volume << "worker:" << (m_audioWorker ? "yes" : "no");
    // Software volume control via sample scaling in BufferIODevice
    if (m_audioWorker) {
        QMetaObject::invokeMethod(m_audioWorker, "setVolume", Qt::QueuedConnection,
                                  Q_ARG(qreal, m_volume));
    }
}

void AudioEngine::setSinkBufferMs(int ms)
{
    ms = qBound(50, ms, 500);  // Clamp to reasonable range
    if (m_sinkBufferMs == ms) return;
    m_sinkBufferMs = ms;
    emit sinkBufferMsChanged(ms);
}

void AudioEngine::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

bool AudioEngine::openFile(const QString &filePath, qint64 *outDurationMs)
{
    closeFile();
    
    std::string path = filePath.toStdString();
    
    if (avformat_open_input(&m_formatCtx, path.c_str(), nullptr, nullptr) < 0) {
        emit errorOccurred(2, "Could not open file: " + filePath);
        return false;
    }
    
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        emit errorOccurred(3, "Could not find stream info");
        closeFile();
        return false;
    }
    
    m_audioStreamIndex = -1;
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            break;
        }
    }
    
    if (m_audioStreamIndex < 0) {
        emit errorOccurred(4, "No audio stream found");
        closeFile();
        return false;
    }
    
    AVStream *audioStream = m_formatCtx->streams[m_audioStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!codec) {
        emit errorOccurred(5, "Codec not found");
        closeFile();
        return false;
    }
    
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        emit errorOccurred(6, "Could not allocate codec context");
        closeFile();
        return false;
    }
    
    if (avcodec_parameters_to_context(m_codecCtx, audioStream->codecpar) < 0) {
        emit errorOccurred(7, "Could not copy codec parameters");
        closeFile();
        return false;
    }
    
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        emit errorOccurred(8, "Could not open codec");
        closeFile();
        return false;
    }
    
    // Use fixed session sample rate for gapless playback
    // All tracks are resampled to this rate to ensure seamless transitions
    m_outputSampleRate = SESSION_SAMPLE_RATE;
    
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2);
    
    if (swr_alloc_set_opts2(&m_swrCtx,
                            &outLayout, AV_SAMPLE_FMT_S16, SESSION_SAMPLE_RATE,
                            &m_codecCtx->ch_layout, m_codecCtx->sample_fmt, m_codecCtx->sample_rate,
                            0, nullptr) < 0) {
        emit errorOccurred(9, "Could not set resampler options");
        closeFile();
        return false;
    }
    
    if (swr_init(m_swrCtx) < 0) {
        emit errorOccurred(10, "Could not initialize resampler");
        closeFile();
        return false;
    }
    
    qint64 durationMs = 0;
    if (m_formatCtx->duration != AV_NOPTS_VALUE) {
        durationMs = m_formatCtx->duration / (AV_TIME_BASE / 1000);
    }

    if (outDurationMs) {
        *outDurationMs = durationMs;
    } else {
        // Only update public duration for the currently playing track.
        // When we open the next track in the decode thread, we must not
        // mutate/emit current duration early (it breaks remaining-time logic).
        m_durationMs = durationMs;
        emit durationMsChanged(m_durationMs);
    }
    
    return true;
}

void AudioEngine::closeFile()
{
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    
    m_audioStreamIndex = -1;
}

void AudioEngine::closeNextFile()
{
    if (m_nextSwrCtx) {
        swr_free(&m_nextSwrCtx);
        m_nextSwrCtx = nullptr;
    }

    if (m_nextCodecCtx) {
        avcodec_free_context(&m_nextCodecCtx);
        m_nextCodecCtx = nullptr;
    }

    if (m_nextFormatCtx) {
        avformat_close_input(&m_nextFormatCtx);
        m_nextFormatCtx = nullptr;
    }

    m_nextAudioStreamIndex = -1;
    {
        QMutexLocker locker(&m_nextMutex);
        m_nextPreopenedPath.clear();
    }
}

bool AudioEngine::initAudioOutput(int sampleRate, int channels)
{
    // Create dedicated audio output thread with high priority
    m_audioThread = std::make_unique<QThread>();
    m_audioThread->setObjectName(QStringLiteral("AudioOutputThread"));
    
    // Create worker (will be moved to audio thread)
    m_audioWorker = new AudioOutputWorker(m_ringBuffer.get());
    m_audioWorker->moveToThread(m_audioThread.get());
    
    // Connect worker signals to engine slots (queued connections for thread safety)
    connect(m_audioWorker, &AudioOutputWorker::initialized,
            this, &AudioEngine::onAudioWorkerInitialized, Qt::QueuedConnection);
    connect(m_audioWorker, &AudioOutputWorker::stateChanged,
            this, &AudioEngine::onAudioStateChanged, Qt::QueuedConnection);
    connect(m_audioWorker, &AudioOutputWorker::positionUpdated,
            this, &AudioEngine::onPositionUpdated, Qt::QueuedConnection);
    
    // Clean up worker when thread finishes
    connect(m_audioThread.get(), &QThread::finished, m_audioWorker, &QObject::deleteLater);
    
    // Start the audio thread with high priority
    m_audioThread->start(QThread::HighPriority);
    
    // Initialize the worker (queued call to audio thread)
    QMetaObject::invokeMethod(m_audioWorker, "initialize", Qt::QueuedConnection,
                              Q_ARG(int, sampleRate), Q_ARG(int, channels), Q_ARG(int, m_sinkBufferMs));
    
    // Start the audio sink (suspended initially)
    QMetaObject::invokeMethod(m_audioWorker, "start", Qt::QueuedConnection);
    
    m_audioInitialized = true;
    
    qCDebug(lcAudioEngine) << "initAudioOutput()" << "rate=" << sampleRate
                           << "ch=" << channels << "bufferMs=" << m_sinkBufferMs
                           << "(audio thread started)";
    
    return true;
}

void AudioEngine::startDecodeThread()
{
    if (m_decodeThread && m_decodeThread->isRunning())
        return;
    m_decodeThread.reset();

    m_stopDecoding = false;
    m_ringBuffer->clear();
    m_ringBuffer->setFinished(false);
    
    m_decodeThread.reset(QThread::create([this]() {
        qCDebug(lcAudioEngine) << "decode thread started";
        decodeLoop();
        qCDebug(lcAudioEngine) << "decodeLoop() exited";
    }));
    m_decodeThread->start(QThread::HighPriority);
}

void AudioEngine::stopDecodeThread()
{
    m_stopDecoding = true;
    
    if (m_decodeThread && m_decodeThread->isRunning()) {
        // decodeLoop checks m_stopDecoding and ring buffer abort, so it will exit cleanly
        m_decodeThread->wait(2000);
        if (m_decodeThread->isRunning()) {
            qCWarning(lcAudioEngine) << "stopDecodeThread(): force terminating";
            m_decodeThread->terminate();
            m_decodeThread->wait();
        }
    }
    m_decodeThread.reset();
}

void AudioEngine::decodeLoop()
{
    if (!m_formatCtx || !m_codecCtx || !m_swrCtx) {
        return;
    }

    qCDebug(lcAudioEngine) << "decodeLoop(): enter";
    
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    std::vector<uint8_t> outputBuffer(32768);
    
    while (!m_stopDecoding) {
        // Pre-open next track while buffer has space (RingBuffer::write() will block if full)
        {
            QString pathToPreopen;
            bool shouldClearPreopen = false;
            {
                QMutexLocker locker(&m_nextMutex);
                const bool hasNext = m_hasNextPrepared && !m_nextFilePath.isEmpty();
                if (!hasNext) {
                    if (!m_nextPreopenedPath.isEmpty()) {
                        shouldClearPreopen = true;
                    }
                } else if (!m_nextLocked && m_nextPreopenedPath != m_nextFilePath) {
                    pathToPreopen = m_nextFilePath;
                }
            }

            if (shouldClearPreopen) {
                closeNextFile();
            } else if (!pathToPreopen.isEmpty()) {
                qint64 dur = 0;
                if (preopenNextFile(pathToPreopen, &dur)) {
                    QMutexLocker locker(&m_nextMutex);
                    m_nextTrackDurationMs = dur;
                }
            }
        }
        
        int ret = av_read_frame(m_formatCtx, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                qCDebug(lcAudioEngine) << "decodeLoop(): reached EOF";
                // Current track finished decoding
                // Store total samples for this track
                m_currentTrackSamples = m_samplesWritten.load();
                
                // Flush resampler
                if (m_swrCtx) {
                    uint8_t *outPtr = outputBuffer.data();
                    int flushed = swr_convert(m_swrCtx, &outPtr, outputBuffer.size() / 4, nullptr, 0);
                    if (flushed > 0) {
                        size_t bytesToWrite = flushed * 2 * sizeof(int16_t);
                        // Volume scaling moved to BufferIODevice::readData() to avoid double-application
                        m_ringBuffer->write(outputBuffer.data(), bytesToWrite);
                        m_samplesWritten += flushed;
                        m_currentTrackSamples = m_samplesWritten.load();
                    }
                }
                
                // Check if we have a next track to decode for gapless
                // CRITICAL: Wait for next track to be prepared - the decoder runs ahead of playback
                // so trackAboutToFinish may not have been emitted yet
                QString nextPath;
                bool usePreopened = false;
                qint64 nextDurationMs = 0;
                
                // Wait for next track - decoder runs ahead of playback
                // Calculate wait time based on buffer content
                qint64 bufferMs = (m_ringBuffer->availableToRead() * 1000) / (m_outputSampleRate * 4);
                qint64 maxWaitMs = bufferMs + GAPLESS_LEAD_IN_MS;
                
                QElapsedTimer waitTimer;
                waitTimer.start();
                while (!m_stopDecoding && waitTimer.elapsed() < maxWaitMs) {
                    {
                        QMutexLocker locker(&m_nextMutex);
                        if (m_hasNextPrepared && !m_nextFilePath.isEmpty()) {
                            break;
                        }
                    }
                    QThread::msleep(10);
                }
                
                {
                    QMutexLocker locker(&m_nextMutex);
                    if (m_hasNextPrepared && !m_nextFilePath.isEmpty()) {
                        // Lock the next track - no more changes allowed
                        m_nextLocked = true;
                        nextPath = m_nextFilePath;
                        m_nextFilePath.clear();
                        m_hasNextPrepared = false;

                        if (!m_nextPreopenedPath.isEmpty() && m_nextPreopenedPath == nextPath &&
                            m_nextFormatCtx && m_nextCodecCtx && m_nextSwrCtx && m_nextAudioStreamIndex >= 0) {
                            usePreopened = true;
                            nextDurationMs = m_nextTrackDurationMs;
                        }
                    }
                }

                if (!nextPath.isEmpty()) {
                    closeFile();

                    bool opened = false;
                    if (usePreopened) {
                        // Swap in the preopened contexts (no I/O at the boundary).
                        m_formatCtx = m_nextFormatCtx; m_nextFormatCtx = nullptr;
                        m_codecCtx = m_nextCodecCtx; m_nextCodecCtx = nullptr;
                        m_swrCtx = m_nextSwrCtx; m_nextSwrCtx = nullptr;
                        m_audioStreamIndex = m_nextAudioStreamIndex;
                        m_nextAudioStreamIndex = -1;
                        {
                            QMutexLocker locker(&m_nextMutex);
                            m_nextPreopenedPath.clear();
                        }
                        opened = (m_formatCtx && m_codecCtx && m_swrCtx && m_audioStreamIndex >= 0);
                    } else {
                        opened = openFile(nextPath, &nextDurationMs);
                    }

                    if (opened) {
                        qCDebug(lcAudioEngine) << "decodeLoop(): switched to next track" << nextPath
                                               << "usePreopened=" << usePreopened;
                        // Reset per-track decode counter for the new track.
                        // We must NOT touch m_currentTrackSamples here because it still refers to
                        // the finishing track and is needed for the playback boundary detection.
                        m_samplesWritten = 0;
                        // Store next track info. Note: we must not update m_durationMs yet.
                        {
                            QMutexLocker locker(&m_nextMutex);
                            m_currentFilePath = nextPath;
                            m_nextTrackDurationMs = nextDurationMs;
                        }
                        continue;
                    }
                }
                
                // No next track or failed to open - mark as finished
                m_ringBuffer->setFinished(true);
                qCDebug(lcAudioEngine) << "decodeLoop(): finished (no next track)";
            }
            break;
        }
        
        if (packet->stream_index != m_audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }
        
        ret = avcodec_send_packet(m_codecCtx, packet);
        av_packet_unref(packet);
        
        if (ret < 0)
            continue;
        
        while (ret >= 0 && !m_stopDecoding) {
            ret = avcodec_receive_frame(m_codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;
            
            int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
            if (outSamples * 4 > (int)outputBuffer.size()) {
                outputBuffer.resize(outSamples * 4);
            }
            
            uint8_t *outPtr = outputBuffer.data();
            int converted = swr_convert(m_swrCtx, &outPtr, outSamples,
                                        (const uint8_t **)frame->extended_data, frame->nb_samples);
            
            if (converted > 0) {
                size_t bytesToWrite = converted * 2 * sizeof(int16_t);
                // Volume scaling moved to BufferIODevice::readData() to avoid double-application
                m_samplesWritten += converted;
                m_ringBuffer->write(outputBuffer.data(), bytesToWrite);
            }
            
            av_frame_unref(frame);
        }
    }
    
    av_packet_free(&packet);
    av_frame_free(&frame);
}

void AudioEngine::onAudioStateChanged(QAudio::State state)
{
    bool nextLockedSnapshot = false;
    {
        QMutexLocker locker(&m_nextMutex);
        nextLockedSnapshot = m_nextLocked;
    }

    qCDebug(lcAudioEngine) << "QAudioSink stateChanged" << state
                           << "engineState=" << m_state
                           << "finished=" << m_ringBuffer->isFinished()
                           << "avail=" << static_cast<qulonglong>(m_ringBuffer->availableToRead())
                           << "nextLocked=" << nextLockedSnapshot;
    if (state == QAudio::IdleState && m_state == Playing) {
        // Buffer underrun or end of playback
        if (m_ringBuffer->isFinished() && m_ringBuffer->availableToRead() == 0) {
            // Check if we're in a gapless transition
            if (nextLockedSnapshot) {
                // Don't stop - gapless transition will handle it
                return;
            }
            // Actually finished - stop BEFORE emitting trackFinished
            stop();
            emit trackFinished();
        }
    }
}

void AudioEngine::onAudioWorkerInitialized(bool success)
{
    if (!success) {
        qCWarning(lcAudioEngine) << "Audio worker initialization failed";
        m_audioInitialized = false;
    } else {
        // Apply current volume to the newly initialized audio sink
        QMetaObject::invokeMethod(m_audioWorker, "setVolume", Qt::QueuedConnection,
                                  Q_ARG(qreal, m_volume));
    }
}

void AudioEngine::onPositionUpdated(qint64 processedUSecs)
{
    // Cache the position pushed from audio thread (thread-safe atomic store)
    m_cachedProcessedUSecs.store(processedUSecs);
}

void AudioEngine::updatePosition()
{
    if (m_state == Playing) {
        // Check for gapless transition
        checkGaplessTransition();
        
        // Emit trackAboutToFinish based on PLAYBACK position (not decode position)
        // This matches the helpfiles approach and ensures the signal is emitted
        // when the user is actually near the end of the track
        qint64 pos = positionMs();
        qint64 remaining = m_durationMs - pos;
        if (!m_trackAboutToFinishEmitted && remaining <= GAPLESS_LEAD_IN_MS && remaining > 0) {
            m_trackAboutToFinishEmitted = true;
            emit trackAboutToFinish(remaining);
        }

        if (m_durationMs > 0 && pos >= m_durationMs) {
            bool nextLockedSnapshot = false;
            {
                QMutexLocker locker(&m_nextMutex);
                nextLockedSnapshot = m_nextLocked;
            }

            if (!nextLockedSnapshot) {
                qCDebug(lcAudioEngine) << "updatePosition(): reached end" << "posMs=" << pos
                                       << "durationMs=" << m_durationMs;
                stop();
                emit trackFinished();
                return;
            }
        }
    }
    
    emit positionMsChanged(positionMs());
}

void AudioEngine::checkGaplessTransition()
{
    QString filePathToEmit;
    qint64 durationToEmit = 0;
    bool shouldEmit = false;
    {
        QMutexLocker locker(&m_nextMutex);
        if (!m_nextLocked || m_currentTrackSamples == 0)
            return;
        // We'll unlock while doing the processedUSecs check (no shared state needed).
    }
    
    if (!m_audioInitialized)
        return;
    
    // Calculate current playback position in samples (using cached value from audio thread)
    qint64 processedUs = m_cachedProcessedUSecs.load();
    qint64 playedSamples = (processedUs * m_outputSampleRate) / 1000000;
    
    // Check if we've crossed the track boundary (played past current track's samples)
    if (playedSamples >= m_trackStartSample + m_currentTrackSamples) {
        qCDebug(lcAudioEngine) << "checkGaplessTransition(): boundary crossed" << "playedSamples=" << playedSamples
                               << "trackStartSample=" << m_trackStartSample
                               << "currentTrackSamples=" << m_currentTrackSamples;
        {
            QMutexLocker locker(&m_nextMutex);
            // We've transitioned to the next track
            //
            // CRITICAL: Set track start sample to current processed samples
            // This ensures position() returns ~0 for the new track
            m_trackStartSample = playedSamples;
            m_seekOffsetMs = 0;
            m_currentTrackSamples = 0;  // Will be set when next track finishes decoding
            m_trackAboutToFinishEmitted = false;
            m_nextLocked = false;

            // Now publish next-track duration and file path
            m_durationMs = m_nextTrackDurationMs;
            durationToEmit = m_durationMs;
            filePathToEmit = m_currentFilePath;
            shouldEmit = true;
        }

        if (shouldEmit) {
            emit durationMsChanged(durationToEmit);
            emit trackChanged(filePathToEmit);
        }
    }
}

