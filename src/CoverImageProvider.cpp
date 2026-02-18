#include "CoverImageProvider.h"
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <QRegularExpression>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4coverart.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacpicture.h>

// Static members
CoverImageProvider *CoverImageProvider::s_instance = nullptr;
QCache<QString, QImage> CoverImageProvider::s_cache(300);
QMutex CoverImageProvider::s_cacheMutex;
QStringList CoverImageProvider::s_coverPatterns = {
    "cover.jpg", "cover.jpeg", "cover.png",
    "folder.jpg", "folder.png",
    "front.jpg", "front.png",
    "%album%.jpg", "%album%.png",
    "*.jpg", "*.png"
};
QMutex CoverImageProvider::s_patternsMutex;

CoverImageProvider::CoverImageProvider()
    : QQuickAsyncImageProvider()
{
    m_threadPool.setMaxThreadCount(2); // Limit concurrent image loading
}

void CoverImageProvider::setInstance(CoverImageProvider *provider)
{
    s_instance = provider;
}

CoverImageProvider *CoverImageProvider::instance()
{
    return s_instance;
}

void CoverImageProvider::setCoverPatterns(const QStringList &patterns)
{
    QMutexLocker locker(&s_patternsMutex);
    s_coverPatterns = patterns;
    // Invalidate cache when patterns change
    QMutexLocker cacheLocker(&s_cacheMutex);
    s_cache.clear();
}

QStringList CoverImageProvider::coverPatterns()
{
    QMutexLocker locker(&s_patternsMutex);
    return s_coverPatterns;
}

QQuickImageResponse *CoverImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    CoverImageResponse *response = new CoverImageResponse(id, requestedSize);
    response->setAutoDelete(false);
    m_threadPool.start(response);
    return response;
}

QImage CoverImageProvider::getCached(const QString &key)
{
    QMutexLocker locker(&s_cacheMutex);
    if (QImage *cached = s_cache.object(key)) {
        return *cached;
    }
    return QImage();
}

void CoverImageProvider::putCached(const QString &key, const QImage &image)
{
    QMutexLocker locker(&s_cacheMutex);
    s_cache.insert(key, new QImage(image));
}

void CoverImageProvider::clearCache()
{
    QMutexLocker locker(&s_cacheMutex);
    s_cache.clear();
}

QImage CoverImageProvider::loadCoverForPath(const QString &filePath, const QSize &)
{
    // First try embedded cover
    QImage cover = loadEmbeddedCover(filePath);
    
    // If no embedded cover, try folder cover
    if (cover.isNull()) {
        QFileInfo fi(filePath);
        cover = loadFolderCover(fi.absolutePath());
    }
    
    // Note: Scaling is done in CoverImageResponse::run() after loading
    return cover;
}

static QImage imageFromData(const char *data, unsigned int size)
{
    QImage img;
    img.loadFromData(reinterpret_cast<const uchar*>(data), size);
    return img;
}

QImage CoverImageProvider::loadEmbeddedCover(const QString &filePath)
{
    QByteArray pathBytes = filePath.toUtf8();
    TagLib::FileRef fileRef(pathBytes.constData());
    
    if (fileRef.isNull()) {
        return QImage();
    }
    
    // Try MPEG/ID3v2
    if (auto *mpegFile = dynamic_cast<TagLib::MPEG::File*>(fileRef.file())) {
        if (auto *id3v2 = mpegFile->ID3v2Tag()) {
            auto frames = id3v2->frameList("APIC");
            if (!frames.isEmpty()) {
                auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                if (pic) {
                    return imageFromData(pic->picture().data(), pic->picture().size());
                }
            }
        }
    }
    
    // Try FLAC — prefer FrontCover, fall back to any picture
    if (auto *flacFile = dynamic_cast<TagLib::FLAC::File*>(fileRef.file())) {
        const auto &pics = flacFile->pictureList();
        if (!pics.isEmpty()) {
            for (auto *pic : pics) {
                if (pic && pic->type() == TagLib::FLAC::Picture::FrontCover) {
                    QImage img = imageFromData(pic->data().data(), pic->data().size());
                    if (!img.isNull()) return img;
                }
            }
            auto *pic = pics.front();
            if (pic) return imageFromData(pic->data().data(), pic->data().size());
        }
    }
    
    // Try MP4/M4A
    if (auto *mp4File = dynamic_cast<TagLib::MP4::File*>(fileRef.file())) {
        if (auto *tag = mp4File->tag()) {
            if (tag->itemMap().contains("covr")) {
                auto coverArtList = tag->itemMap()["covr"].toCoverArtList();
                if (!coverArtList.isEmpty()) {
                    auto coverArt = coverArtList.front();
                    return imageFromData(coverArt.data().data(), coverArt.data().size());
                }
            }
        }
    }
    
    // Try Ogg Opus
    if (auto *opusFile = dynamic_cast<TagLib::Ogg::Opus::File*>(fileRef.file())) {
        if (auto *tag = opusFile->tag()) {
            auto pics = tag->pictureList();
            if (!pics.isEmpty()) {
                return imageFromData(pics.front()->data().data(), pics.front()->data().size());
            }
        }
    }
    
    // Try Ogg Vorbis
    if (auto *vorbisFile = dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileRef.file())) {
        if (auto *tag = vorbisFile->tag()) {
            auto pics = tag->pictureList();
            if (!pics.isEmpty()) {
                return imageFromData(pics.front()->data().data(), pics.front()->data().size());
            }
        }
    }
    
    return QImage();
}

QImage CoverImageProvider::loadFolderCover(const QString &folderPath, const QString &album, const QString &artist)
{
    QDir dir(folderPath);
    if (!dir.exists()) return QImage();
    
    // Get all image files in directory
    QStringList imageFiles = dir.entryList({"*.jpg", "*.jpeg", "*.png", "*.gif", "*.webp", "*.bmp"}, QDir::Files, QDir::Name);
    if (imageFiles.isEmpty()) return QImage();
    
    // Get current patterns (thread-safe copy)
    QStringList patterns = coverPatterns();
    
    // Try each pattern in priority order
    for (const QString &pattern : patterns) {
        // Expand %album% / %artist% placeholders
        QString expanded = pattern;
        if (!album.isEmpty()) expanded.replace("%album%", album, Qt::CaseInsensitive);
        if (!artist.isEmpty()) expanded.replace("%artist%", artist, Qt::CaseInsensitive);
        
        // Skip patterns with unexpanded placeholders
        if (expanded.contains('%')) continue;
        
        for (const QString &imageFile : imageFiles) {
            bool match = false;
            if (expanded.contains('*')) {
                QRegularExpression regex(
                    "^" + QRegularExpression::escape(expanded).replace("\\*", ".*") + "$",
                    QRegularExpression::CaseInsensitiveOption);
                match = regex.match(imageFile).hasMatch();
            } else {
                match = imageFile.compare(expanded, Qt::CaseInsensitive) == 0;
            }
            if (match) {
                QImage img(dir.filePath(imageFile));
                if (!img.isNull()) return img;
            }
        }
    }
    
    return QImage();
}

// CoverImageResponse implementation

CoverImageResponse::CoverImageResponse(const QString &id, const QSize &requestedSize)
    : m_id(id)
    , m_requestedSize(requestedSize)
{
}

QQuickTextureFactory *CoverImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void CoverImageResponse::run()
{
    try {
        // Build cache key including size
        QString cacheKey = m_id;
        if (m_requestedSize.isValid() && m_requestedSize.width() > 0) {
            cacheKey = QString("%1@%2x%3").arg(m_id).arg(m_requestedSize.width()).arg(m_requestedSize.height());
        }
        
        // Check cache first
        m_image = CoverImageProvider::getCached(cacheKey);
        if (!m_image.isNull()) {
            emit finished();
            return;
        }
        
        // Load the image (runs on worker thread, not blocking render)
        if (!m_id.isEmpty() && QFileInfo::exists(m_id)) {
            m_image = CoverImageProvider::loadCoverForPath(m_id, m_requestedSize);
        }
        
        // Create placeholder if no image found
        if (m_image.isNull()) {
            int w = m_requestedSize.width() > 0 ? m_requestedSize.width() : 96;
            int h = m_requestedSize.height() > 0 ? m_requestedSize.height() : 96;
            m_image = QImage(w, h, QImage::Format_ARGB32);
            m_image.fill(Qt::transparent);
        } else {
            // Scale if needed - use SmoothTransformation for quality
            if (m_requestedSize.isValid() && m_requestedSize.width() > 0 && m_requestedSize.height() > 0) {
                m_image = m_image.scaled(m_requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
        
        // Cache the result
        if (!cacheKey.isEmpty()) {
            CoverImageProvider::putCached(cacheKey, m_image);
        }
    } catch (...) {
        // Catch any exceptions from TagLib or image processing
        qWarning() << "CoverImageResponse: Exception loading cover for" << m_id;
        int w = m_requestedSize.width() > 0 ? m_requestedSize.width() : 96;
        int h = m_requestedSize.height() > 0 ? m_requestedSize.height() : 96;
        m_image = QImage(w, h, QImage::Format_ARGB32);
        m_image.fill(Qt::transparent);
    }
    
    emit finished();
}
