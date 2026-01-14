#ifndef COVERARTPROVIDER_H
#define COVERARTPROVIDER_H

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QCache>
#include <QImage>
#include <QUrl>

class CoverArtProvider : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    
    Q_PROPERTY(QStringList coverPatterns READ coverPatterns WRITE setCoverPatterns NOTIFY coverPatternsChanged)

public:
    explicit CoverArtProvider(QObject *parent = nullptr);
    
    static CoverArtProvider *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static CoverArtProvider *instance();
    
    QStringList coverPatterns() const { return m_coverPatterns; }
    void setCoverPatterns(const QStringList &patterns);
    
    // Get cover art URL for a file - returns file:// URL or empty string
    Q_INVOKABLE QString coverUrlForFile(const QString &filePath, const QString &album = QString(), const QString &artist = QString());
    
    // Extract embedded cover art from audio file, save to cache, return path
    QString extractEmbeddedCover(const QString &filePath);
    
    // Find folder-based cover art
    QString findFolderCover(const QString &filePath, const QString &album, const QString &artist);
    
    // Clear the cache
    Q_INVOKABLE void clearCache();

signals:
    void coverPatternsChanged();

private:
    QString getCacheDir() const;
    QString hashFilePath(const QString &filePath) const;
    bool matchesPattern(const QString &filename, const QString &pattern, const QString &album, const QString &artist) const;
    QString expandPattern(const QString &pattern, const QString &album, const QString &artist) const;
    
    QStringList m_coverPatterns;
    QCache<QString, QString> m_cache;  // filePath -> coverUrl
    
    static CoverArtProvider *s_instance;
};

#endif // COVERARTPROVIDER_H
