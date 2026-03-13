#include "LibraryMigration.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QStringList>
#include <QDebug>

namespace {

bool execChecked(QSqlQuery &query, const QString &sql, const char *context)
{
    if (query.exec(sql))
        return true;
    qWarning() << context << query.lastError().text();
    return false;
}

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

bool migrateLegacyTrackSchema(QSqlDatabase &db, QSqlQuery &query)
{
    if (!db.transaction()) {
        qWarning() << "Failed to start track schema migration transaction:" << db.lastError().text();
        return false;
    }

    const auto fail = [&db]() {
        db.rollback();
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
    if (!execChecked(query, QString("PRAGMA user_version = %1").arg(LibraryMigration::TrackSchemaVersion), "Failed to bump schema version:"))
        return fail();

    if (!db.commit()) {
        qWarning() << "Failed to commit track schema migration:" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

} // anonymous namespace

namespace LibraryMigration {

bool ensureTrackSchema(QSqlDatabase &db, QSqlQuery &query)
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
            return migrateLegacyTrackSchema(db, query);
    }

    if (!execChecked(query, QString::fromUtf8(kCreateTracksSql), "Failed to ensure tracks table:"))
        return false;
    if (!execChecked(query, QString::fromUtf8(kCreateTrackAttributesSql), "Failed to ensure track_attributes table:"))
        return false;
    query.exec(QString("PRAGMA user_version = %1").arg(TrackSchemaVersion));
    return true;
}

} // namespace LibraryMigration
