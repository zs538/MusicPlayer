#ifndef LIBRARYSCANNER_H
#define LIBRARYSCANNER_H

#include <QObject>
#include <QThread>
#include <QStringList>
#include <atomic>
#include "LibraryDatabase.h"

class LibraryScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY totalFilesChanged)

public:
    explicit LibraryScanner(LibraryDatabase *db, QObject *parent = nullptr);
    ~LibraryScanner();
    
    bool isScanning() const { return m_scanning; }
    int progress() const { return m_progress; }
    int totalFiles() const { return m_totalFiles; }

public slots:
    void scanFolder(const QString &path);
    void scanFolders(const QStringList &paths, bool detectDeletions = false);
    void rescanFiles(const QStringList &filePaths);
    void rescanAll();
    void cancelScan();

signals:
    void scanningChanged(bool scanning);
    void progressChanged(int progress);
    void totalFilesChanged(int total);
    void scanStarted();
    void scanFinished();
    void scanCancelled();
    void errorOccurred(const QString &message);

private:
    void startScan(const QStringList &paths, bool detectDeletions, bool inputPathsAreFiles);
    void doScan(const QStringList &paths, bool detectDeletions, bool inputPathsAreFiles);
    QStringList collectAudioFiles(const QString &path);
    bool isAudioFile(const QString &path) const;
    
    LibraryDatabase *m_db;
    QThread *m_scanThread = nullptr;
    std::atomic<bool> m_scanning{false};
    std::atomic<bool> m_cancelRequested{false};
    int m_progress = 0;
    int m_totalFiles = 0;
    
    static const QStringList s_audioExtensions;
};

#endif // LIBRARYSCANNER_H
