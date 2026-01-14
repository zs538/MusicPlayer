#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QTimer>
#include <QRect>

class PlaylistStore;
class Settings;

/**
 * @brief Manages session persistence - playlists, UI state, window geometry.
 * 
 * Session data is stored as JSON for reliability and human-readability.
 * Uses atomic writes (temp file + rename) to prevent corruption.
 * Auto-saves with debouncing to avoid excessive disk writes.
 */
class SessionManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    
    // UI state properties exposed to QML
    Q_PROPERTY(int currentPanel READ currentPanel WRITE setCurrentPanel NOTIFY currentPanelChanged)
    Q_PROPERTY(QString fileBrowserPath READ fileBrowserPath WRITE setFileBrowserPath NOTIFY fileBrowserPathChanged)
    Q_PROPERTY(QVariantList playlistColumns READ playlistColumns WRITE setPlaylistColumns NOTIFY playlistColumnsChanged)
    Q_PROPERTY(QStringList libraryGroupingLevels READ libraryGroupingLevels WRITE setLibraryGroupingLevels NOTIFY libraryGroupingLevelsChanged)
    Q_PROPERTY(QVariantList floatingWindows READ floatingWindows WRITE setFloatingWindows NOTIFY floatingWindowsChanged)

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;
    
    static SessionManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static SessionManager *instance();
    
    // Initialize with dependencies (call after all managers are created)
    void initialize(PlaylistStore *playlistStore);
    
    // Session operations
    Q_INVOKABLE bool saveSession();
    Q_INVOKABLE bool loadSession();
    Q_INVOKABLE void scheduleAutoSave();
    
    // Window geometry
    Q_INVOKABLE QRect windowGeometry() const;
    Q_INVOKABLE void setWindowGeometry(const QRect &rect);
    Q_INVOKABLE void setWindowGeometry(int x, int y, int width, int height);
    
    // UI state
    int currentPanel() const;
    void setCurrentPanel(int panel);
    
    QString fileBrowserPath() const;
    void setFileBrowserPath(const QString &path);
    
    QVariantList playlistColumns() const;
    void setPlaylistColumns(const QVariantList &columns);
    
    QStringList libraryGroupingLevels() const;
    void setLibraryGroupingLevels(const QStringList &levels);
    
    QVariantList floatingWindows() const;
    void setFloatingWindows(const QVariantList &windows);
    
    // Playback state (for optional restore)
    Q_INVOKABLE int lastPlaylistIndex() const;
    Q_INVOKABLE qint64 lastPosition() const;
    Q_INVOKABLE void setPlaybackState(int playlistIndex, qint64 positionMs);

signals:
    void currentPanelChanged();
    void fileBrowserPathChanged();
    void playlistColumnsChanged();
    void libraryGroupingLevelsChanged();
    void floatingWindowsChanged();
    void sessionLoaded();
    void sessionSaved();

private:
    QString sessionFilePath() const;
    QJsonObject buildSessionJson() const;
    bool parseSessionJson(const QJsonObject &json);
    bool writeJsonAtomic(const QString &path, const QJsonObject &json);
    
    // Playlist serialization
    QJsonArray serializePlaylists() const;
    bool deserializePlaylists(const QJsonArray &arr);
    
    static SessionManager *s_instance;
    
    PlaylistStore *m_playlistStore = nullptr;
    QTimer m_autoSaveTimer;
    
    // Cached UI state
    QRect m_windowGeometry;
    int m_currentPanel = 0;
    QString m_fileBrowserPath;
    QVariantList m_playlistColumns;
    QStringList m_libraryGroupingLevels;
    QVariantList m_floatingWindows;
    
    // Playback state
    int m_lastPlaylistIndex = -1;
    qint64 m_lastPosition = 0;
    
    bool m_initialized = false;
    bool m_dirty = false;
};

#endif // SESSIONMANAGER_H
