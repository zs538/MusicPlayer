#include "LibraryDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QDebug>

LibraryDatabase::LibraryDatabase(QObject *parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString())
{
}

LibraryDatabase::~LibraryDatabase()
{
    close();
}

bool LibraryDatabase::open(const QString &path)
{
    m_dbPath = path;
    if (m_dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDir);
        m_dbPath = dataDir + "/library.db";
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
    
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    
    // Enable WAL mode and busy timeout for better concurrency with scanner thread
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA busy_timeout=5000");
    
    return createTables();
}

void LibraryDatabase::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool LibraryDatabase::isOpen() const
{
    return m_db.isOpen();
}

bool LibraryDatabase::createTables()
{
    QSqlQuery query(m_db);
    
    bool ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_path TEXT UNIQUE NOT NULL,
            title TEXT,
            artist TEXT,
            album TEXT,
            album_artist TEXT,
            performer TEXT,
            composer TEXT,
            track_number INTEGER DEFAULT 0,
            disc_number INTEGER DEFAULT 0,
            year INTEGER DEFAULT 0,
            original_year INTEGER DEFAULT 0,
            duration_ms INTEGER DEFAULT 0,
            genre TEXT,
            sample_rate INTEGER DEFAULT 0,
            bit_depth INTEGER DEFAULT 0,
            bitrate INTEGER DEFAULT 0,
            channels INTEGER DEFAULT 0,
            url TEXT,
            file_name TEXT,
            file_type TEXT,
            file_size INTEGER DEFAULT 0,
            created_time INTEGER DEFAULT 0,
            modified_time INTEGER DEFAULT 0,
            comment TEXT,
            bpm INTEGER DEFAULT 0,
            initial_key TEXT,
            codec TEXT
        )
    )");
    
    if (!ok) {
        qWarning() << "Failed to create tracks table:" << query.lastError().text();
        return false;
    }
    
    // Indexes for common queries (file_path already has implicit index from UNIQUE)
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album)");
    
    ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS watch_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT UNIQUE NOT NULL
        )
    )");
    
    if (!ok) {
        qWarning() << "Failed to create watch_folders table:" << query.lastError().text();
        return false;
    }
    
    // Playlists tables
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uuid TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            created_time INTEGER DEFAULT 0
        )
    )");
    
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            playlist_id INTEGER NOT NULL,
            position INTEGER NOT NULL,
            file_path TEXT NOT NULL,
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE
        )
    )");
    
    return true;
}

QString LibraryDatabase::upsertTrackSql()
{
    return QStringLiteral(R"(
        INSERT INTO tracks (file_path, title, artist, album, album_artist, performer, composer,
                           track_number, disc_number, year, original_year, duration_ms, genre,
                           sample_rate, bit_depth, bitrate, channels, url, file_name, file_type,
                           file_size, created_time, modified_time, comment, bpm, initial_key, codec)
        VALUES (:file_path, :title, :artist, :album, :album_artist, :performer, :composer,
                :track_number, :disc_number, :year, :original_year, :duration_ms, :genre,
                :sample_rate, :bit_depth, :bitrate, :channels, :url, :file_name, :file_type,
                :file_size, :created_time, :modified_time, :comment, :bpm, :initial_key, :codec)
        ON CONFLICT(file_path) DO UPDATE SET
            title = excluded.title,
            artist = excluded.artist,
            album = excluded.album,
            album_artist = excluded.album_artist,
            performer = excluded.performer,
            composer = excluded.composer,
            track_number = excluded.track_number,
            disc_number = excluded.disc_number,
            year = excluded.year,
            original_year = excluded.original_year,
            duration_ms = excluded.duration_ms,
            genre = excluded.genre,
            sample_rate = excluded.sample_rate,
            bit_depth = excluded.bit_depth,
            bitrate = excluded.bitrate,
            channels = excluded.channels,
            url = excluded.url,
            file_name = excluded.file_name,
            file_type = excluded.file_type,
            file_size = excluded.file_size,
            created_time = excluded.created_time,
            modified_time = excluded.modified_time,
            comment = excluded.comment,
            bpm = excluded.bpm,
            initial_key = excluded.initial_key,
            codec = excluded.codec
    )");
}

void LibraryDatabase::bindTrackToQuery(QSqlQuery &query, const LibraryTrack &track)
{
    query.bindValue(":file_path", track.filePath);
    query.bindValue(":title", track.title);
    query.bindValue(":artist", track.artist);
    query.bindValue(":album", track.album);
    query.bindValue(":album_artist", track.albumArtist);
    query.bindValue(":performer", track.performer);
    query.bindValue(":composer", track.composer);
    query.bindValue(":track_number", track.trackNumber);
    query.bindValue(":disc_number", track.discNumber);
    query.bindValue(":year", track.year);
    query.bindValue(":original_year", track.originalYear);
    query.bindValue(":duration_ms", track.durationMs);
    query.bindValue(":genre", track.genre);
    query.bindValue(":sample_rate", track.sampleRate);
    query.bindValue(":bit_depth", track.bitDepth);
    query.bindValue(":bitrate", track.bitrate);
    query.bindValue(":channels", track.channels);
    query.bindValue(":url", track.url);
    query.bindValue(":file_name", track.fileName);
    query.bindValue(":file_type", track.fileType);
    query.bindValue(":file_size", track.fileSize);
    query.bindValue(":created_time", track.createdTime);
    query.bindValue(":modified_time", track.modifiedTime);
    query.bindValue(":comment", track.comment);
    query.bindValue(":bpm", track.bpm);
    query.bindValue(":initial_key", track.initialKey);
    query.bindValue(":codec", track.codec);
}

bool LibraryDatabase::upsertTrack(const LibraryTrack &track)
{
    QSqlQuery query(m_db);
    query.prepare(upsertTrackSql());
    bindTrackToQuery(query, track);
    
    if (!query.exec()) {
        qWarning() << "Failed to upsert track:" << query.lastError().text();
        return false;
    }
    
    emit databaseChanged();
    return true;
}

bool LibraryDatabase::removeTrack(qint64 id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM tracks WHERE id = :id");
    query.bindValue(":id", id);
    
    if (!query.exec()) {
        return false;
    }
    
    if (query.numRowsAffected() > 0) {
        emit databaseChanged();
    }
    return true;
}

bool LibraryDatabase::removeTrackByPath(const QString &filePath)
{
    auto track = trackByPath(filePath);
    if (track) {
        return removeTrack(track->id);
    }
    return false;
}

bool LibraryDatabase::removeTracksInFolder(const QString &folderPath)
{
    QSqlQuery query(m_db);
    QString pattern = folderPath + "/%";
    query.prepare("DELETE FROM tracks WHERE file_path LIKE :pattern");
    query.bindValue(":pattern", pattern);
    
    if (!query.exec()) {
        return false;
    }
    
    if (query.numRowsAffected() > 0) {
        emit databaseChanged();
    }
    return true;
}

LibraryTrack LibraryDatabase::trackFromQuery(const QSqlQuery &query) const
{
    LibraryTrack track;
    track.id = query.value("id").toLongLong();
    track.filePath = query.value("file_path").toString();
    track.title = query.value("title").toString();
    track.artist = query.value("artist").toString();
    track.album = query.value("album").toString();
    track.albumArtist = query.value("album_artist").toString();
    track.performer = query.value("performer").toString();
    track.composer = query.value("composer").toString();
    track.trackNumber = query.value("track_number").toInt();
    track.discNumber = query.value("disc_number").toInt();
    track.year = query.value("year").toInt();
    track.originalYear = query.value("original_year").toInt();
    track.durationMs = query.value("duration_ms").toLongLong();
    track.genre = query.value("genre").toString();
    track.sampleRate = query.value("sample_rate").toInt();
    track.bitDepth = query.value("bit_depth").toInt();
    track.bitrate = query.value("bitrate").toInt();
    track.channels = query.value("channels").toInt();
    track.url = query.value("url").toString();
    track.fileName = query.value("file_name").toString();
    track.fileType = query.value("file_type").toString();
    track.fileSize = query.value("file_size").toLongLong();
    track.createdTime = query.value("created_time").toLongLong();
    track.modifiedTime = query.value("modified_time").toLongLong();
    track.comment = query.value("comment").toString();
    track.bpm = query.value("bpm").toInt();
    track.initialKey = query.value("initial_key").toString();
    track.codec = query.value("codec").toString();
    return track;
}

std::optional<LibraryTrack> LibraryDatabase::trackById(qint64 id) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM tracks WHERE id = :id");
    query.bindValue(":id", id);
    
    if (query.exec() && query.next()) {
        return trackFromQuery(query);
    }
    return std::nullopt;
}

std::optional<LibraryTrack> LibraryDatabase::trackByPath(const QString &filePath) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM tracks WHERE file_path = :path");
    query.bindValue(":path", filePath);
    
    if (query.exec() && query.next()) {
        return trackFromQuery(query);
    }
    return std::nullopt;
}

QVector<LibraryTrack> LibraryDatabase::allTracks() const
{
    QVector<LibraryTrack> tracks;
    QSqlQuery query(m_db);
    query.exec("SELECT * FROM tracks ORDER BY artist, album, disc_number, track_number");
    
    while (query.next()) {
        tracks.append(trackFromQuery(query));
    }
    return tracks;
}

QVector<LibraryTrack> LibraryDatabase::searchTracks(const QString &searchQuery) const
{
    QVector<LibraryTrack> tracks;
    QSqlQuery query(m_db);
    QString pattern = "%" + searchQuery + "%";
    query.prepare(R"(
        SELECT * FROM tracks 
        WHERE title LIKE :pattern 
           OR artist LIKE :pattern 
           OR album LIKE :pattern
        ORDER BY artist, album, track_number
    )");
    query.bindValue(":pattern", pattern);
    
    if (query.exec()) {
        while (query.next()) {
            tracks.append(trackFromQuery(query));
        }
    }
    return tracks;
}

int LibraryDatabase::trackCount() const
{
    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM tracks") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QVector<LibraryTrack> LibraryDatabase::tracksMatchingFilter(const TrackFilter &filter) const
{
    QVector<LibraryTrack> tracks;
    
    if (filter.isEmpty()) {
        return allTracks();
    }
    
    QString sql = "SELECT * FROM tracks WHERE 1=1";
    QVector<QPair<QString, QVariant>> bindings;
    int paramIndex = 0;
    
    for (const FilterCondition &cond : filter) {
        QString col = groupTypeToColumn(cond.field);
        if (col.isEmpty())
            continue;
        
        QString paramName = QString(":p%1").arg(paramIndex++);
        sql += QString(" AND %1 = %2").arg(col, paramName);
        bindings.append({paramName, cond.value});
    }
    
    sql += " ORDER BY album_artist, album, disc_number, track_number";
    
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const auto &binding : bindings) {
        query.bindValue(binding.first, binding.second);
    }
    
    if (query.exec()) {
        while (query.next()) {
            tracks.append(trackFromQuery(query));
        }
    }
    
    return tracks;
}

bool LibraryDatabase::addWatchFolder(const QString &path)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO watch_folders (path) VALUES (:path)");
    query.bindValue(":path", path);
    return query.exec();
}

bool LibraryDatabase::removeWatchFolder(const QString &path)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM watch_folders WHERE path = :path");
    query.bindValue(":path", path);
    return query.exec();
}

QStringList LibraryDatabase::watchFolders() const
{
    QStringList folders;
    QSqlQuery query(m_db);
    query.exec("SELECT path FROM watch_folders ORDER BY path");
    
    while (query.next()) {
        folders.append(query.value(0).toString());
    }
    return folders;
}
