#include "LibraryScanner.h"
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
    scanFolders(QStringList{path});
}

void LibraryScanner::scanFolders(const QStringList &paths)
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
    connect(m_scanThread, &QThread::started, this, [this, paths]() {
        doScan(paths);
        
        QMetaObject::invokeMethod(this, [this]() {
            m_scanning = false;
            emit scanningChanged(false);
            if (m_cancelRequested) {
                emit scanCancelled();
            } else {
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

void LibraryScanner::doScan(const QStringList &paths)
{
    QStringList allFiles;
    for (const QString &path : paths) {
        allFiles.append(collectAudioFiles(path));
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
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA busy_timeout=5000");
        
        // Prepare query once and reuse
        QSqlQuery query(db);
        query.prepare(LibraryDatabase::upsertTrackSql());
        
        // Wrap in transaction for much better performance
        db.transaction();
        
        int processed = 0;
        constexpr int commitInterval = 100;
        
        for (const QString &filePath : allFiles) {
            if (m_cancelRequested)
                break;
            
            LibraryTrack track = MetadataExtractor::extractLibraryTrack(filePath);
            if (!track.filePath.isEmpty()) {
                LibraryDatabase::bindTrackToQuery(query, track);
                if (!query.exec()) {
                    qWarning() << "Scanner: Failed to insert track:" << query.lastError().text();
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

