#ifndef LIBRARYDATABASE_H
#define LIBRARYDATABASE_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantMap>
#include <QVector>
#include <optional>
#include "TrackFilter.h"

struct LibraryTrack {
    qint64 id = -1;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString performer;
    QString composer;
    int trackNumber = 0;
    int discNumber = 0;
    int year = 0;
    int originalYear = 0;
    qint64 durationMs = 0;
    QString genre;
    int sampleRate = 0;
    int bitDepth = 0;
    int bitrate = 0;
    int channels = 0;
    QString url;
    QString fileName;
    QString fileType;
    qint64 fileSize = 0;
    qint64 createdTime = 0;
    qint64 modifiedTime = 0;
    QString comment;
    int bpm = 0;
    QString initialKey;
    QString codec;
    QVariantMap customTags;
    
    bool isValid() const { return id >= 0 && !filePath.isEmpty(); }
};

class LibraryDatabase : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LibraryDatabase is created by AppViewModel")

public:
    explicit LibraryDatabase(QObject *parent = nullptr);
    ~LibraryDatabase();
    
    bool open(const QString &path = QString());
    void close();
    bool isOpen() const;
    QString databasePath() const { return m_dbPath; }
    
    bool upsertTrack(const LibraryTrack &track);
    bool removeTrack(qint64 id);
    bool removeTracksInFolder(const QString &folderPath);
    
    std::optional<LibraryTrack> trackByPath(const QString &filePath) const;
    
    QVector<LibraryTrack> allTracks() const;
    QVector<LibraryTrack> searchTracks(const QString &query) const;
    QVector<LibraryTrack> tracksMatchingFilter(const TrackFilter &filter) const;
    Q_INVOKABLE QStringList customTagKeys() const;
    
    int trackCount() const;
    
    bool addWatchFolder(const QString &path);
    bool removeWatchFolder(const QString &path);
    QStringList watchFolders() const;
    static QString normalizeFileSystemPath(const QString &path);
    static QString normalizeWatchFolderPath(const QString &path);

    // SQL query for upsert - shared with LibraryScanner for thread-local connections
    static QString upsertTrackSql();
    static void bindTrackToQuery(QSqlQuery &query, const LibraryTrack &track);

    // Call this to notify listeners that the database content has changed externally
    // (e.g., after scanner finishes using its own connection)
    void notifyDatabaseChanged() { emit databaseChanged(); }

signals:
    void databaseChanged();

private:
    bool createTables();
    LibraryTrack trackFromQuery(const QSqlQuery &query) const;
    
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_dbPath;
};

#endif // LIBRARYDATABASE_H
