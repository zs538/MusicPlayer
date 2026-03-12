#include "CoverArtProvider.h"

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDebug>
#include <QRegularExpression>

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/mp4coverart.h>

CoverArtProvider *CoverArtProvider::s_instance = nullptr;

CoverArtProvider::CoverArtProvider(QObject *parent)
    : QObject(parent)
    , m_cache(100)
{
    s_instance = this;
    
    // Default cover patterns - user can customize
    m_coverPatterns = QStringList{
        "cover.jpg",
        "cover.jpeg",
        "cover.png",
        "folder.jpg",
        "folder.png",
        "front.jpg",
        "front.png",
        "%album%.jpg",
        "%album%.png",
        "*.jpg",
        "*.png"
    };
}

CoverArtProvider *CoverArtProvider::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new CoverArtProvider();
    }
    return s_instance;
}

CoverArtProvider *CoverArtProvider::instance()
{
    return s_instance;
}

void CoverArtProvider::setCoverPatterns(const QStringList &patterns)
{
    if (m_coverPatterns != patterns) {
        m_coverPatterns = patterns;
        m_cache.clear();  // Invalidate cache when patterns change
        emit coverPatternsChanged();
    }
}

QString CoverArtProvider::coverUrlForFile(const QString &filePath, const QString &album, const QString &artist)
{
    if (filePath.isEmpty()) {
        return QString();
    }
    
    // Check cache first
    if (m_cache.contains(filePath)) {
        return *m_cache.object(filePath);
    }
    
    QString coverUrl;
    
    // Try embedded cover first
    coverUrl = extractEmbeddedCover(filePath);
    
    // Fall back to folder-based cover
    if (coverUrl.isEmpty()) {
        coverUrl = findFolderCover(filePath, album, artist);
    }
    
    // Cache the result (even if empty to avoid repeated lookups)
    m_cache.insert(filePath, new QString(coverUrl));
    
    return coverUrl;
}

QString CoverArtProvider::extractEmbeddedCover(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return QString();
    }
    
    QString cacheDir = getCacheDir();
    QString hash = hashFilePath(filePath);
    QString cachedPath = cacheDir + "/" + hash + ".jpg";
    
    // Check if already extracted
    if (QFileInfo::exists(cachedPath)) {
        return QUrl::fromLocalFile(cachedPath).toString();
    }
    
    QImage coverImage;
    TagLib::FileName fileName(filePath.toUtf8().constData());
    
    // FLAC files
    if (filePath.endsWith(".flac", Qt::CaseInsensitive)) {
        TagLib::FLAC::File flacFile(fileName);
        if (flacFile.isValid() && !flacFile.pictureList().isEmpty()) {
            const TagLib::List<TagLib::FLAC::Picture*>& pictures = flacFile.pictureList();
            for (auto it = pictures.begin(); it != pictures.end(); ++it) {
                TagLib::FLAC::Picture* picture = *it;
                if (picture && picture->type() == TagLib::FLAC::Picture::FrontCover) {
                    coverImage = QImage::fromData(
                        reinterpret_cast<const uchar*>(picture->data().data()),
                        picture->data().size()
                    );
                    if (!coverImage.isNull()) break;
                }
            }
            // If no front cover, try any picture
            if (coverImage.isNull() && !pictures.isEmpty()) {
                TagLib::FLAC::Picture* picture = pictures.front();
                if (picture) {
                    coverImage = QImage::fromData(
                        reinterpret_cast<const uchar*>(picture->data().data()),
                        picture->data().size()
                    );
                }
            }
        }
    }
    // MP3 files with ID3v2 tags
    else if (filePath.endsWith(".mp3", Qt::CaseInsensitive)) {
        TagLib::MPEG::File mpegFile(fileName);
        if (mpegFile.isValid() && mpegFile.ID3v2Tag()) {
            TagLib::ID3v2::Tag* tag = mpegFile.ID3v2Tag();
            TagLib::ID3v2::FrameList frames = tag->frameList("APIC");
            for (auto it = frames.begin(); it != frames.end(); ++it) {
                TagLib::ID3v2::AttachedPictureFrame* frame = 
                    static_cast<TagLib::ID3v2::AttachedPictureFrame*>(*it);
                if (frame) {
                    coverImage = QImage::fromData(
                        reinterpret_cast<const uchar*>(frame->picture().data()),
                        frame->picture().size()
                    );
                    if (!coverImage.isNull()) break;
                }
            }
        }
    }
    // MP4/M4A files
    else if (filePath.endsWith(".mp4", Qt::CaseInsensitive) || 
             filePath.endsWith(".m4a", Qt::CaseInsensitive)) {
        TagLib::MP4::File mp4File(fileName);
        if (mp4File.isValid() && mp4File.tag()) {
            TagLib::MP4::Tag* tag = mp4File.tag();
            if (tag->itemMap().contains("covr")) {
                TagLib::MP4::Item coverItem = tag->itemMap()["covr"];
                TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
                if (!coverArtList.isEmpty()) {
                    TagLib::MP4::CoverArt coverArt = coverArtList.front();
                    coverImage = QImage::fromData(
                        reinterpret_cast<const uchar*>(coverArt.data().data()),
                        coverArt.data().size()
                    );
                }
            }
        }
    }
    
    if (!coverImage.isNull()) {
        // Save to cache
        QDir().mkpath(cacheDir);
        if (coverImage.save(cachedPath, "JPEG", 90)) {
            return QUrl::fromLocalFile(cachedPath).toString();
        }
    }
    
    return QString();
}

QString CoverArtProvider::findFolderCover(const QString &filePath, const QString &album, const QString &artist)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    
    if (!dir.exists()) {
        return QString();
    }
    
    // Get all image files in directory
    QStringList imageFilters = {"*.jpg", "*.jpeg", "*.png", "*.gif", "*.webp", "*.bmp"};
    QStringList imageFiles = dir.entryList(imageFilters, QDir::Files, QDir::Name);
    
    if (imageFiles.isEmpty()) {
        return QString();
    }
    
    // Try each pattern in order
    for (const QString &pattern : m_coverPatterns) {
        QString expandedPattern = expandPattern(pattern, album, artist);
        
        for (const QString &imageFile : imageFiles) {
            if (matchesPattern(imageFile, expandedPattern, album, artist)) {
                return QUrl::fromLocalFile(dir.absoluteFilePath(imageFile)).toString();
            }
        }
    }
    
    return QString();
}

void CoverArtProvider::clearCache()
{
    m_cache.clear();
    
    // Also clear disk cache
    QString cacheDir = getCacheDir();
    QDir dir(cacheDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

QString CoverArtProvider::getCacheDir() const
{
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cacheLocation + "/covers";
}

QString CoverArtProvider::hashFilePath(const QString &filePath) const
{
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

bool CoverArtProvider::matchesPattern(const QString &filename, const QString &pattern, const QString &album, const QString &artist) const
{
    Q_UNUSED(album)
    Q_UNUSED(artist)
    
    // Simple wildcard matching
    if (pattern.contains('*')) {
        QRegularExpression regex(
            "^" + QRegularExpression::escape(pattern)
                .replace("\\*", ".*")
                .replace("\\?", ".") + "$",
            QRegularExpression::CaseInsensitiveOption
        );
        return regex.match(filename).hasMatch();
    }
    
    // Exact match (case insensitive)
    return filename.compare(pattern, Qt::CaseInsensitive) == 0;
}

QString CoverArtProvider::expandPattern(const QString &pattern, const QString &album, const QString &artist) const
{
    QString result = pattern;
    
    // Replace placeholders
    if (!album.isEmpty()) {
        result.replace("%album%", album, Qt::CaseInsensitive);
    }
    if (!artist.isEmpty()) {
        result.replace("%artist%", artist, Qt::CaseInsensitive);
    }
    
    return result;
}
