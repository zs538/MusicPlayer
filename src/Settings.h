#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <QString>
#include <QStringList>

class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    
    Q_PROPERTY(int playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY outputDeviceChanged)
    Q_PROPERTY(int bufferSizeMs READ bufferSizeMs WRITE setBufferSizeMs NOTIFY bufferSizeMsChanged)
    Q_PROPERTY(bool restoreSession READ restoreSession WRITE setRestoreSession NOTIFY restoreSessionChanged)
    Q_PROPERTY(QStringList coverArtPatterns READ coverArtPatterns WRITE setCoverArtPatterns NOTIFY coverArtPatternsChanged)
    Q_PROPERTY(int browseTargetPolicy READ browseTargetPolicy WRITE setBrowseTargetPolicy NOTIFY browseTargetPolicyChanged)
    Q_PROPERTY(int browseAutoplayPolicy READ browseAutoplayPolicy WRITE setBrowseAutoplayPolicy NOTIFY browseAutoplayPolicyChanged)

public:
    enum PlaybackMode {
        GaplessSession = 0,
        BitPerfectSameRate
    };
    Q_ENUM(PlaybackMode)
    
    enum BrowseTargetPolicy {
        AppendToViewed = 0,
        ReplaceGeneratedPreferViewed,
        NewPlaylist
    };
    Q_ENUM(BrowseTargetPolicy)
    
    enum BrowseAutoplayPolicy {
        NeverStart = 0,
        StartIfStopped,
        AlwaysStart
    };
    Q_ENUM(BrowseAutoplayPolicy)
    
    explicit Settings(QObject *parent = nullptr);
    
    static Settings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static Settings *instance();
    
    int playbackMode() const;
    void setPlaybackMode(int mode);
    
    double volume() const;
    void setVolume(double value);
    
    QString outputDevice() const;
    void setOutputDevice(const QString &device);
    
    int bufferSizeMs() const;
    void setBufferSizeMs(int ms);
    
    bool restoreSession() const;
    void setRestoreSession(bool restore);
    
    QStringList coverArtPatterns() const;
    void setCoverArtPatterns(const QStringList &patterns);
    
    int browseTargetPolicy() const;
    void setBrowseTargetPolicy(int policy);
    
    int browseAutoplayPolicy() const;
    void setBrowseAutoplayPolicy(int policy);
    
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

    // Per-group-type collection browsing settings
    Q_INVOKABLE QString groupTypeNextGroupBy(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeNextGroupBy(const QString &groupType, const QString &nextGroupBy);
    Q_INVOKABLE QString groupTypeOpenAction(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeOpenAction(const QString &groupType, const QString &openAction);
    Q_INVOKABLE QString groupTypeViewMode(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeViewMode(const QString &groupType, const QString &viewMode);

signals:
    void playbackModeChanged();
    void volumeChanged();
    void outputDeviceChanged();
    void bufferSizeMsChanged();
    void restoreSessionChanged();
    void coverArtPatternsChanged();
    void browseTargetPolicyChanged();
    void browseAutoplayPolicyChanged();
    void settingsChanged();

private:
    QSettings m_settings;
    
    int m_playbackMode = GaplessSession;
    double m_volume = 1.0;
    QString m_outputDevice;
    int m_bufferSizeMs = 100;
    bool m_restoreSession = true;
    QStringList m_coverArtPatterns;
    int m_browseTargetPolicy = AppendToViewed;
    int m_browseAutoplayPolicy = StartIfStopped;
};

#endif // SETTINGS_H
