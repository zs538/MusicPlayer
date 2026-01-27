#include "SessionManager.h"
#include "PlaylistStore.h"
#include "TrackListModel.h"
#include "Settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDebug>

SessionManager *SessionManager::s_instance = nullptr;

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    
    // Auto-save timer with 2 second debounce
    m_autoSaveTimer.setSingleShot(true);
    m_autoSaveTimer.setInterval(2000);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, [this]() {
        if (m_dirty) {
            saveSession();
        }
    });
    
    // Default window geometry
    m_windowGeometry = QRect(100, 100, 1000, 700);
}

SessionManager::~SessionManager()
{
    // Final save on destruction
    if (m_dirty && m_initialized) {
        saveSession();
    }
    s_instance = nullptr;
}

SessionManager *SessionManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new SessionManager(qmlEngine);
    }
    return s_instance;
}

SessionManager *SessionManager::instance()
{
    return s_instance;
}

void SessionManager::initialize(PlaylistStore *playlistStore)
{
    m_playlistStore = playlistStore;
    m_initialized = true;
    
    // Connect to playlist changes for auto-save
    if (m_playlistStore) {
        connect(m_playlistStore, &PlaylistStore::activePlaylistChanged, this, &SessionManager::scheduleAutoSave);
        connect(m_playlistStore, &PlaylistStore::displayedPlaylistChanged, this, &SessionManager::scheduleAutoSave);
        connect(m_playlistStore, &PlaylistStore::playlistChanged, this, &SessionManager::scheduleAutoSave);
        connect(m_playlistStore, &PlaylistStore::tabInserted, this, &SessionManager::scheduleAutoSave);
        connect(m_playlistStore, &PlaylistStore::tabRemoved, this, &SessionManager::scheduleAutoSave);
        connect(m_playlistStore, &PlaylistStore::tabDataChanged, this, &SessionManager::scheduleAutoSave);
    }
}

QString SessionManager::sessionFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/session.json";
}

bool SessionManager::saveSession()
{
    if (!m_initialized) {
        return false;
    }
    
    QJsonObject json = buildSessionJson();
    bool success = writeJsonAtomic(sessionFilePath(), json);
    
    if (success) {
        m_dirty = false;
        emit sessionSaved();
        qDebug() << "Session saved to" << sessionFilePath();
    } else {
        qWarning() << "Failed to save session";
    }
    
    return success;
}

bool SessionManager::loadSession()
{
    QString path = sessionFilePath();
    QFile file(path);
    
    if (!file.exists()) {
        qDebug() << "No session file found at" << path;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open session file:" << file.errorString();
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse session JSON:" << error.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "Session file is not a JSON object";
        return false;
    }
    
    bool success = parseSessionJson(doc.object());
    
    if (success) {
        emit sessionLoaded();
        qDebug() << "Session loaded from" << path;
    }
    
    return success;
}

void SessionManager::scheduleAutoSave()
{
    m_dirty = true;
    m_autoSaveTimer.start();
}

QRect SessionManager::windowGeometry() const
{
    return m_windowGeometry;
}

void SessionManager::setWindowGeometry(const QRect &rect)
{
    if (m_windowGeometry != rect) {
        m_windowGeometry = rect;
        // Note: geometry persistence disabled - no auto-save
    }
}

void SessionManager::setWindowGeometry(int x, int y, int width, int height)
{
    setWindowGeometry(QRect(x, y, width, height));
}

int SessionManager::currentPanel() const
{
    return m_currentPanel;
}

void SessionManager::setCurrentPanel(int panel)
{
    if (m_currentPanel != panel) {
        m_currentPanel = panel;
        emit currentPanelChanged();
        scheduleAutoSave();
    }
}

QString SessionManager::fileBrowserPath() const
{
    return m_fileBrowserPath;
}

void SessionManager::setFileBrowserPath(const QString &path)
{
    if (m_fileBrowserPath != path) {
        m_fileBrowserPath = path;
        emit fileBrowserPathChanged();
        scheduleAutoSave();
    }
}

QVariantList SessionManager::playlistColumns() const
{
    return m_playlistColumns;
}

void SessionManager::setPlaylistColumns(const QVariantList &columns)
{
    if (m_playlistColumns != columns) {
        m_playlistColumns = columns;
        emit playlistColumnsChanged();
        scheduleAutoSave();
    }
}

QStringList SessionManager::libraryGroupingLevels() const
{
    return m_libraryGroupingLevels;
}

void SessionManager::setLibraryGroupingLevels(const QStringList &levels)
{
    if (m_libraryGroupingLevels != levels) {
        m_libraryGroupingLevels = levels;
        emit libraryGroupingLevelsChanged();
        scheduleAutoSave();
    }
}

QVariantList SessionManager::floatingWindows() const
{
    return m_floatingWindows;
}

void SessionManager::setFloatingWindows(const QVariantList &windows)
{
    if (m_floatingWindows != windows) {
        m_floatingWindows = windows;
        emit floatingWindowsChanged();
        scheduleAutoSave();
    }
}

int SessionManager::lastPlaylistIndex() const
{
    return m_lastPlaylistIndex;
}

qint64 SessionManager::lastPosition() const
{
    return m_lastPosition;
}

void SessionManager::setPlaybackState(int playlistIndex, qint64 positionMs)
{
    m_lastPlaylistIndex = playlistIndex;
    m_lastPosition = positionMs;
    scheduleAutoSave();
}

QJsonObject SessionManager::buildSessionJson() const
{
    QJsonObject json;
    
    // Version 2 adds floating windows support
    json["version"] = 2;
    
    // UI state
    QJsonObject uiState;
    uiState["currentPanel"] = m_currentPanel;
    uiState["fileBrowserPath"] = m_fileBrowserPath;
    uiState["playlistColumns"] = QJsonArray::fromVariantList(m_playlistColumns);
    uiState["libraryGroupingLevels"] = QJsonArray::fromStringList(m_libraryGroupingLevels);
    json["uiState"] = uiState;
    
    // Playback state
    QJsonObject playback;
    playback["playlistIndex"] = m_lastPlaylistIndex;
    playback["positionMs"] = m_lastPosition;
    json["playback"] = playback;
    
    // Playlists
    json["playlists"] = serializePlaylists();
    
    // Active/displayed playlist IDs
    if (m_playlistStore) {
        json["activePlaylistId"] = m_playlistStore->activePlaylistId().toString();
        json["displayedPlaylistId"] = m_playlistStore->displayedPlaylistId().toString();
    }
    
    return json;
}

bool SessionManager::parseSessionJson(const QJsonObject &json)
{
    // Check version
    int version = json["version"].toInt(0);
    if (version < 1) {
        qWarning() << "Unknown session version";
        // Continue anyway, try to load what we can
    }
    
    // UI state
    if (json.contains("uiState")) {
        QJsonObject ui = json["uiState"].toObject();
        m_currentPanel = ui["currentPanel"].toInt(0);
        m_fileBrowserPath = ui["fileBrowserPath"].toString();
        m_playlistColumns = ui["playlistColumns"].toArray().toVariantList();
        
        // Load library grouping levels
        QJsonArray groupingArr = ui["libraryGroupingLevels"].toArray();
        m_libraryGroupingLevels.clear();
        for (const QJsonValue &v : groupingArr) {
            m_libraryGroupingLevels.append(v.toString());
        }
        
        emit currentPanelChanged();
        emit fileBrowserPathChanged();
        emit playlistColumnsChanged();
        emit libraryGroupingLevelsChanged();
    }
    
    // Playback state
    if (json.contains("playback")) {
        QJsonObject pb = json["playback"].toObject();
        m_lastPlaylistIndex = pb["playlistIndex"].toInt(-1);
        m_lastPosition = pb["positionMs"].toVariant().toLongLong();
    }
    
    // Playlists
    if (json.contains("playlists") && m_playlistStore) {
        deserializePlaylists(json["playlists"].toArray());
        
        // Restore active/displayed playlist
        QString activeId = json["activePlaylistId"].toString();
        QString displayedId = json["displayedPlaylistId"].toString();
        
        if (!activeId.isEmpty()) {
            m_playlistStore->setActivePlaylist(activeId);
        }
        if (!displayedId.isEmpty()) {
            m_playlistStore->setDisplayedPlaylist(displayedId);
        }
    }
    
    return true;
}

bool SessionManager::writeJsonAtomic(const QString &path, const QJsonObject &json)
{
    QSaveFile file(path);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << file.errorString();
        return false;
    }
    
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    if (file.write(data) != data.size()) {
        qWarning() << "Failed to write all data";
        file.cancelWriting();
        return false;
    }
    
    return file.commit();
}

QJsonArray SessionManager::serializePlaylists() const
{
    QJsonArray playlists;
    
    if (!m_playlistStore) {
        return playlists;
    }
    
    // Iterate through all tabs
    for (int i = 0; i < m_playlistStore->tabCount(); ++i) {
        QUuid uuid = m_playlistStore->tabUuid(i);
        QString name = m_playlistStore->tabName(i);
        
        QJsonObject playlist;
        playlist["uuid"] = uuid.toString();
        playlist["name"] = name;
        playlist["isUserCreated"] = m_playlistStore->tabIsUserCreated(i);
        
        // Add tracks
        TrackListModel *model = m_playlistStore->playlistModel(i);
        if (model) {
            QJsonArray tracks;
            for (int t = 0; t < model->count(); ++t) {
                TrackInfo track = model->trackAt(t);
                QJsonObject trackObj;
                trackObj["filePath"] = track.filePath;
                trackObj["title"] = track.title;
                trackObj["artist"] = track.artist;
                trackObj["album"] = track.album;
                trackObj["albumArtist"] = track.albumArtist;
                trackObj["performer"] = track.performer;
                trackObj["composer"] = track.composer;
                trackObj["year"] = track.year;
                trackObj["originalYear"] = track.originalYear;
                trackObj["trackNumber"] = track.trackNumber;
                trackObj["discNumber"] = track.discNumber;
                trackObj["durationMs"] = track.durationMs;
                trackObj["genre"] = track.genre;
                trackObj["sampleRate"] = track.sampleRate;
                trackObj["bitDepth"] = track.bitDepth;
                trackObj["bitrate"] = track.bitrate;
                tracks.append(trackObj);
            }
            playlist["tracks"] = tracks;
        }
        
        playlists.append(playlist);
    }
    
    return playlists;
}

bool SessionManager::deserializePlaylists(const QJsonArray &arr)
{
    if (!m_playlistStore || arr.isEmpty()) {
        return false;
    }
    
    // Close all existing tabs except one (we need at least one)
    while (m_playlistStore->tabCount() > 1) {
        QUuid uuid = m_playlistStore->tabUuid(0);
        m_playlistStore->closeTab(uuid.toString());
    }
    
    // Clear the remaining tab
    if (m_playlistStore->tabCount() > 0) {
        TrackListModel *model = m_playlistStore->playlistModel(0);
        if (model) {
            model->clear();
        }
    }
    
    bool firstPlaylist = true;
    
    for (const QJsonValue &val : arr) {
        QJsonObject playlist = val.toObject();
        QString name = playlist["name"].toString();
        QString savedUuid = playlist["uuid"].toString();
        // Default to true for backward compatibility with old sessions
        bool isUserCreated = playlist["isUserCreated"].toBool(true);
        
        QString uuid;
        if (firstPlaylist && m_playlistStore->tabCount() > 0) {
            // Reuse the first existing tab
            uuid = m_playlistStore->tabUuid(0).toString();
            m_playlistStore->renameTab(uuid, name);
            firstPlaylist = false;
        } else {
            // Create new tab with isUserCreated flag
            uuid = m_playlistStore->createNewTab(name, isUserCreated);
        }
        
        // Add tracks
        TrackListModel *model = m_playlistStore->getPlaylistModel(uuid);
        if (model) {
            QJsonArray tracks = playlist["tracks"].toArray();
            for (const QJsonValue &trackVal : tracks) {
                QJsonObject trackObj = trackVal.toObject();
                TrackInfo track;
                track.filePath = trackObj["filePath"].toString();
                track.title = trackObj["title"].toString();
                track.artist = trackObj["artist"].toString();
                track.album = trackObj["album"].toString();
                track.albumArtist = trackObj["albumArtist"].toString();
                track.performer = trackObj["performer"].toString();
                track.composer = trackObj["composer"].toString();
                track.year = trackObj["year"].toInt();
                track.originalYear = trackObj["originalYear"].toInt();
                track.trackNumber = trackObj["trackNumber"].toInt();
                track.discNumber = trackObj["discNumber"].toInt();
                track.durationMs = trackObj["durationMs"].toVariant().toLongLong();
                track.genre = trackObj["genre"].toString();
                track.sampleRate = trackObj["sampleRate"].toInt();
                track.bitDepth = trackObj["bitDepth"].toInt();
                track.bitrate = trackObj["bitrate"].toInt();
                
                // Derive fileName from filePath if not stored
                if (track.fileName.isEmpty() && !track.filePath.isEmpty()) {
                    track.fileName = QFileInfo(track.filePath).fileName();
                }
                
                model->addTrack(track);
            }
        }
    }
    
    return true;
}
