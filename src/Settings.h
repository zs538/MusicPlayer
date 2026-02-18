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
    
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY outputDeviceChanged)
    Q_PROPERTY(int bufferSizeMs READ bufferSizeMs WRITE setBufferSizeMs NOTIFY bufferSizeMsChanged)
    Q_PROPERTY(bool restoreSession READ restoreSession WRITE setRestoreSession NOTIFY restoreSessionChanged)
    Q_PROPERTY(QStringList coverArtPatterns READ coverArtPatterns WRITE setCoverArtPatterns NOTIFY coverArtPatternsChanged)
    Q_PROPERTY(int addTracksPolicy READ addTracksPolicy WRITE setAddTracksPolicy NOTIFY addTracksPolicyChanged)
    Q_PROPERTY(int previousButtonAction READ previousButtonAction WRITE setPreviousButtonAction NOTIFY previousButtonActionChanged)
    Q_PROPERTY(int openingTracksAction READ openingTracksAction WRITE setOpeningTracksAction NOTIFY openingTracksActionChanged)
    Q_PROPERTY(int generatedPlaylistCount READ generatedPlaylistCount WRITE setGeneratedPlaylistCount NOTIFY generatedPlaylistCountChanged)
    Q_PROPERTY(int gridCellMinWidth READ gridCellMinWidth WRITE setGridCellMinWidth NOTIFY gridCellMinWidthChanged)
    Q_PROPERTY(int gridCellMaxWidth READ gridCellMaxWidth WRITE setGridCellMaxWidth NOTIFY gridCellMaxWidthChanged)
    Q_PROPERTY(bool watcherEnabled READ watcherEnabled WRITE setWatcherEnabled NOTIFY watcherEnabledChanged)
    Q_PROPERTY(int periodicRescanMinutes READ periodicRescanMinutes WRITE setPeriodicRescanMinutes NOTIFY periodicRescanMinutesChanged)

public:
    enum AddTracksPolicy {
        AddNeverStart = 0,
        AddStartIfStopped,
        AddAlwaysStart
    };
    Q_ENUM(AddTracksPolicy)
    
    enum PreviousButtonAction {
        JumpToPrevious = 0,
        RestartThenJump
    };
    Q_ENUM(PreviousButtonAction)
    
    enum OpeningTracksAction {
        OpeningAppendToViewed = 0,
        OpeningCreateNewPlaylist
    };
    Q_ENUM(OpeningTracksAction)
    
    explicit Settings(QObject *parent = nullptr);
    
    static Settings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static Settings *instance();
    
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
    
    
    int addTracksPolicy() const;
    void setAddTracksPolicy(int policy);
    
    int previousButtonAction() const;
    void setPreviousButtonAction(int action);
    
    int openingTracksAction() const;
    void setOpeningTracksAction(int action);
    
    int generatedPlaylistCount() const;
    void setGeneratedPlaylistCount(int count);
    
    int gridCellMinWidth() const;
    void setGridCellMinWidth(int width);
    
    int gridCellMaxWidth() const;
    void setGridCellMaxWidth(int width);
    
    bool watcherEnabled() const;
    void setWatcherEnabled(bool enabled);
    
    int periodicRescanMinutes() const;
    void setPeriodicRescanMinutes(int minutes);
    
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

    // Per-group-type collection browsing settings
    Q_INVOKABLE QString groupTypeNextGroupBy(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeNextGroupBy(const QString &groupType, const QString &nextGroupBy);
    Q_INVOKABLE QString groupTypeOpenAction(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeOpenAction(const QString &groupType, const QString &openAction);
    Q_INVOKABLE QString groupTypeViewMode(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeViewMode(const QString &groupType, const QString &viewMode);
    Q_INVOKABLE bool groupTypeExploreInWindow(const QString &groupType) const;
    Q_INVOKABLE void setGroupTypeExploreInWindow(const QString &groupType, bool inWindow);

signals:
    void volumeChanged();
    void outputDeviceChanged();
    void bufferSizeMsChanged();
    void restoreSessionChanged();
    void coverArtPatternsChanged();
    void addTracksPolicyChanged();
    void previousButtonActionChanged();
    void openingTracksActionChanged();
    void generatedPlaylistCountChanged();
    void gridCellMinWidthChanged();
    void gridCellMaxWidthChanged();
    void watcherEnabledChanged();
    void periodicRescanMinutesChanged();
    void settingsChanged();

private:
    QSettings m_settings;
    
    double m_volume = 1.0;
    QString m_outputDevice;
    int m_bufferSizeMs = 100;
    bool m_restoreSession = true;
    QStringList m_coverArtPatterns;
    int m_addTracksPolicy = AddNeverStart;
    int m_previousButtonAction = RestartThenJump;
    int m_openingTracksAction = OpeningAppendToViewed;
    int m_generatedPlaylistCount = 5;
    int m_gridCellMinWidth = 100;
    int m_gridCellMaxWidth = 200;
    bool m_watcherEnabled = true;
    int m_periodicRescanMinutes = 10;
};

#endif // SETTINGS_H
