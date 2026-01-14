#include "CoverImageProvider.h"
#include "library/LibraryDatabase.h"
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacpicture.h>

// Static members
CoverImageProvider *CoverImageProvider::s_instance = nullptr;
QCache<QString, QImage> CoverImageProvider::s_cache(300);
QMutex CoverImageProvider::s_cacheMutex;

CoverImageProvider::CoverImageProvider(LibraryDatabase *db)
    : QQuickAsyncImageProvider()
    , m_db(db)
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
                    QImage img;
                    img.loadFromData(reinterpret_cast<const uchar*>(pic->picture().data()), 
                                     pic->picture().size());
                    return img;
                }
            }
        }
    }
    
    // Try FLAC
    if (auto *flacFile = dynamic_cast<TagLib::FLAC::File*>(fileRef.file())) {
        auto pics = flacFile->pictureList();
        if (!pics.isEmpty()) {
            auto *pic = pics.front();
            QImage img;
            img.loadFromData(reinterpret_cast<const uchar*>(pic->data().data()), 
                             pic->data().size());
            return img;
        }
    }
    
    // Try Ogg Opus
    if (auto *opusFile = dynamic_cast<TagLib::Ogg::Opus::File*>(fileRef.file())) {
        if (auto *tag = opusFile->tag()) {
            auto pics = tag->pictureList();
            if (!pics.isEmpty()) {
                auto *pic = pics.front();
                QImage img;
                img.loadFromData(reinterpret_cast<const uchar*>(pic->data().data()), 
                                 pic->data().size());
                return img;
            }
        }
    }
    
    // Try Ogg Vorbis
    if (auto *vorbisFile = dynamic_cast<TagLib::Ogg::Vorbis::File*>(fileRef.file())) {
        if (auto *tag = vorbisFile->tag()) {
            auto pics = tag->pictureList();
            if (!pics.isEmpty()) {
                auto *pic = pics.front();
                QImage img;
                img.loadFromData(reinterpret_cast<const uchar*>(pic->data().data()), 
                                 pic->data().size());
                return img;
            }
        }
    }
    
    return QImage();
}

QImage CoverImageProvider::loadFolderCover(const QString &folderPath)
{
    QDir dir(folderPath);
    
    // Common cover art filenames
    static const QStringList coverNames = {
        "cover.jpg", "cover.jpeg", "cover.png",
        "folder.jpg", "folder.jpeg", "folder.png",
        "front.jpg", "front.jpeg", "front.png",
        "album.jpg", "album.jpeg", "album.png",
        "Cover.jpg", "Cover.jpeg", "Cover.png",
        "Folder.jpg", "Folder.jpeg", "Folder.png"
    };
    
    for (const QString &name : coverNames) {
        QString path = dir.filePath(name);
        if (QFileInfo::exists(path)) {
            QImage img(path);
            if (!img.isNull()) {
                return img;
            }
        }
    }
    
    // Try any image file in the folder
    QStringList imageFilters = {"*.jpg", "*.jpeg", "*.png", "*.gif", "*.bmp"};
    QStringList images = dir.entryList(imageFilters, QDir::Files);
    if (!images.isEmpty()) {
        QImage img(dir.filePath(images.first()));
        if (!img.isNull()) {
            return img;
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
    if (QFileInfo::exists(m_id)) {
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
    CoverImageProvider::putCached(cacheKey, m_image);
    
    emit finished();
}
