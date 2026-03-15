#include "AudioOutputWorker.h"
#include "SPSCRingBuffer.h"
#include "BufferIODevice.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAudioWorker, "musicplayer.audio.worker")

AudioOutputWorker::AudioOutputWorker(SPSCRingBuffer *ringBuffer, qreal initialVolume, QObject *parent)
    : QObject(parent)
    , m_ringBuffer(ringBuffer)
    , m_volume(qBound<qreal>(0.0, initialVolume, 1.0))
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
    
    // Use software volume control in BufferIODevice, keep sink at 1.0
    m_audioSink->setVolume(1.0);
    
    connect(m_audioSink.get(), &QAudioSink::stateChanged,
            this, &AudioOutputWorker::onAudioStateChanged);
    
    m_bufferDevice = std::make_unique<BufferIODevice>(m_ringBuffer, static_cast<float>(m_volume));
    setOutputArmed(false);
    
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
    
    setOutputArmed(false);
    m_audioSink->start(m_bufferDevice.get());
    m_running = true;
    
    qCDebug(lcAudioWorker) << "start() - audio sink started (output gated)";
}

void AudioOutputWorker::suspend()
{
    if (m_audioSink && m_running) {
        setOutputArmed(false);
        m_audioSink->suspend();
        m_positionTimer->stop();
        qCDebug(lcAudioWorker) << "suspend()";
    }
}

void AudioOutputWorker::resume()
{
    if (m_audioSink && m_running) {
        setOutputArmed(true);
        if (m_audioSink->state() == QAudio::SuspendedState) {
            m_audioSink->resume();
        }
        m_positionTimer->start();
        
        // Aggressively force QAudioSink to 1.0 to combat PipeWire's stream-restore
        // which may apply stored volume at any point after stream activation
        m_audioSink->setVolume(1.0);
        
        // Start a temporary timer that forces volume to 1.0 repeatedly for 15 seconds
        // to override any delayed PipeWire restoration
        QTimer *forceVolumeTimer = new QTimer(this);
        forceVolumeTimer->setInterval(500);  // Every 500ms
        int *counter = new int(0);
        connect(forceVolumeTimer, &QTimer::timeout, this, [this, forceVolumeTimer, counter]() {
            if (m_audioSink) {
                qreal currentVol = m_audioSink->volume();
                if (currentVol < 0.99) {
                    qCDebug(lcAudioWorker) << "PipeWire changed sink volume to" << currentVol << "- forcing back to 1.0";
                }
                m_audioSink->setVolume(1.0);
            }
            (*counter)++;
            if (*counter >= 30) {  // 30 * 500ms = 15 seconds
                forceVolumeTimer->stop();
                forceVolumeTimer->deleteLater();
                delete counter;
            }
        });
        forceVolumeTimer->start();
        
        qCDebug(lcAudioWorker) << "resume() - started 15s volume enforcement timer";
    }
}

void AudioOutputWorker::setVolume(qreal volume)
{
    m_volume = qBound<qreal>(0.0, volume, 1.0);
    qCDebug(lcAudioWorker) << "setVolume:" << m_volume;
    // Software volume control via BufferIODevice sample scaling
    // QAudioSink is kept at 1.0 to avoid PipeWire volume stacking
    if (m_bufferDevice) {
        m_bufferDevice->setVolume(static_cast<float>(m_volume));
    }
}

void AudioOutputWorker::stop()
{
    m_running = false;
    m_positionTimer->stop();
    setOutputArmed(false);
    
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

void AudioOutputWorker::setOutputArmed(bool armed)
{
    if (m_bufferDevice) {
        m_bufferDevice->setArmed(armed);
    }
}

void AudioOutputWorker::pushPosition()
{
    if (m_audioSink && m_running) {
        qint64 processedUs = m_audioSink->processedUSecs();
        emit positionUpdated(processedUs);
    }
}
