#ifndef COVERIMAGEPROVIDER_H
#define COVERIMAGEPROVIDER_H

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QCache>
#include <QMutex>
#include <QThreadPool>
#include <QRunnable>
#include <QStringList>

/**
 * @brief Async QQuickImageProvider for cover art images.
 * 
 * Provides cover art for albums/artists by:
 * 1. Looking for embedded cover art in audio files
 * 2. Looking for cover art files in the folder (cover.jpg, folder.jpg, etc.)
 * 
 * Uses async loading to avoid blocking the render thread during window resize/move.
 * 
 * Usage in QML: Image { source: "image://cover/<filePath>" }
 */
class CoverImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit CoverImageProvider();
    
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    
    static void setInstance(CoverImageProvider *provider);
    static CoverImageProvider *instance();
    
    // Configurable cover patterns (thread-safe)
    static void setCoverPatterns(const QStringList &patterns);
    static QStringList coverPatterns();
    
    // Static methods for worker thread access
    static QString sourceForFilePath(const QString &filePath);
    static QString sourceForFilePaths(const QStringList &filePaths);
    static QString decodeFilePathId(const QString &id);
    static QStringList decodeFilePathIds(const QString &id);
    static QImage loadCoverForPath(const QString &filePath, const QSize &requestedSize);
    static QImage loadEmbeddedCover(const QString &filePath);
    static QImage loadFolderCover(const QString &folderPath, const QString &album = {}, const QString &artist = {});
    static QImage getCached(const QString &key);
    static void putCached(const QString &key, const QImage &image);
    static QImage getOriginalCached(const QString &key);
    static void putOriginalCached(const QString &key, const QImage &image);
    static void clearCache();

private:
    QThreadPool m_threadPool;
    
    static QCache<QString, QImage> s_cache;
    static QMutex s_cacheMutex;
    static QCache<QString, QImage> s_originalCache;
    static QMutex s_originalCacheMutex;
    static QStringList s_coverPatterns;
    static QMutex s_patternsMutex;
    static CoverImageProvider *s_instance;
};

/**
 * @brief Async response for cover image loading.
 */
class CoverImageResponse : public QQuickImageResponse, public QRunnable
{
    Q_OBJECT
public:
    CoverImageResponse(const QString &id, const QSize &requestedSize);
    QQuickTextureFactory *textureFactory() const override;
    void run() override;

private:
    QString m_id;
    QSize m_requestedSize;
    QImage m_image;
};

#endif // COVERIMAGEPROVIDER_H
