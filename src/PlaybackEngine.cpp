#include "PlaybackEngine.h"

#include <QDebug>
#include <QFile>
#include <QMediaDevices>
#include <cstring>

// FFmpeg headers (C linkage)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

// ============================================================================
// AudioBuffer implementation
// ============================================================================

AudioBuffer::AudioBuffer(size_t capacityBytes)
    : m_buffer(capacityBytes)
{
}

size_t AudioBuffer::write(const char* data, size_t bytes)
{
    QMutexLocker lock(&m_mutex);
    if (m_aborted) return 0;
    
    size_t written = 0;
    while (written < bytes) {
        if (m_aborted) return written;
        
        size_t space = freeSpace();
        if (space == 0) {
            // Buffer full, wait for consumer
            m_spaceAvailable.wait(&m_mutex, 10);
            continue;
        }
        
        size_t toWrite = qMin(bytes - written, space);
        
        // Handle wrap-around
        size_t firstPart = qMin(toWrite, m_buffer.size() - m_writePos);
        std::memcpy(m_buffer.data() + m_writePos, data + written, firstPart);
        
        if (toWrite > firstPart) {
            std::memcpy(m_buffer.data(), data + written + firstPart, toWrite - firstPart);
        }
        
        m_writePos = (m_writePos + toWrite) % m_buffer.size();
        written += toWrite;
    }
    
    return written;
}

size_t AudioBuffer::read(char* data, size_t bytes)
{
    QMutexLocker lock(&m_mutex);
    
    size_t avail = available();
    if (avail == 0) {
        // No data available, return silence
        std::memset(data, 0, bytes);
        return bytes;
    }
    
    size_t toRead = qMin(bytes, avail);
    
    // Handle wrap-around
    size_t firstPart = qMin(toRead, m_buffer.size() - m_readPos);
    std::memcpy(data, m_buffer.data() + m_readPos, firstPart);
    
    if (toRead > firstPart) {
        std::memcpy(data + firstPart, m_buffer.data(), toRead - firstPart);
    }
    
    m_readPos = (m_readPos + toRead) % m_buffer.size();
    
    // Fill remainder with silence
    if (toRead < bytes) {
        std::memset(data + toRead, 0, bytes - toRead);
    }
    
    m_spaceAvailable.wakeOne();
    return bytes;
}

size_t AudioBuffer::available() const
{
    if (m_writePos >= m_readPos) {
        return m_writePos - m_readPos;
    }
    return m_buffer.size() - m_readPos + m_writePos;
}

size_t AudioBuffer::freeSpace() const
{
    return m_buffer.size() - available() - 1;
}

void AudioBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_readPos = 0;
    m_writePos = 0;
    m_spaceAvailable.wakeAll();
}

void AudioBuffer::abort()
{
    QMutexLocker lock(&m_mutex);
    m_aborted = true;
    m_spaceAvailable.wakeAll();
}

void AudioBuffer::reset()
{
    QMutexLocker lock(&m_mutex);
    m_aborted = false;
}

// ============================================================================
// BufferIODevice implementation
// ============================================================================

BufferIODevice::BufferIODevice(AudioBuffer* buffer, QObject* parent)
    : QIODevice(parent)
    , m_buffer(buffer)
{
    open(QIODevice::ReadOnly);
}

qint64 BufferIODevice::readData(char* data, qint64 maxSize)
{
    return static_cast<qint64>(m_buffer->read(data, static_cast<size_t>(maxSize)));
}

qint64 BufferIODevice::writeData(const char*, qint64)
{
    return -1;
}

qint64 BufferIODevice::bytesAvailable() const
{
    return static_cast<qint64>(m_buffer->available()) + QIODevice::bytesAvailable();
}

// ============================================================================
// DecoderWorker implementation
// ============================================================================

DecoderWorker::DecoderWorker(PlaybackEngine* engine)
    : QObject(nullptr)
    , m_engine(engine)
{
}

void DecoderWorker::startDecoding(const TrackRef& track, bool isNext, qint64 seekPositionMs)
{
    m_stopRequested = false;
    m_abortRequested = false;
    
    qint64 samplesDecoded = 0;
    bool success = decodeTrack(track, isNext, seekPositionMs, samplesDecoded);
    
    if (success && !m_abortRequested) {
        emit decodeFinished(track, samplesDecoded);
    }
}

void DecoderWorker::stopDecoding()
{
    m_stopRequested = true;
    m_abortRequested = true;
}

void DecoderWorker::abortDecoding()
{
    m_abortRequested = true;
}

bool DecoderWorker::decodeTrack(const TrackRef& track, bool isNext, qint64 seekPositionMs, qint64& samplesDecoded)
{
    samplesDecoded = 0;
    Q_UNUSED(isNext)
    
    // Validate URL
    QUrl validUrl = track.url;
    if (validUrl.scheme().isEmpty() && !validUrl.path().isEmpty()) {
        validUrl = QUrl::fromLocalFile(validUrl.path());
    }
    
    QString filePath = validUrl.toLocalFile();
    if (filePath.isEmpty()) {
        emit decodeError(QStringLiteral("Invalid or empty file path: ") + track.url.toString());
        return false;
    }
    
    // Open input file
    AVFormatContext* formatCtx = nullptr;
    if (avformat_open_input(&formatCtx, filePath.toUtf8().constData(), nullptr, nullptr) < 0) {
        emit decodeError(QStringLiteral("Failed to open file: ") + filePath);
        return false;
    }
    
    // Find stream info
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        avformat_close_input(&formatCtx);
        emit decodeError(QStringLiteral("Failed to find stream info"));
        return false;
    }
    
    // Find audio stream
    int audioStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }
    
    if (audioStreamIndex == -1) {
        avformat_close_input(&formatCtx);
        emit decodeError(QStringLiteral("No audio stream found"));
        return false;
    }
    
    AVCodecParameters* codecPar = formatCtx->streams[audioStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecPar);
    
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        emit decodeError(QStringLiteral("Failed to open codec"));
        return false;
    }
    
    // Seek if requested
    AVStream* stream = formatCtx->streams[audioStreamIndex];
    if (seekPositionMs > 0) {
        int64_t seekTarget = av_rescale_q(seekPositionMs, {1, 1000}, stream->time_base);
        if (av_seek_frame(formatCtx, audioStreamIndex, seekTarget, AVSEEK_FLAG_BACKWARD) >= 0) {
            avcodec_flush_buffers(codecCtx);
        }
    }
    
    // Get track format info
    AudioFormat trackFormat;
    trackFormat.sampleRate = codecCtx->sample_rate;
    trackFormat.channels = codecCtx->ch_layout.nb_channels;
    
    // Determine bits per sample from format
    switch (codecCtx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            trackFormat.bitsPerSample = 8;
            break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            trackFormat.bitsPerSample = 16;
            break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            trackFormat.bitsPerSample = 32;
            break;
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            trackFormat.bitsPerSample = 64;
            break;
        default:
            trackFormat.bitsPerSample = 16;
    }
    
    // Calculate duration (reuse stream variable from seek section)
    qint64 durationMs = 0;
    if (stream->duration != AV_NOPTS_VALUE) {
        durationMs = av_rescale_q(stream->duration, stream->time_base, {1, 1000});
    } else if (formatCtx->duration != AV_NOPTS_VALUE) {
        durationMs = formatCtx->duration / 1000;
    }
    
    // Emit decode started
    emit decodeStarted(track, durationMs, trackFormat);
    
    // Determine output format based on mode
    int outSampleRate = m_engine->m_sessionFormat.sampleRate();
    int outChannels = m_engine->m_sessionFormat.channelCount();
    AVSampleFormat outSampleFmt = AV_SAMPLE_FMT_S16;
    
    if (m_engine->m_sessionFormat.sampleFormat() == QAudioFormat::Int32) {
        outSampleFmt = AV_SAMPLE_FMT_S32;
    } else if (m_engine->m_sessionFormat.sampleFormat() == QAudioFormat::Float) {
        outSampleFmt = AV_SAMPLE_FMT_FLT;
    }
    
    // In BitPerfect mode, use native format only if track matches session format
    bool bitPerfect = (m_engine->m_mode == PlaybackMode::BitPerfectSameRate);
    if (bitPerfect) {
        const int sessionRate   = m_engine->m_sessionFormat.sampleRate();
        const int sessionChans  = m_engine->m_sessionFormat.channelCount();
        const int trackRate     = codecCtx->sample_rate;
        const int trackChannels = codecCtx->ch_layout.nb_channels;

        // Only use track format if it matches session format exactly
        if (trackRate == sessionRate && trackChannels == sessionChans) {
            outSampleRate = trackRate;
            outChannels   = trackChannels;
            // No resampling needed, just format conversion
        } else {
            bitPerfect = false;  // Fall back to session format/resampling
        }
    }
    
    // Setup resampler
    SwrContext* swrCtx = nullptr;
    AVChannelLayout outLayout;
    AVChannelLayout inLayout;
    
    av_channel_layout_default(&outLayout, outChannels);
    if (codecCtx->ch_layout.nb_channels > 0) {
        av_channel_layout_copy(&inLayout, &codecCtx->ch_layout);
    } else {
        av_channel_layout_default(&inLayout, 2);
    }
    
    swr_alloc_set_opts2(&swrCtx,
                        &outLayout, outSampleFmt, outSampleRate,
                        &inLayout, codecCtx->sample_fmt, codecCtx->sample_rate,
                        0, nullptr);
    
    if (!swrCtx || swr_init(swrCtx) < 0) {
        if (swrCtx) swr_free(&swrCtx);
        av_channel_layout_uninit(&inLayout);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        emit decodeError(QStringLiteral("Failed to initialize resampler"));
        return false;
    }
    
    // Allocate packet and frame
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    // Calculate bytes per sample for output
    int bytesPerSample = 2;  // Int16
    if (outSampleFmt == AV_SAMPLE_FMT_S32 || outSampleFmt == AV_SAMPLE_FMT_FLT) {
        bytesPerSample = 4;
    }
    int bytesPerFrame = outChannels * bytesPerSample;
    
    // Decode loop
    std::vector<uint8_t> resampleBuffer(8192 * bytesPerFrame);
    float volume = (m_engine->m_mode == PlaybackMode::BitPerfectSameRate) ? 1.0f : m_engine->m_volume;
    
    while (!m_stopRequested && !m_abortRequested) {
        int ret = av_read_frame(formatCtx, packet);
        if (ret < 0) {
            break;  // EOF or error
        }
        
        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }
        
        ret = avcodec_send_packet(codecCtx, packet);
        av_packet_unref(packet);
        
        if (ret < 0) continue;
        
        while (!m_stopRequested && !m_abortRequested) {
            ret = avcodec_receive_frame(codecCtx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;
            
            // Resample frame
            int outSamples = swr_get_out_samples(swrCtx, frame->nb_samples);
            if (outSamples * bytesPerFrame > static_cast<int>(resampleBuffer.size())) {
                resampleBuffer.resize(outSamples * bytesPerFrame);
            }
            
            uint8_t* outPtr = resampleBuffer.data();
            int converted = swr_convert(swrCtx, &outPtr, outSamples,
                                        const_cast<const uint8_t**>(frame->extended_data),
                                        frame->nb_samples);
            
            if (converted > 0) {
                size_t bytes = converted * bytesPerFrame;
                
                // Apply volume (only in GaplessSession mode)
                if (volume < 0.999f) {
                    if (outSampleFmt == AV_SAMPLE_FMT_S16) {
                        int16_t* samples = reinterpret_cast<int16_t*>(resampleBuffer.data());
                        for (int i = 0; i < converted * outChannels; i++) {
                            samples[i] = static_cast<int16_t>(samples[i] * volume);
                        }
                    } else if (outSampleFmt == AV_SAMPLE_FMT_FLT) {
                        float* samples = reinterpret_cast<float*>(resampleBuffer.data());
                        for (int i = 0; i < converted * outChannels; i++) {
                            samples[i] *= volume;
                        }
                    }
                }
                
                // Write to buffer
                m_engine->m_buffer.write(reinterpret_cast<const char*>(resampleBuffer.data()), bytes);
                samplesDecoded += converted;
                m_engine->m_samplesWritten += converted;
            }
            
            av_frame_unref(frame);
        }
    }
    
    // Flush resampler
    if (!m_stopRequested && !m_abortRequested) {
        int outSamples = swr_get_out_samples(swrCtx, 0);
        if (outSamples > 0) {
            if (outSamples * bytesPerFrame > static_cast<int>(resampleBuffer.size())) {
                resampleBuffer.resize(outSamples * bytesPerFrame);
            }
            
            uint8_t* outPtr = resampleBuffer.data();
            int converted = swr_convert(swrCtx, &outPtr, outSamples, nullptr, 0);
            
            if (converted > 0) {
                size_t bytes = converted * bytesPerFrame;
                
                if (volume < 0.999f) {
                    if (outSampleFmt == AV_SAMPLE_FMT_S16) {
                        int16_t* samples = reinterpret_cast<int16_t*>(resampleBuffer.data());
                        for (int i = 0; i < converted * outChannels; i++) {
                            samples[i] = static_cast<int16_t>(samples[i] * volume);
                        }
                    }
                }
                
                m_engine->m_buffer.write(reinterpret_cast<const char*>(resampleBuffer.data()), bytes);
                samplesDecoded += converted;
                m_engine->m_samplesWritten += converted;
            }
        }
    }
    
    // Cleanup
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swrCtx);
    av_channel_layout_uninit(&inLayout);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    
    return !m_abortRequested;
}

// ============================================================================
// PlaybackEngine implementation
// ============================================================================

PlaybackEngine::PlaybackEngine(QObject* parent)
    : QObject(parent)
    , m_buffer(2 * 1024 * 1024)  // 2MB buffer
{
    qRegisterMetaType<TrackRef>("TrackRef");
    qRegisterMetaType<PlaybackMode>("PlaybackMode");
    qRegisterMetaType<AudioFormat>("AudioFormat");
    qRegisterMetaType<PlaybackEngine::State>("PlaybackEngine::State");
    
    m_outputDevice = QMediaDevices::defaultAudioOutput();
    
    // Position update timer
    m_positionTimer = new QTimer(this);
    connect(m_positionTimer, &QTimer::timeout, this, &PlaybackEngine::updatePosition);
    
    m_bufferDevice = new BufferIODevice(&m_buffer, this);
}

PlaybackEngine::~PlaybackEngine()
{
    stop();
}

void PlaybackEngine::setMode(PlaybackMode mode)
{
    if (m_mode == mode) return;
    
    // Changing mode while playing stops playback
    if (m_state != State::Stopped) {
        stop();
    }
    
    m_mode = mode;
    emit modeChanged(mode);
}

void PlaybackEngine::setBufferProfile(BufferProfile profile)
{
    if (m_bufferProfile == profile) return;
    
    m_bufferProfile = profile;
    
    // If we have an audio sink, recreate it with new buffer size
    if (m_audioSink) {
        bool wasPlaying = (m_state == State::Playing);
        TrackRef currentTrack = m_currentTrack;
        qint64 currentPos = position();
        
        stop();
        setupAudioOutput();
        
        if (wasPlaying && currentTrack.isValid()) {
            play(currentTrack);
            seek(currentPos);  // Restore position
        }
    }
}

void PlaybackEngine::setOutputDevice(const QAudioDevice& device)
{
    if (m_outputDevice == device) return;
    
    // Changing device while playing stops playback
    if (m_state != State::Stopped) {
        stop();
    }
    
    m_outputDevice = device;
}

void PlaybackEngine::setVolume(float v)
{
    m_volume = qBound(0.0f, v, 1.0f);
    // Note: In BitPerfectSameRate mode, volume is effectively ignored (fixed at 1.0)
}

QAudioFormat PlaybackEngine::determineSessionFormat()
{
    QAudioFormat format;
    
    // Use device preferred format as base
    format = m_outputDevice.preferredFormat();
    
    // Ensure we have reasonable defaults
    if (format.sampleRate() <= 0) {
        format.setSampleRate(48000);
    }
    if (format.channelCount() <= 0) {
        format.setChannelCount(2);
    }
    
    // Use Int16 for broad compatibility
    format.setSampleFormat(QAudioFormat::Int16);
    
    return format;
}

void PlaybackEngine::setupAudioOutput()
{
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }
    
    m_sessionFormat = determineSessionFormat();
    
    qDebug() << "Setting up audio output:"
             << "rate=" << m_sessionFormat.sampleRate()
             << "channels=" << m_sessionFormat.channelCount()
             << "format=" << m_sessionFormat.sampleFormat();
    
    m_audioSink = new QAudioSink(m_outputDevice, m_sessionFormat, this);
    connect(m_audioSink, &QAudioSink::stateChanged, this, &PlaybackEngine::onAudioStateChanged);
    
    // Set buffer size based on profile
    int bufferMs = (m_bufferProfile == BufferProfile::Short) ? 100 : 250;
    const int sampleRate = m_sessionFormat.sampleRate();
    const int bytesPerFr = bytesPerFrame();
    const int frames = sampleRate * bufferMs / 1000;
    const int bufferBytes = frames * bytesPerFr;
    
    m_audioSink->setBufferSize(bufferBytes);
    qDebug() << "Buffer profile:" << (m_bufferProfile == BufferProfile::Short ? "Short" : "Long")
             << "Buffer size:" << bufferBytes << "bytes (" << bufferMs << "ms)";
}

void PlaybackEngine::startDecoderThread()
{
    if (m_decodeThread) {
        stopDecoderThread();
    }
    
    m_decodeThread = new QThread(this);
    m_decoder = new DecoderWorker(this);
    m_decoder->moveToThread(m_decodeThread);
    
    connect(m_decoder, &DecoderWorker::decodeStarted,
            this, &PlaybackEngine::onDecodeStarted, Qt::QueuedConnection);
    connect(m_decoder, &DecoderWorker::decodeFinished,
            this, &PlaybackEngine::onDecodeFinished, Qt::QueuedConnection);
    connect(m_decoder, &DecoderWorker::decodeError,
            this, &PlaybackEngine::onDecodeError, Qt::QueuedConnection);
    
    m_decodeThread->start();
}

void PlaybackEngine::stopDecoderThread()
{
    if (!m_decodeThread) return;
    
    if (m_decoder) {
        m_decoder->stopDecoding();
    }
    
    m_buffer.abort();
    
    m_decodeThread->quit();
    m_decodeThread->wait(1000);
    
    if (m_decodeThread->isRunning()) {
        m_decodeThread->terminate();
        m_decodeThread->wait();
    }
    
    delete m_decoder;
    m_decoder = nullptr;
    
    delete m_decodeThread;
    m_decodeThread = nullptr;
}

void PlaybackEngine::play(const TrackRef& track)
{
    if (!track.isValid()) {
        emit errorOccurred("Invalid track");
        return;
    }
    
    // Stop any current playback
    stop();
    
    // Setup audio output
    setupAudioOutput();
    
    // Clear state
    m_buffer.clear();
    m_buffer.reset();
    m_currentTrack = track;
    m_nextTrack = TrackRef();
    m_hasNextPrepared = false;
    m_nextLocked = false;
    m_currentDurationMs = 0;
    m_currentTrackSamples = 0;
    m_samplesWritten = 0;
    m_trackStartSample = 0;
    m_seekOffsetMs = 0;  // Reset seek offset for new track
    
    // Start decoder
    startDecoderThread();
    QMetaObject::invokeMethod(m_decoder, "startDecoding", Qt::QueuedConnection,
                              Q_ARG(TrackRef, track), Q_ARG(bool, false), Q_ARG(qint64, 0));
    
    // Start audio output
    m_bufferDevice->close();
    m_bufferDevice->open(QIODevice::ReadOnly);
    m_audioSink->start(m_bufferDevice);
    
    // Start position timer
    m_positionTimer->start(POSITION_UPDATE_INTERVAL_MS);
    
    m_state = State::Playing;
    emit stateChanged(m_state);
    emit trackChanged(track);
}

void PlaybackEngine::prepareNext(const TrackRef& track)
{
    // If we're already past the lead-in point, this affects the NEXT transition, not current
    if (m_nextLocked) {
        // Store for after current transition completes
        return;
    }
    
    m_nextTrack = track;
    m_hasNextPrepared = track.isValid();
    
    // In BitPerfectSameRate mode, check format compatibility
    if (m_mode == PlaybackMode::BitPerfectSameRate && m_hasNextPrepared) {
        // We'll check format when we actually start decoding the next track
        // For now, just store it
    }
}

void PlaybackEngine::clearNext()
{
    if (m_nextLocked) return;  // Can't clear during transition
    
    m_nextTrack = TrackRef();
    m_hasNextPrepared = false;
}

void PlaybackEngine::pause()
{
    if (m_state != State::Playing) return;
    
    if (m_audioSink) {
        m_audioSink->suspend();
    }
    m_positionTimer->stop();
    
    m_state = State::Paused;
    emit stateChanged(m_state);
}

void PlaybackEngine::resume()
{
    if (m_state != State::Paused) return;
    
    if (m_audioSink) {
        m_audioSink->resume();
    }
    m_positionTimer->start(POSITION_UPDATE_INTERVAL_MS);
    
    m_state = State::Playing;
    emit stateChanged(m_state);
}

void PlaybackEngine::stop()
{
    m_positionTimer->stop();
    stopDecoderThread();
    
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->reset();  // Drop all buffered audio data
    }
    
    m_buffer.clear();
    m_bufferDevice->close();
    
    m_currentTrack = TrackRef();
    m_nextTrack = TrackRef();
    m_hasNextPrepared = false;
    m_nextLocked = false;
    m_currentDurationMs = 0;
    m_currentTrackSamples = 0;
    m_samplesWritten = 0;
    
    m_state = State::Stopped;
    emit stateChanged(m_state);
}

void PlaybackEngine::seek(qint64 positionMs)
{
    if (m_state == State::Stopped || !m_currentTrack.isValid()) return;
    
    // Clamp position to valid range
    positionMs = qBound(0LL, positionMs, m_currentDurationMs);
    
    // Early-return for tiny position changes to avoid unnecessary seeks
    qint64 currentPos = position();
    if (qAbs(positionMs - currentPos) < 50) {
        return;
    }
    
    // Seeking clears the prepared next track
    m_nextTrack = TrackRef();
    m_hasNextPrepared = false;
    m_nextLocked = false;
    
    // Save current track and duration before stopping
    TrackRef trackToSeek = m_currentTrack;
    qint64 savedDuration = m_currentDurationMs;
    
    // Abort buffer first to unblock any waiting writes
    m_buffer.abort();
    
    // Stop decoder thread (this will wait for it to finish)
    stopDecoderThread();
    
    // Stop and reset audio sink to clear all pending audio data
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink->reset();  // Drop all buffered audio data
    }
    
    // Clear and reset buffer
    m_buffer.clear();
    m_buffer.reset();
    
    // Reset sample tracking - use seek offset for position calculation
    m_samplesWritten = 0;
    m_trackStartSample = 0;
    m_currentTrackSamples = 0;
    m_seekOffsetMs = positionMs;
    m_currentDurationMs = savedDuration;
    
    // Restart audio output
    m_bufferDevice->close();
    m_bufferDevice->open(QIODevice::ReadOnly);
    m_audioSink->start(m_bufferDevice);
    
    // Restart decoding from seek position
    startDecoderThread();
    QMetaObject::invokeMethod(m_decoder, "startDecoding", Qt::QueuedConnection,
                              Q_ARG(TrackRef, trackToSeek), Q_ARG(bool, false), Q_ARG(qint64, positionMs));
    
    emit positionChanged(positionMs);
}

qint64 PlaybackEngine::position() const
{
    if (m_state == State::Stopped || !m_audioSink) return 0;
    
    qint64 processedUs = m_audioSink->processedUSecs();
    qint64 processedSamples = processedUs * m_sessionFormat.sampleRate() / 1000000;
    
    // Subtract track start offset and add seek offset
    qint64 trackSamples = processedSamples - m_trackStartSample;
    qint64 posMs = samplesToMs(trackSamples) + m_seekOffsetMs;
    
    return qBound(0LL, posMs, m_currentDurationMs);
}

qint64 PlaybackEngine::samplesToMs(qint64 samples) const
{
    if (m_sessionFormat.sampleRate() <= 0) return 0;
    return samples * 1000 / m_sessionFormat.sampleRate();
}

qint64 PlaybackEngine::msToSamples(qint64 ms) const
{
    return ms * m_sessionFormat.sampleRate() / 1000;
}

int PlaybackEngine::bytesPerFrame() const
{
    int bytesPerSample = 2;  // Int16
    if (m_sessionFormat.sampleFormat() == QAudioFormat::Int32 ||
        m_sessionFormat.sampleFormat() == QAudioFormat::Float) {
        bytesPerSample = 4;
    }
    return m_sessionFormat.channelCount() * bytesPerSample;
}

void PlaybackEngine::updatePosition()
{
    if (m_state != State::Playing) return;
    
    // Snapshot current track before checking transitions
    TrackRef trackBefore = m_currentTrack;
    
    qint64 pos = position();
    emit positionChanged(pos);
    
    checkTrackTransition();
    
    // If a gapless transition just occurred, don't run the trackFinished logic
    // on this tick - the position and duration have changed to the new track
    if (m_currentTrack != trackBefore) {
        return;
    }
    
    // Handle seek-to-end case: if we're at or past duration and no gapless transition will occur,
    // emit trackFinished so PlayerController can advance to next track.
    // Critical: Must stop() before emitting trackFinished() because PlayerController ignores
    // the signal if the engine is not in Stopped state.
    if (m_currentDurationMs > 0 && pos >= m_currentDurationMs) {
        if (!m_nextLocked || !m_nextTrack.isValid()) {
            stop();
            emit trackFinished();
        }
    }
}

void PlaybackEngine::checkTrackTransition()
{
    if (m_state != State::Playing || m_currentDurationMs <= 0) return;
    
    qint64 pos = position();
    qint64 remaining = m_currentDurationMs - pos;
    
    // Check if we've crossed into the next track (position >= duration)
    if (remaining <= 0 && m_nextLocked && m_nextTrack.isValid()) {
        // We've transitioned to the next track based on audio position
        performTrackTransition();
        return;
    }
    
    // Check if we're in the lead-in window - start decoding next track
    if (remaining <= GAPLESS_LEAD_IN_MS && remaining > 0 && !m_nextLocked) {
        emit trackAboutToFinish(remaining);
        
        // Lock in the next track for this transition
        if (m_hasNextPrepared && m_nextTrack.isValid()) {
            m_nextLocked = true;
            
            // Start decoding next track (from beginning)
            QMetaObject::invokeMethod(m_decoder, "startDecoding", Qt::QueuedConnection,
                                      Q_ARG(TrackRef, m_nextTrack), Q_ARG(bool, true), Q_ARG(qint64, 0));
        }
    }
}

void PlaybackEngine::performTrackTransition()
{
    // Transition to next track
    m_currentTrack = m_nextTrack;
    m_nextTrack = TrackRef();
    m_hasNextPrepared = false;
    m_nextLocked = false;
    
    // CRITICAL: Set track start sample to the ACTUAL current processed samples,
    // not an accumulated estimate. This ensures position() returns ~0 for the new track.
    qint64 processedUs = m_audioSink ? m_audioSink->processedUSecs() : 0;
    m_trackStartSample = processedUs * m_sessionFormat.sampleRate() / 1000000;
    m_seekOffsetMs = 0;  // Reset seek offset for new track
    
    // Update duration and format from stored next track info
    m_currentDurationMs = m_nextTrackDurationMs;
    m_currentTrackFormat = m_nextTrackFormat;
    m_currentTrackSamples = msToSamples(m_nextTrackDurationMs);  // Estimate until decode finishes
    
    // Emit signals - trackChanged will trigger PlayerController to update queue and arm next
    emit durationChanged(m_currentDurationMs);
    emit trackChanged(m_currentTrack);
}

void PlaybackEngine::onDecodeStarted(const TrackRef& track, qint64 durationMs, const AudioFormat& format)
{
    if (track == m_currentTrack) {
        m_currentDurationMs = durationMs;
        m_currentTrackFormat = format;
        emit durationChanged(durationMs);
    } else if (track == m_nextTrack && m_nextLocked) {
        // Store next track's duration/format for when we transition
        m_nextTrackDurationMs = durationMs;
        m_nextTrackFormat = format;
    }
}

void PlaybackEngine::onDecodeFinished(const TrackRef& track, qint64 samplesDecoded)
{
    // Update sample count for the track that finished decoding
    if (track == m_currentTrack) {
        m_currentTrackSamples = samplesDecoded;
    }
    // Note: Track transitions are now handled by performTrackTransition() 
    // which is called from checkTrackTransition() when audio position crosses the boundary
}

void PlaybackEngine::onDecodeError(const QString& message)
{
    qWarning() << "Decode error:" << message;
    emit errorOccurred(message);
}

void PlaybackEngine::onAudioStateChanged(QAudio::State state)
{
    if (state == QAudio::IdleState && m_state == State::Playing) {
        // Buffer underrun or end of playback
        
        // Check if we've actually finished all audio
        if (m_currentTrackSamples > 0) {
            qint64 processedUs = m_audioSink ? m_audioSink->processedUSecs() : 0;
            qint64 processedSamples = processedUs * m_sessionFormat.sampleRate() / 1000000;
            
            if (processedSamples >= m_trackStartSample + m_currentTrackSamples) {
                // Actually finished
                if (!m_hasNextPrepared) {
                    stop();
                    emit trackFinished();
                }
            }
        }
    }
}
