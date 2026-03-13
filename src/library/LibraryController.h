#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVariantList>

class LibraryDatabase;
class LibraryScanner;
class LibraryWatcher;

class LibraryController : public QObject
{
    Q_OBJECT

public:
    explicit LibraryController(QObject *parent = nullptr);
    ~LibraryController();

    // Must be called after construction to open DB and wire settings
    void initialize();

    // Component accessors (composition root needs these for cross-subsystem wiring)
    LibraryDatabase *database() const { return m_database; }

    // Folder management (single authority — collapses former add/removeLibraryFolder + add/removeWatchFolder)
    void addLibraryFolder(const QString &path);
    void removeLibraryFolder(const QString &path);
    QStringList libraryFolders() const;

    // Scanning
    void rescanLibrary();
    void rescanCollectionEntry(const QVariantList &filter, const QString &entryType,
                               const QString &groupType, const QVariant &groupValue,
                               const QString &filePath);
    void rescanFiles(const QStringList &filePaths);

    // State queries
    bool isScanning() const;
    int scanProgress() const;
    int trackCount() const;

signals:
    void scanningChanged();
    void scanProgressChanged();
    void libraryFoldersChanged();
    void trackCountChanged();
    void scanFinished();

private:
    void connectWatcherSettings();

    LibraryDatabase *m_database = nullptr;
    LibraryScanner *m_scanner = nullptr;
    LibraryWatcher *m_watcher = nullptr;
};

#endif // LIBRARYCONTROLLER_H
