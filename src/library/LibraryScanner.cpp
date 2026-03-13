#include "LibraryScanner.h"
#include "LibrarySparseAttributes.h"
#include "MetadataExtractor.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

const QStringList LibraryScanner::s_audioExtensions = {
    "mp3", "flac", "wav", "ogg", "m4a", "aac", "opus", "wma", "ape", "wv", "aiff", "aif"
};

LibraryScanner::LibraryScanner(LibraryDatabase *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

LibraryScanner::~LibraryScanner()
{
    cancelScan();
    if (m_scanThread) {
        m_scanThread->quit();
        m_scanThread->wait();
        delete m_scanThread;
    }
}

void LibraryScanner::scanFolder(const QString &path)
{
    scanFolders(QStringList{path}, false);
}

void LibraryScanner::scanFolders(const QStringList &paths, bool detectDeletions)
{
    startScan(paths, detectDeletions, false);
}

void LibraryScanner::rescanFiles(const QStringList &filePaths)
{
    startScan(filePaths, false, true);
}

void LibraryScanner::startScan(const QStringList &paths, bool detectDeletions, bool inputPathsAreFiles)
{
    if (m_scanning)
        return;
    
    m_scanning = true;
    m_cancelRequested = false;
    m_progress = 0;
    m_totalFiles = 0;
    
    emit scanningChanged(true);
    emit scanStarted();
    
    // Stop and clean up any existing thread
    if (m_scanThread) {
        m_scanThread->quit();
        m_scanThread->wait();
        delete m_scanThread;
        m_scanThread = nullptr;
    }
    
    m_scanThread = new QThread();
    
    // Use QtConcurrent-style: run doScan in the thread
    connect(m_scanThread, &QThread::started, this, [this, paths, detectDeletions, inputPathsAreFiles]() {
        doScan(paths, detectDeletions, inputPathsAreFiles);
        
        QMetaObject::invokeMethod(this, [this]() {
            m_scanning = false;
            emit scanningChanged(false);
            if (m_cancelRequested) {
                qDebug() << "LibraryScanner: Scan cancelled after" << m_progress << "of" << m_totalFiles << "files";
                emit scanCancelled();
            } else {
                qDebug() << "LibraryScanner: Scan finished -" << m_progress << "files processed, library now has" << m_db->trackCount() << "tracks";
                emit scanFinished();
            }
        }, Qt::QueuedConnection);
        
        m_scanThread->quit();
    }, Qt::DirectConnection);
    
    m_scanThread->start();
}

void LibraryScanner::rescanAll()
{
    QStringList folders = m_db->watchFolders();
    if (!folders.isEmpty()) {
        scanFolders(folders);
    }
}

void LibraryScanner::cancelScan()
{
    m_cancelRequested = true;
}

void LibraryScanner::doScan(const QStringList &paths, bool detectDeletions, bool inputPathsAreFiles)
{
    QStringList allFiles;

    if (inputPathsAreFiles) {
        QSet<QString> seenFiles;
        for (const QString &path : paths) {
            const QString normalizedPath = LibraryDatabase::normalizeFileSystemPath(path);
            if (normalizedPath.isEmpty() || seenFiles.contains(normalizedPath)) {
                continue;
            }
            if (!QFileInfo::exists(normalizedPath) || !isAudioFile(normalizedPath)) {
                continue;
            }

            seenFiles.insert(normalizedPath);
            allFiles.append(normalizedPath);
        }
    } else {
        for (const QString &path : paths) {
            allFiles.append(collectAudioFiles(path));
        }
    }
    
    m_totalFiles = allFiles.size();
    QMetaObject::invokeMethod(this, [this]() {
        emit totalFilesChanged(m_totalFiles);
    }, Qt::QueuedConnection);
    
    if (allFiles.isEmpty())
        return;
    
    // Create a thread-local database connection for scanning
    QString connectionName = "scanner_" + QUuid::createUuid().toString();
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(m_db->databasePath());
        
        if (!db.open()) {
            qWarning() << "Scanner: Failed to open database";
            QSqlDatabase::removeDatabase(connectionName);
            return;
        }
        
        // Enable WAL mode and set busy timeout for better concurrency
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA foreign_keys=ON");
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA busy_timeout=5000");
        
        QString upsertSql = LibraryDatabase::upsertTrackSql();
        QSqlQuery upsertQuery(db);
        upsertQuery.prepare(upsertSql);
        QSqlQuery idQuery(db);
        idQuery.prepare("SELECT id FROM tracks WHERE file_path = :file_path");
        
        // Wrap in transaction for much better performance
        db.transaction();
        
        int processed = 0;
        constexpr int commitInterval = 100;
        
        for (const QString &filePath : allFiles) {
            if (m_cancelRequested)
                break;
            
            LibraryTrack track = MetadataExtractor::extractLibraryTrack(filePath);
            if (!track.filePath.isEmpty()) {
                LibraryDatabase::bindTrackToQuery(upsertQuery, track);
                if (!upsertQuery.exec()) {
                    qWarning() << "Scanner: Failed to insert track:" << upsertQuery.lastError().text();
                } else {
                    idQuery.bindValue(":file_path", track.filePath);
                    if (idQuery.exec() && idQuery.next()) {
                        const qint64 trackId = idQuery.value(0).toLongLong();
                        const QVector<QPair<QString, QString>> attributes = LibrarySparseAttributes::sparseAttributesForTrack(track);
                        if (!LibrarySparseAttributes::replaceTrackAttributes(db, trackId, attributes,
                                                                            "Scanner: Failed to replace track attributes:")) {
                            qWarning() << "Scanner: Failed to replace sparse attributes for" << track.filePath;
                        }
                    } else {
                        qWarning() << "Scanner: Failed to resolve track id after insert:" << idQuery.lastError().text();
                    }
                }
            }
            
            processed++;
            m_progress = processed;
            
            // Commit periodically and emit progress
            if (processed % commitInterval == 0) {
                db.commit();
                db.transaction();
                QMetaObject::invokeMethod(this, [this, processed]() {
                    emit progressChanged(processed);
                }, Qt::QueuedConnection);
            }
        }
        
        // Deletion detection: remove DB entries for files no longer on disk
        if (detectDeletions && !m_cancelRequested) {
            QSet<QString> diskSet(allFiles.begin(), allFiles.end());
            
            for (const QString &folderPath : paths) {
                QString pattern = folderPath + "/%";
                QSqlQuery selectQuery(db);
                selectQuery.prepare("SELECT file_path FROM tracks WHERE file_path LIKE :pattern");
                selectQuery.bindValue(":pattern", pattern);
                
                if (selectQuery.exec()) {
                    QSqlQuery delQuery(db);
                    delQuery.prepare("DELETE FROM tracks WHERE file_path = :path");
                    while (selectQuery.next()) {
                        QString dbPath = selectQuery.value(0).toString();
                        if (!diskSet.contains(dbPath)) {
                            delQuery.bindValue(":path", dbPath);
                            if (delQuery.exec()) {
                                qDebug() << "Scanner: Removed deleted file:" << dbPath;
                            }
                        }
                    }
                }
            }
        }
        
        db.commit();
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QStringList LibraryScanner::collectAudioFiles(const QString &path)
{
    QStringList files;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        if (isAudioFile(filePath)) {
            files.append(filePath);
        }
    }
    
    return files;
}

bool LibraryScanner::isAudioFile(const QString &path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    return s_audioExtensions.contains(ext);
}

