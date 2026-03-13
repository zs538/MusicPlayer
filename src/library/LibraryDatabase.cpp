#include "LibraryDatabase.h"
#include "LibraryMigration.h"
#include "LibrarySparseAttributes.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <QUrl>

namespace LSA = LibrarySparseAttributes;

namespace {

QString baseTrackSelectSql()
{
    return QStringLiteral(
        "SELECT id, file_path, title, artist, album, album_artist, "
        "track_number, disc_number, year, duration_ms, genre, bitrate, "
        "file_name, file_type, file_size, modified_time FROM tracks");
}

}

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
    pragma.exec("PRAGMA foreign_keys=ON");
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

    if (!LibraryMigration::ensureTrackSchema(m_db, query))
        return false;

    // Indexes for common queries (file_path already has implicit index from UNIQUE)
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_album_artist ON tracks(album_artist)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_genre ON tracks(genre)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_year ON tracks(year)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_bitrate ON tracks(bitrate)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_file_type ON tracks(file_type)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tracks_modified_time ON tracks(modified_time)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_track_attributes_key_value ON track_attributes(key, value, track_id)");
    
    bool ok = query.exec(R"(
        CREATE TABLE IF NOT EXISTS watch_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT UNIQUE NOT NULL
        )
    )");
    
    if (!ok) {
        qWarning() << "Failed to create watch_folders table:" << query.lastError().text();
        return false;
    }
    
    return true;
}

QString LibraryDatabase::upsertTrackSql()
{
    return QStringLiteral(R"(
        INSERT INTO tracks (file_path, title, artist, album, album_artist,
                           track_number, disc_number, year, duration_ms, genre,
                           bitrate, file_name, file_type, file_size, modified_time)
        VALUES (:file_path, :title, :artist, :album, :album_artist,
                :track_number, :disc_number, :year, :duration_ms, :genre,
                :bitrate, :file_name, :file_type, :file_size, :modified_time)
        ON CONFLICT(file_path) DO UPDATE SET
            title = excluded.title,
            artist = excluded.artist,
            album = excluded.album,
            album_artist = excluded.album_artist,
            track_number = excluded.track_number,
            disc_number = excluded.disc_number,
            year = excluded.year,
            duration_ms = excluded.duration_ms,
            genre = excluded.genre,
            bitrate = excluded.bitrate,
            file_name = excluded.file_name,
            file_type = excluded.file_type,
            file_size = excluded.file_size,
            modified_time = excluded.modified_time
    )");
}

void LibraryDatabase::bindTrackToQuery(QSqlQuery &query, const LibraryTrack &track)
{
    query.bindValue(":file_path", track.filePath);
    query.bindValue(":title", track.title);
    query.bindValue(":artist", track.artist);
    query.bindValue(":album", track.album);
    query.bindValue(":album_artist", track.albumArtist);
    query.bindValue(":track_number", track.trackNumber);
    query.bindValue(":disc_number", track.discNumber);
    query.bindValue(":year", track.year);
    query.bindValue(":duration_ms", track.durationMs);
    query.bindValue(":genre", track.genre);
    query.bindValue(":bitrate", track.bitrate);
    query.bindValue(":file_name", track.fileName);
    query.bindValue(":file_type", track.fileType);
    query.bindValue(":file_size", track.fileSize);
    query.bindValue(":modified_time", track.modifiedTime);
}

bool LibraryDatabase::upsertTrack(const LibraryTrack &track)
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery upsertQuery(m_db);
    upsertQuery.prepare(upsertTrackSql());
    bindTrackToQuery(upsertQuery, track);
    if (!upsertQuery.exec()) {
        qWarning() << "Failed to upsert track:" << upsertQuery.lastError().text();
        return false;
    }

    QSqlQuery idQuery(m_db);
    idQuery.prepare("SELECT id FROM tracks WHERE file_path = :file_path");
    idQuery.bindValue(":file_path", track.filePath);
    if (!idQuery.exec() || !idQuery.next()) {
        qWarning() << "Failed to resolve track id after upsert:" << idQuery.lastError().text();
        return false;
    }

    if (!LSA::replaceTrackAttributes(m_db, idQuery.value(0).toLongLong(), LSA::sparseAttributesForTrack(track), "Failed to replace track attributes:"))
        return false;

    emit databaseChanged();
    return true;
}

QString LibraryDatabase::normalizeFileSystemPath(const QString &path)
{
    QString normalized = path.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QUrl url(normalized);
    if (url.isValid() && url.isLocalFile()) {
        normalized = url.toLocalFile();
    }

    normalized = QDir::cleanPath(QDir::fromNativeSeparators(normalized));

#ifdef Q_OS_WIN
    if (normalized.size() >= 3 && normalized.at(0) == '/' && normalized.at(2) == ':') {
        normalized.remove(0, 1);
    }

    if (normalized.size() >= 2 && normalized.at(1) == ':') {
        normalized[0] = normalized.at(0).toUpper();
    }
#endif

    return normalized;
}

QString LibraryDatabase::normalizeWatchFolderPath(const QString &path)
{
    return normalizeFileSystemPath(path);
}

bool LibraryDatabase::removeTracksInFolder(const QString &folderPath)
{
    const QString normalizedFolderPath = normalizeWatchFolderPath(folderPath);
    QSqlQuery query(m_db);
    QString pattern = normalizedFolderPath + "/%";
    query.prepare(R"(
        DELETE FROM tracks
        WHERE lower(replace(file_path, '\\', '/')) = lower(:folderPath)
           OR lower(replace(file_path, '\\', '/')) LIKE lower(:pattern)
    )");
    query.bindValue(":folderPath", normalizedFolderPath);
    query.bindValue(":pattern", pattern);
    
    if (!query.exec()) {
        return false;
    }
    
    int affected = query.numRowsAffected();
    qDebug() << "removeTracksInFolder:" << normalizedFolderPath << "- removed" << affected << "tracks";
    
    emit databaseChanged();
    return true;
}

std::optional<LibraryTrack> LibraryDatabase::trackByPath(const QString &filePath) const
{
    const QString normalizedPath = normalizeFileSystemPath(filePath);
    QSqlQuery query(m_db);
    query.prepare(baseTrackSelectSql() + " WHERE file_path = :path");
    query.bindValue(":path", normalizedPath);
    
    if (query.exec() && query.next()) {
        LibraryTrack track = trackFromQuery(query);
        LSA::hydrateSparseAttributes(m_db, track);
        return track;
    }
    return std::nullopt;
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
    track.trackNumber = query.value("track_number").toInt();
    track.discNumber = query.value("disc_number").toInt();
    track.year = query.value("year").toInt();
    track.durationMs = query.value("duration_ms").toLongLong();
    track.genre = query.value("genre").toString();
    track.bitrate = query.value("bitrate").toInt();
    track.fileName = query.value("file_name").toString();
    track.fileType = query.value("file_type").toString();
    track.fileSize = query.value("file_size").toLongLong();
    track.modifiedTime = query.value("modified_time").toLongLong();
    return track;
}

QVector<LibraryTrack> LibraryDatabase::allTracks() const
{
    QVector<LibraryTrack> tracks;
    QSqlQuery query(m_db);
    query.exec(baseTrackSelectSql() + " ORDER BY artist, album, disc_number, track_number");

    while (query.next()) {
        tracks.append(trackFromQuery(query));
    }
    LSA::hydrateSparseAttributes(m_db, tracks);
    return tracks;
}

QStringList LibraryDatabase::customTagKeys() const
{
    QStringList keys;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT DISTINCT key FROM track_attributes WHERE substr(key, 1, 1) <> '_' ORDER BY key"))
        return keys;

    while (query.next())
        keys.append(query.value(0).toString());
    return keys;
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

    QString sql = baseTrackSelectSql() + " WHERE 1=1";
    QVector<QPair<QString, QVariant>> bindings;
    int paramIndex = 0;

    for (const FilterCondition &cond : filter) {
        const QString sparseKey = isCustomGroupType(cond.field) ? customGroupKey(cond.field)
                                                                 : groupTypeToSparseAttributeKey(cond.field);
        if (!sparseKey.isEmpty()) {
            const QString keyParam = QString(":p%1").arg(paramIndex++);
            const QString valueText = cond.value.toString().trimmed();
            const bool negated = cond.op == "!=" || cond.op == "<>";
            if (valueText.isEmpty()) {
                sql += QString(
                    negated
                        ? " AND EXISTS (SELECT 1 FROM track_attributes ta WHERE ta.track_id = tracks.id AND ta.key = %1 AND ta.value <> '')"
                        : " AND (NOT EXISTS (SELECT 1 FROM track_attributes ta WHERE ta.track_id = tracks.id AND ta.key = %1) OR EXISTS (SELECT 1 FROM track_attributes ta WHERE ta.track_id = tracks.id AND ta.key = %1 AND ta.value = ''))"
                ).arg(keyParam);
            } else {
                const QString valueParam = QString(":p%1").arg(paramIndex++);
                sql += QString(
                    negated
                        ? " AND NOT EXISTS (SELECT 1 FROM track_attributes ta WHERE ta.track_id = tracks.id AND ta.key = %1 AND ta.value = %2)"
                        : " AND EXISTS (SELECT 1 FROM track_attributes ta WHERE ta.track_id = tracks.id AND ta.key = %1 AND ta.value = %2)"
                ).arg(keyParam, valueParam);
                bindings.append({valueParam, valueText});
            }
            bindings.append({keyParam, sparseKey});
            continue;
        }

        QString col = groupTypeToColumn(cond.field);
        if (col.isEmpty())
            continue;

        QString paramName = QString(":p%1").arg(paramIndex++);
        const QString op = (cond.op == "!=" || cond.op == "<>") ? QStringLiteral("<>") : QStringLiteral("=");
        sql += QString(" AND %1 %2 %3").arg(col, op, paramName);
        bindings.append({paramName, cond.value});
    }

    sql += " ORDER BY COALESCE(NULLIF(album_artist, ''), artist), album, disc_number, track_number";

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

    LSA::hydrateSparseAttributes(m_db, tracks);
    return tracks;
}

bool LibraryDatabase::addWatchFolder(const QString &path)
{
    const QString normalizedPath = normalizeWatchFolderPath(path);
    if (normalizedPath.isEmpty()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO watch_folders (path) VALUES (:path)");
    query.bindValue(":path", normalizedPath);
    return query.exec();
}

bool LibraryDatabase::removeWatchFolder(const QString &path)
{
    const QString normalizedPath = normalizeWatchFolderPath(path);
    if (normalizedPath.isEmpty()) {
        return false;
    }

    QVector<qint64> idsToDelete;
    QSqlQuery selectQuery(m_db);
    selectQuery.prepare("SELECT id, path FROM watch_folders");

    if (!selectQuery.exec()) {
        return false;
    }

    while (selectQuery.next()) {
        const qint64 id = selectQuery.value(0).toLongLong();
        const QString storedPath = selectQuery.value(1).toString();
        if (normalizeWatchFolderPath(storedPath) == normalizedPath) {
            idsToDelete.append(id);
        }
    }

    if (idsToDelete.isEmpty()) {
        qDebug() << "removeWatchFolder:" << normalizedPath << "- no matching stored folder row found";
        return true;
    }

    QSqlQuery deleteQuery(m_db);
    deleteQuery.prepare("DELETE FROM watch_folders WHERE id = :id");

    bool removedAny = false;
    for (qint64 id : idsToDelete) {
        deleteQuery.bindValue(":id", id);
        if (!deleteQuery.exec()) {
            return false;
        }
        removedAny = true;
    }

    qDebug() << "removeWatchFolder:" << normalizedPath << "- removed" << idsToDelete.size() << "stored folder row(s)";
    return removedAny;
}

QStringList LibraryDatabase::watchFolders() const
{
    QStringList folders;
    QSqlQuery query(m_db);
    query.exec("SELECT path FROM watch_folders ORDER BY path");

    while (query.next()) {
        folders.append(normalizeWatchFolderPath(query.value(0).toString()));
    }
    return folders;
}
