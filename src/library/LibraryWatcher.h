#ifndef LIBRARYWATCHER_H
#define LIBRARYWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>

class LibraryScanner;

class LibraryWatcher : public QObject
{
    Q_OBJECT

public:
    explicit LibraryWatcher(LibraryScanner *scanner, QObject *parent = nullptr);
    
    void setWatchFolders(const QStringList &folders);
    void setEnabled(bool enabled);
    void setPeriodicRescanMinutes(int minutes);  // 0 = disabled
    
signals:
    void watcherError(const QString &path, const QString &reason);
    
private slots:
    void onDirectoryChanged(const QString &path);
    void onDebounceTimeout();
    void onPeriodicRescanTimeout();
    void onScanFinished();
    
private:
    void processPendingRoots();
    
    LibraryScanner *m_scanner;
    QFileSystemWatcher *m_watcher;
    QTimer *m_debounceTimer;
    QTimer *m_periodicRescanTimer;
    QSet<QString> m_pendingRoots;
    QStringList m_watchedFolders;
    bool m_enabled = true;
    bool m_needsWatchRefresh = false;  // Only refresh watches when directory changes detected
    
    static constexpr int DEBOUNCE_MS = 3000;
};

#endif // LIBRARYWATCHER_H
