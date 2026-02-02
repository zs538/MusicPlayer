#include "LibraryWatcher.h"
#include "LibraryScanner.h"
#include <QDebug>
#include <QDirIterator>

LibraryWatcher::LibraryWatcher(LibraryScanner *scanner, QObject *parent)
    : QObject(parent)
    , m_scanner(scanner)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounceTimer(new QTimer(this))
    , m_periodicRescanTimer(new QTimer(this))
{
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DEBOUNCE_MS);
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, 
            this, &LibraryWatcher::onDirectoryChanged);
    connect(m_debounceTimer, &QTimer::timeout, 
            this, &LibraryWatcher::onDebounceTimeout);
    connect(m_periodicRescanTimer, &QTimer::timeout,
            this, &LibraryWatcher::onPeriodicRescanTimeout);
    connect(m_scanner, &LibraryScanner::scanFinished,
            this, &LibraryWatcher::onScanFinished);
}

void LibraryWatcher::setWatchFolders(const QStringList &folders)
{
    qDebug() << "LibraryWatcher::setWatchFolders called with" << folders.size() << "root folders:" << folders;
    
    // Skip if already watching the same folders (avoid redundant re-enumeration)
    if (folders == m_watchedFolders && !m_watcher->directories().isEmpty()) {
        qDebug() << "LibraryWatcher: Skipping - already watching same folders";
        return;
    }
    
    m_watchedFolders = folders;
    
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    
    if (!m_enabled || folders.isEmpty()) {
        qDebug() << "LibraryWatcher: Not setting up watches - enabled:" << m_enabled << "folders empty:" << folders.isEmpty();
        return;
    }
    
    // Collect all subdirectories recursively for each root folder
    QStringList allDirs;
    for (const QString &root : folders) {
        QDir rootDir(root);
        if (!rootDir.exists()) {
            qWarning() << "LibraryWatcher: Root folder does not exist:" << root;
            continue;
        }
        allDirs.append(root);
        QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            allDirs.append(it.next());
        }
    }
    
    qDebug() << "LibraryWatcher: Found" << allDirs.size() << "directories to watch";
    
    // Add all paths at once - QFileSystemWatcher handles this internally
    if (!allDirs.isEmpty()) {
        QStringList failed = m_watcher->addPaths(allDirs);
        int successCount = allDirs.size() - failed.size();
        
        if (!failed.isEmpty()) {
            // Only warn once with count, not every failed path (too verbose for large libs)
            qWarning() << "LibraryWatcher: Failed to watch" << failed.size() << "directories"
                       << "(inotify limit may be reached, see /proc/sys/fs/inotify/max_user_watches)";
            // Emit error signal for first few failures for debugging
            for (int i = 0; i < qMin(3, failed.size()); ++i) {
                emit watcherError(failed[i], "Failed to add watch");
            }
        }
        
        qDebug() << "LibraryWatcher: Successfully watching" << successCount << "of" << allDirs.size() << "directories";
    }
}

void LibraryWatcher::onDirectoryChanged(const QString &path)
{
    if (!m_enabled)
        return;
    
    qDebug() << "LibraryWatcher: Directory changed:" << path;
    
    // Find which root folder this path belongs to
    QString rootFolder;
    for (const QString &root : m_watchedFolders) {
        if (path.startsWith(root)) {
            rootFolder = root;
            break;
        }
    }
    
    if (rootFolder.isEmpty()) {
        // Path doesn't belong to any watched folder, use path itself
        rootFolder = path;
    }
    
    m_pendingRoots.insert(rootFolder);
    m_needsWatchRefresh = true;  // Directory changed, may have new subdirs
    m_debounceTimer->start();
    
    // Re-add this directory to watcher (it may have been removed if deleted/recreated)
    if (!m_watcher->directories().contains(path)) {
        m_watcher->addPath(path);
    }
}

void LibraryWatcher::onDebounceTimeout()
{
    processPendingRoots();
}

void LibraryWatcher::processPendingRoots()
{
    if (m_pendingRoots.isEmpty())
        return;
    
    if (m_scanner->isScanning()) {
        // Stay queued, will be processed after scan finishes
        return;
    }
    
    QStringList roots = m_pendingRoots.values();
    m_pendingRoots.clear();
    
    qDebug() << "LibraryWatcher: Scanning" << roots.size() << "changed folders";
    m_scanner->scanFolders(roots, true);  // detectDeletions=true
}

void LibraryWatcher::onScanFinished()
{
    // Process any pending roots that accumulated during the scan
    if (!m_pendingRoots.isEmpty()) {
        QTimer::singleShot(500, this, &LibraryWatcher::processPendingRoots);
    }
    
    // Only refresh watches if directory changes were detected (may have new subdirs)
    if (m_needsWatchRefresh) {
        m_needsWatchRefresh = false;
        QTimer::singleShot(1000, this, [this]() {
            if (m_enabled && !m_watchedFolders.isEmpty()) {
                setWatchFolders(m_watchedFolders);
            }
        });
    }
}

void LibraryWatcher::onPeriodicRescanTimeout()
{
    if (!m_enabled || m_watchedFolders.isEmpty())
        return;
    
    if (m_scanner->isScanning())
        return;
    
    qDebug() << "LibraryWatcher: Periodic rescan triggered";
    m_scanner->scanFolders(m_watchedFolders, true);  // detectDeletions=true
}

void LibraryWatcher::setPeriodicRescanMinutes(int minutes)
{
    if (minutes <= 0) {
        m_periodicRescanTimer->stop();
        qDebug() << "LibraryWatcher: Periodic rescan disabled";
    } else {
        m_periodicRescanTimer->setInterval(minutes * 60 * 1000);
        if (m_enabled) {
            m_periodicRescanTimer->start();
            qDebug() << "LibraryWatcher: Periodic rescan set to" << minutes << "minutes";
        }
    }
}

void LibraryWatcher::setEnabled(bool enabled)
{
    m_enabled = enabled;
    
    if (enabled) {
        setWatchFolders(m_watchedFolders);
        if (m_periodicRescanTimer->interval() > 0) {
            m_periodicRescanTimer->start();
        }
        qDebug() << "LibraryWatcher: Enabled";
    } else {
        if (!m_watcher->directories().isEmpty())
            m_watcher->removePaths(m_watcher->directories());
        m_periodicRescanTimer->stop();
        m_pendingRoots.clear();
        qDebug() << "LibraryWatcher: Disabled";
    }
}
