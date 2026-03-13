#include "LibraryController.h"
#include "LibraryDatabase.h"
#include "LibraryScanner.h"
#include "LibraryWatcher.h"
#include "../Settings.h"
#include "../TrackFilter.h"
#include <QTimer>
#include <QDebug>

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_database(new LibraryDatabase(this))
{
}

LibraryController::~LibraryController() = default;

void LibraryController::initialize()
{
    m_database->open();
    m_scanner = new LibraryScanner(m_database, this);
    m_watcher = new LibraryWatcher(m_scanner, this);
    m_watcher->setWatchFolders(m_database->watchFolders());

    connect(m_scanner, &LibraryScanner::scanningChanged, this, &LibraryController::scanningChanged);
    connect(m_scanner, &LibraryScanner::progressChanged, this, &LibraryController::scanProgressChanged);

    connect(m_scanner, &LibraryScanner::scanFinished, this, [this]() {
        if (m_database)
            m_database->notifyDatabaseChanged();
        emit trackCountChanged();
        emit scanFinished();
    });

    // Defer Settings wiring until the Settings singleton exists
    QTimer::singleShot(0, this, &LibraryController::connectWatcherSettings);
}

void LibraryController::connectWatcherSettings()
{
    Settings *settings = Settings::instance();
    if (!settings) {
        qWarning() << "LibraryController: Settings not available for watcher initialization";
        return;
    }

    m_watcher->setEnabled(settings->watcherEnabled());
    m_watcher->setPeriodicRescanMinutes(settings->periodicRescanMinutes());

    connect(settings, &Settings::watcherEnabledChanged, this, [this]() {
        if (m_watcher && Settings::instance())
            m_watcher->setEnabled(Settings::instance()->watcherEnabled());
    });
    connect(settings, &Settings::periodicRescanMinutesChanged, this, [this]() {
        if (m_watcher && Settings::instance())
            m_watcher->setPeriodicRescanMinutes(Settings::instance()->periodicRescanMinutes());
    });

    qDebug() << "LibraryWatcher: Settings applied - enabled:" << settings->watcherEnabled()
             << ", periodic:" << settings->periodicRescanMinutes() << "min";
}

// --- Folder management ---

void LibraryController::addLibraryFolder(const QString &path)
{
    const QString normalizedPath = LibraryDatabase::normalizeWatchFolderPath(path);
    qDebug() << "LibraryController::addLibraryFolder:" << normalizedPath;
    m_database->addWatchFolder(normalizedPath);
    m_watcher->setWatchFolders(m_database->watchFolders());
    m_scanner->scanFolder(normalizedPath);
    emit libraryFoldersChanged();
}

void LibraryController::removeLibraryFolder(const QString &path)
{
    const QString normalizedPath = LibraryDatabase::normalizeWatchFolderPath(path);
    m_database->removeWatchFolder(normalizedPath);
    m_database->removeTracksInFolder(normalizedPath);
    m_watcher->setWatchFolders(m_database->watchFolders());
    m_database->notifyDatabaseChanged();
    emit libraryFoldersChanged();
    emit trackCountChanged();
}

QStringList LibraryController::libraryFolders() const
{
    return m_database->watchFolders();
}

// --- Scanning ---

void LibraryController::rescanLibrary()
{
    m_scanner->rescanAll();
}

void LibraryController::rescanCollectionEntry(const QVariantList &filter, const QString &entryType,
                                               const QString &groupType, const QVariant &groupValue,
                                               const QString &filePath)
{
    QStringList filePaths;

    if (entryType == QStringLiteral("group")) {
        TrackFilter trackFilter = trackFilterFromVariant(filter);
        if (!groupType.isEmpty()) {
            FilterCondition condition;
            condition.field = groupType;
            condition.op = QStringLiteral("=");
            condition.value = groupValue;
            trackFilter.append(condition);
        }

        const QVector<LibraryTrack> tracks = m_database->tracksMatchingFilter(trackFilter);
        for (const LibraryTrack &track : tracks) {
            if (!track.filePath.isEmpty())
                filePaths.append(track.filePath);
        }
    } else if (!filePath.isEmpty()) {
        filePaths.append(filePath);
    }

    rescanFiles(filePaths);
}

void LibraryController::rescanFiles(const QStringList &filePaths)
{
    if (!m_scanner)
        return;

    QStringList normalizedPaths;
    for (const QString &fp : filePaths) {
        const QString normalized = LibraryDatabase::normalizeFileSystemPath(fp);
        if (!normalized.isEmpty() && !normalizedPaths.contains(normalized))
            normalizedPaths.append(normalized);
    }

    if (!normalizedPaths.isEmpty())
        m_scanner->rescanFiles(normalizedPaths);
}

// --- State queries ---

bool LibraryController::isScanning() const
{
    return m_scanner ? m_scanner->isScanning() : false;
}

int LibraryController::scanProgress() const
{
    return m_scanner ? m_scanner->progress() : 0;
}

int LibraryController::trackCount() const
{
    return m_database ? m_database->trackCount() : 0;
}
