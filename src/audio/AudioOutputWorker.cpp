#include "AudioOutputWorker.h"
#include "SPSCRingBuffer.h"
#include "BufferIODevice.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAudioWorker, "musicplayer.audio.worker")

AudioOutputWorker::AudioOutputWorker(SPSCRingBuffer *ringBuffer, QObject *parent)
    : QObject(parent)
    , m_ringBuffer(ringBuffer)
{
    // Position timer runs in this thread's event loop
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(50);  // 50ms updates, pushed to GUI thread
    connect(m_positionTimer, &QTimer::timeout, this, &AudioOutputWorker::pushPosition);
}

AudioOutputWorker::~AudioOutputWorker()
{
    stop();
}

void AudioOutputWorker::initialize(int sampleRate, int channels, int bufferMs)
{
    m_bufferMs = bufferMs;
    
    m_audioFormat.setSampleRate(sampleRate);
    m_audioFormat.setChannelCount(channels);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16);
    
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    if (!defaultDevice.isFormatSupported(m_audioFormat)) {
        qCCritical(lcAudioWorker) << "Audio format not supported:" 
                                   << sampleRate << "Hz," << channels << "ch, S16";
        emit initialized(false);
        emit errorOccurred(QStringLiteral("Audio format not supported"));
        return;
    }
    
    m_audioSink = std::make_unique<QAudioSink>(defaultDevice, m_audioFormat);
    int bufferBytes = (sampleRate * bufferMs / 1000) * m_audioFormat.bytesPerFrame();
    m_audioSink->setBufferSize(bufferBytes);
    
    connect(m_audioSink.get(), &QAudioSink::stateChanged,
            this, &AudioOutputWorker::onAudioStateChanged);
    
    m_bufferDevice = std::make_unique<BufferIODevice>(m_ringBuffer);
    
    qCDebug(lcAudioWorker) << "initialize()" << "rate=" << sampleRate
                           << "ch=" << channels << "bufferMs=" << bufferMs;
    
    emit initialized(true);
}

void AudioOutputWorker::start()
{
    if (!m_audioSink || !m_bufferDevice) {
        qCWarning(lcAudioWorker) << "start() called but not initialized";
        return;
    }
    
    m_audioSink->start(m_bufferDevice.get());
    m_audioSink->suspend();  // Start suspended, resume() will activate
    m_running = true;
    
    qCDebug(lcAudioWorker) << "start() - audio sink started (suspended)";
}

void AudioOutputWorker::suspend()
{
    if (m_audioSink && m_running) {
        m_audioSink->suspend();
        m_positionTimer->stop();
        qCDebug(lcAudioWorker) << "suspend()";
    }
}

void AudioOutputWorker::resume()
{
    if (m_audioSink && m_running) {
        m_audioSink->resume();
        m_positionTimer->start();
        qCDebug(lcAudioWorker) << "resume()";
    }
}

void AudioOutputWorker::stop()
{
    m_running = false;
    m_positionTimer->stop();
    
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    m_bufferDevice.reset();
    
    qCDebug(lcAudioWorker) << "stop()";
}

void AudioOutputWorker::onAudioStateChanged(QAudio::State state)
{
    qCDebug(lcAudioWorker) << "QAudioSink stateChanged:" << state;
    emit stateChanged(state);
}

void AudioOutputWorker::pushPosition()
{
    if (m_audioSink && m_running) {
        qint64 processedUs = m_audioSink->processedUSecs();
        emit positionUpdated(processedUs);
    }
}
