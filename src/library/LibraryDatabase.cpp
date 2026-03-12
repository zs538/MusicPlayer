#include "LibraryDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <QUrl>
#include <algorithm>
#include <QHash>
#include <QSet>

namespace {

constexpr int TrackSchemaVersion = 2;

const char *kCreateTracksSql = R"(
    CREATE TABLE IF NOT EXISTS tracks (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_path TEXT UNIQUE NOT NULL,
        title TEXT,
        artist TEXT,
        album TEXT,
        album_artist TEXT,
        track_number INTEGER DEFAULT 0,
        disc_number INTEGER DEFAULT 0,
        year INTEGER DEFAULT 0,
        duration_ms INTEGER DEFAULT 0,
        genre TEXT,
        bitrate INTEGER DEFAULT 0,
        file_name TEXT,
        file_type TEXT,
        file_size INTEGER DEFAULT 0,
        modified_time INTEGER DEFAULT 0
    )
)";

const char *kCreateTrackAttributesSql = R"(
    CREATE TABLE IF NOT EXISTS track_attributes (
        track_id INTEGER NOT NULL,
        key TEXT NOT NULL,
        value TEXT NOT NULL,
        PRIMARY KEY (track_id, key, value),
        FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE
    ) WITHOUT ROWID
)";

const char *kCreateTracksNewSql = R"(
    CREATE TABLE tracks_new (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        file_path TEXT UNIQUE NOT NULL,
        title TEXT,
        artist TEXT,
        album TEXT,
        album_artist TEXT,
        track_number INTEGER DEFAULT 0,
        disc_number INTEGER DEFAULT 0,
        year INTEGER DEFAULT 0,
        duration_ms INTEGER DEFAULT 0,
        genre TEXT,
        bitrate INTEGER DEFAULT 0,
        file_name TEXT,
        file_type TEXT,
        file_size INTEGER DEFAULT 0,
        modified_time INTEGER DEFAULT 0
    )
)";

const char *kCreateTrackAttributesNewSql = R"(
    CREATE TABLE track_attributes_new (
        track_id INTEGER NOT NULL,
        key TEXT NOT NULL,
        value TEXT NOT NULL,
        PRIMARY KEY (track_id, key, value),
        FOREIGN KEY (track_id) REFERENCES tracks_new(id) ON DELETE CASCADE
    ) WITHOUT ROWID
)";

QString baseTrackSelectSql()
{
    return QStringLiteral(
        "SELECT id, file_path, title, artist, album, album_artist, "
        "track_number, disc_number, year, duration_ms, genre, bitrate, "
        "file_name, file_type, file_size, modified_time FROM tracks");
}

QString normalizedSparseKey(const QString &key)
{
    return key.trimmed().toUpper();
}

QStringList variantToStringList(const QVariant &value)
{
    if (!value.isValid())
        return {};
    if (value.typeId() == QMetaType::QStringList)
        return value.toStringList();
    if (value.typeId() == QMetaType::QVariantList) {
        QStringList values;
        const QVariantList list = value.toList();
        values.reserve(list.size());
        for (const QVariant &item : list) {
            const QString text = item.toString().trimmed();
            if (!text.isEmpty())
                values.append(text);
        }
        return values;
    }
    const QString single = value.toString().trimmed();
    return single.isEmpty() ? QStringList{} : QStringList{single};
}

void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key,
                           const QString &value)
{
    const QString normalizedValue = value.trimmed();
    if (normalizedValue.isEmpty())
        return;
    attributes.append({key, normalizedValue});
}

void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key,
                           qint64 value)
{
    if (value <= 0)
        return;
    attributes.append({key, QString::number(value)});
}

bool execChecked(QSqlQuery &query, const QString &sql, const char *context)
{
    if (query.exec(sql))
        return true;
    qWarning() << context << query.lastError().text();
    return false;
}

bool replaceTrackAttributes(QSqlDatabase &db,
                           qint64 trackId,
                           const QVector<QPair<QString, QString>> &attributes,
                           const char *context)
{
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM track_attributes WHERE track_id = :track_id");
    deleteQuery.bindValue(":track_id", trackId);
    if (!deleteQuery.exec()) {
        qWarning() << context << deleteQuery.lastError().text();
        return false;
    }

    if (attributes.isEmpty())
        return true;

    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO track_attributes (track_id, key, value) VALUES (:track_id, :key, :value)");
    for (const auto &attribute : attributes) {
        insertQuery.bindValue(":track_id", trackId);
        insertQuery.bindValue(":key", attribute.first);
        insertQuery.bindValue(":value", attribute.second);
        if (!insertQuery.exec()) {
            qWarning() << context << insertQuery.lastError().text();
            return false;
        }
    }

    return true;
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

    if (!ensureTrackSchema(query))
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

QVector<QPair<QString, QString>> LibraryDatabase::sparseAttributesForTrack(const LibraryTrack &track)
{
    QVector<QPair<QString, QString>> attributes;
    appendSparseAttribute(attributes, QStringLiteral("_performer"), track.performer);
    appendSparseAttribute(attributes, QStringLiteral("_composer"), track.composer);
    appendSparseAttribute(attributes, QStringLiteral("_original_year"), track.originalYear);
    appendSparseAttribute(attributes, QStringLiteral("_sample_rate"), track.sampleRate);
    appendSparseAttribute(attributes, QStringLiteral("_bit_depth"), track.bitDepth);
    appendSparseAttribute(attributes, QStringLiteral("_channels"), track.channels);
    appendSparseAttribute(attributes, QStringLiteral("_url"), track.url);
    appendSparseAttribute(attributes, QStringLiteral("_created_time"), track.createdTime);
    appendSparseAttribute(attributes, QStringLiteral("_comment"), track.comment);
    appendSparseAttribute(attributes, QStringLiteral("_bpm"), track.bpm);
    appendSparseAttribute(attributes, QStringLiteral("_initial_key"), track.initialKey);
    appendSparseAttribute(attributes, QStringLiteral("_codec"), track.codec);

    for (auto it = track.customTags.constBegin(); it != track.customTags.constEnd(); ++it) {
        const QString normalizedKey = normalizedSparseKey(it.key());
        if (normalizedKey.isEmpty() || normalizedKey.startsWith('_'))
            continue;
        const QStringList values = variantToStringList(it.value());
        for (const QString &value : values)
            appendSparseAttribute(attributes, normalizedKey, value);
    }

    return attributes;
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

    if (!replaceTrackAttributes(m_db, idQuery.value(0).toLongLong(), sparseAttributesForTrack(track), "Failed to replace track attributes:"))
        return false;

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

bool LibraryDatabase::ensureTrackSchema(QSqlQuery &query)
{
    QStringList columns;
    if (!query.exec("PRAGMA table_info(tracks)")) {
        qWarning() << "Failed to inspect tracks schema:" << query.lastError().text();
        return false;
    }

    while (query.next())
        columns.append(query.value(1).toString());

    if (columns.isEmpty()) {
        if (!execChecked(query, QString::fromUtf8(kCreateTracksSql), "Failed to create tracks table:"))
            return false;
        if (!execChecked(query, QString::fromUtf8(kCreateTrackAttributesSql), "Failed to create track_attributes table:"))
            return false;
        query.exec(QString("PRAGMA user_version = %1").arg(TrackSchemaVersion));
        return true;
    }

    static const QSet<QString> legacyColumns = {
        "performer", "composer", "original_year", "sample_rate", "bit_depth",
        "channels", "url", "created_time", "comment", "bpm", "initial_key", "codec"
    };

    for (const QString &column : columns) {
        if (legacyColumns.contains(column))
            return migrateLegacyTrackSchema(query, columns);
    }

    if (!execChecked(query, QString::fromUtf8(kCreateTracksSql), "Failed to ensure tracks table:"))
        return false;
    if (!execChecked(query, QString::fromUtf8(kCreateTrackAttributesSql), "Failed to ensure track_attributes table:"))
        return false;
    query.exec(QString("PRAGMA user_version = %1").arg(TrackSchemaVersion));
    return true;
}

bool LibraryDatabase::migrateLegacyTrackSchema(QSqlQuery &query, const QStringList &columns)
{
    Q_UNUSED(columns)

    if (!m_db.transaction()) {
        qWarning() << "Failed to start track schema migration transaction:" << m_db.lastError().text();
        return false;
    }

    const auto fail = [this]() {
        m_db.rollback();
        return false;
    };

    if (!execChecked(query, "DROP TABLE IF EXISTS track_attributes_new", "Failed to reset track_attributes_new table:"))
        return fail();
    if (!execChecked(query, "DROP TABLE IF EXISTS tracks_new", "Failed to reset tracks_new table:"))
        return fail();
    if (!execChecked(query, QString::fromUtf8(kCreateTracksNewSql), "Failed to create tracks_new table:"))
        return fail();
    if (!execChecked(query, QString::fromUtf8(kCreateTrackAttributesNewSql), "Failed to create track_attributes_new table:"))
        return fail();

    if (!execChecked(query, R"(
        INSERT INTO tracks_new (
            id, file_path, title, artist, album, album_artist,
            track_number, disc_number, year, duration_ms, genre,
            bitrate, file_name, file_type, file_size, modified_time
        )
        SELECT
            id, file_path, title, artist, album, album_artist,
            track_number, disc_number, year, duration_ms, genre,
            bitrate, file_name, file_type, file_size, modified_time
        FROM tracks
    )", "Failed to copy tracks into new schema:"))
        return fail();

    const QStringList migrationSql = {
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_performer', performer FROM tracks WHERE performer IS NOT NULL AND trim(performer) != ''"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_composer', composer FROM tracks WHERE composer IS NOT NULL AND trim(composer) != ''"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_original_year', CAST(original_year AS TEXT) FROM tracks WHERE original_year > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_sample_rate', CAST(sample_rate AS TEXT) FROM tracks WHERE sample_rate > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_bit_depth', CAST(bit_depth AS TEXT) FROM tracks WHERE bit_depth > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_channels', CAST(channels AS TEXT) FROM tracks WHERE channels > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_url', url FROM tracks WHERE url IS NOT NULL AND trim(url) != ''"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_created_time', CAST(created_time AS TEXT) FROM tracks WHERE created_time > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_comment', comment FROM tracks WHERE comment IS NOT NULL AND trim(comment) != ''"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_bpm', CAST(bpm AS TEXT) FROM tracks WHERE bpm > 0"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_initial_key', initial_key FROM tracks WHERE initial_key IS NOT NULL AND trim(initial_key) != ''"),
        QStringLiteral("INSERT INTO track_attributes_new (track_id, key, value) SELECT id, '_codec', codec FROM tracks WHERE codec IS NOT NULL AND trim(codec) != ''")
    };

    for (const QString &sql : migrationSql) {
        if (!execChecked(query, sql, "Failed during sparse attribute migration:"))
            return fail();
    }

    if (!execChecked(query, "DROP TABLE IF EXISTS track_attributes", "Failed to drop old track_attributes table:"))
        return fail();
    if (!execChecked(query, "DROP TABLE tracks", "Failed to drop legacy tracks table:"))
        return fail();
    if (!execChecked(query, "ALTER TABLE tracks_new RENAME TO tracks", "Failed to rename tracks_new table:"))
        return fail();
    if (!execChecked(query, "ALTER TABLE track_attributes_new RENAME TO track_attributes", "Failed to rename track_attributes_new table:"))
        return fail();
    if (!execChecked(query, QString("PRAGMA user_version = %1").arg(TrackSchemaVersion), "Failed to bump schema version:"))
        return fail();

    if (!m_db.commit()) {
        qWarning() << "Failed to commit track schema migration:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }

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
        hydrateSparseAttributes(track);
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
    hydrateSparseAttributes(tracks);
    return tracks;
}

QVector<LibraryTrack> LibraryDatabase::searchTracks(const QString &searchQuery) const
{
    QVector<LibraryTrack> tracks;
    QSqlQuery query(m_db);
    QString pattern = "%" + searchQuery + "%";
    query.prepare(baseTrackSelectSql() + R"(
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
    hydrateSparseAttributes(tracks);
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

    hydrateSparseAttributes(tracks);
    return tracks;
}

void LibraryDatabase::applySparseAttribute(LibraryTrack &track, const QString &key, const QString &value) const
{
    bool ok = false;
    if (key == "_performer") {
        track.performer = value;
    } else if (key == "_composer") {
        track.composer = value;
    } else if (key == "_original_year") {
        track.originalYear = value.toInt(&ok);
        if (!ok)
            track.originalYear = 0;
    } else if (key == "_sample_rate") {
        track.sampleRate = value.toInt(&ok);
        if (!ok)
            track.sampleRate = 0;
    } else if (key == "_bit_depth") {
        track.bitDepth = value.toInt(&ok);
        if (!ok)
            track.bitDepth = 0;
    } else if (key == "_channels") {
        track.channels = value.toInt(&ok);
        if (!ok)
            track.channels = 0;
    } else if (key == "_url") {
        track.url = value;
    } else if (key == "_created_time") {
        track.createdTime = value.toLongLong(&ok);
        if (!ok)
            track.createdTime = 0;
    } else if (key == "_comment") {
        track.comment = value;
    } else if (key == "_bpm") {
        track.bpm = value.toInt(&ok);
        if (!ok)
            track.bpm = 0;
    } else if (key == "_initial_key") {
        track.initialKey = value;
    } else if (key == "_codec") {
        track.codec = value;
    } else {
        const QString normalizedKey = normalizedSparseKey(key);
        QStringList values = variantToStringList(track.customTags.value(normalizedKey));
        if (!values.contains(value))
            values.append(value);
        track.customTags.insert(normalizedKey, values);
    }
}

void LibraryDatabase::hydrateSparseAttributes(QVector<LibraryTrack> &tracks) const
{
    if (tracks.isEmpty())
        return;

    QHash<qint64, int> rowById;
    rowById.reserve(tracks.size());
    for (int i = 0; i < tracks.size(); ++i)
        rowById.insert(tracks[i].id, i);

    constexpr int chunkSize = 500;
    for (int offset = 0; offset < tracks.size(); offset += chunkSize) {
        const int end = std::min(offset + chunkSize, static_cast<int>(tracks.size()));
        QStringList placeholders;
        placeholders.reserve(end - offset);

        QSqlQuery query(m_db);
        for (int i = offset; i < end; ++i)
            placeholders.append(QString(":id%1").arg(i - offset));

        query.prepare(QString("SELECT track_id, key, value FROM track_attributes WHERE track_id IN (%1) ORDER BY track_id")
                          .arg(placeholders.join(", ")));

        for (int i = offset; i < end; ++i)
            query.bindValue(QString(":id%1").arg(i - offset), tracks[i].id);

        if (!query.exec())
            continue;

        while (query.next()) {
            const qint64 trackId = query.value(0).toLongLong();
            auto it = rowById.constFind(trackId);
            if (it == rowById.constEnd())
                continue;
            applySparseAttribute(tracks[it.value()], query.value(1).toString(), query.value(2).toString());
        }
    }
}

void LibraryDatabase::hydrateSparseAttributes(LibraryTrack &track) const
{
    if (track.id < 0)
        return;

    QSqlQuery query(m_db);
    query.prepare("SELECT key, value FROM track_attributes WHERE track_id = :track_id ORDER BY key, value");
    query.bindValue(":track_id", track.id);
    if (!query.exec())
        return;

    while (query.next())
        applySparseAttribute(track, query.value(0).toString(), query.value(1).toString());
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
