#include "MetadataReader.h"
#include "TrackMetadata.h"

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>
#include <taglib/tpropertymap.h>
#include <taglib/flacfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <QDebug>
#include <QImage>

MetadataReader::MetadataReader(QObject* parent)
    : QObject(parent)
{
}

TrackMetadata* MetadataReader::readMetadataStandalone(const QUrl& url, QObject* parent)
{
    if (!url.isValid() || url.isEmpty() || !url.isLocalFile()) {
        return nullptr;
    }
    
    return readFromTagLib(url.toLocalFile(), parent);
}

TrackMetadata* MetadataReader::readMetadata(const QUrl& url)
{
    return readMetadataStandalone(url, this);
}

bool MetadataReader::hasMetadata(const QUrl& url)
{
    TrackMetadata* metadata = readMetadata(url);
    bool hasData = metadata && metadata->hasMetadata();
    if (metadata) {
        metadata->deleteLater();
    }
    return hasData;
}

TrackMetadata* MetadataReader::readFromTagLib(const QString& filePath, QObject* parent)
{
    TrackMetadata* metadata = new TrackMetadata(parent);
    
    // Convert QString to TagLib::FileName (UTF-8 encoded)
    TagLib::FileName fileName(filePath.toUtf8().constData());
    
    // Try format-specific readers first for better compatibility
    TagLib::File* file = nullptr;
    
    if (filePath.endsWith(".flac", Qt::CaseInsensitive)) {
        file = new TagLib::FLAC::File(fileName);
    } else if (filePath.endsWith(".opus", Qt::CaseInsensitive)) {
        file = new TagLib::Ogg::Opus::File(fileName);
    } else if (filePath.endsWith(".ogg", Qt::CaseInsensitive)) {
        file = new TagLib::Ogg::Vorbis::File(fileName);
    } else if (filePath.endsWith(".mp4", Qt::CaseInsensitive) || 
               filePath.endsWith(".m4a", Qt::CaseInsensitive)) {
        file = new TagLib::MP4::File(fileName);
    } else if (filePath.endsWith(".mp3", Qt::CaseInsensitive)) {
        file = new TagLib::MPEG::File(fileName);
    } else {
        // Fallback to generic FileRef
        TagLib::FileRef fileRef(fileName);
        if (fileRef.isNull()) {
            qDebug() << "FileRef is null for:" << filePath;
            return metadata;
        }
        return extractFromGeneric(fileRef, metadata, filePath);
    }
    
    if (!file || !file->isValid()) {
        delete file;
        return metadata;
    }
    
    // Extract metadata from properties
    TagLib::PropertyMap properties = file->properties();
    extractFromProperties(properties, metadata);
    
    // Extract cover art (format-specific)
    extractCoverArt(file, filePath, metadata);
    
    // Get audio properties
    if (file->audioProperties()) {
        TagLib::AudioProperties* props = file->audioProperties();
        metadata->m_duration = props->lengthInMilliseconds();
    }
    
    delete file;
    return metadata;
}

void MetadataReader::extractFromProperties(const TagLib::PropertyMap& properties, TrackMetadata* metadata)
{
    // Extract title
    if (properties.contains("TITLE")) {
        QString title = tagLibStringToQString(properties["TITLE"].front());
        metadata->m_title = title.isEmpty() ? "Unknown Title" : title;
    } else {
        metadata->m_title = "Unknown Title";
    }
    
    // Extract artist - try multiple keys
    QString artist;
    if (properties.contains("ARTIST")) {
        artist = tagLibStringToQString(properties["ARTIST"].front());
    } else if (properties.contains("ALBUMARTIST")) {
        artist = tagLibStringToQString(properties["ALBUMARTIST"].front());
    } else if (properties.contains("PERFORMER")) {
        artist = tagLibStringToQString(properties["PERFORMER"].front());
    }
    metadata->m_artist = artist.isEmpty() ? "Unknown Artist" : artist;
    
    // Extract album
    if (properties.contains("ALBUM")) {
        metadata->m_album = tagLibStringToQString(properties["ALBUM"].front());
    }
    
    // Extract genre
    if (properties.contains("GENRE")) {
        metadata->m_genre = tagLibStringToQString(properties["GENRE"].front());
    }
    
    // Extract year/date - try multiple keys
    if (properties.contains("DATE")) {
        QString dateStr = tagLibStringToQString(properties["DATE"].front());
        // Extract year from date string (YYYY-MM-DD or just YYYY)
        if (dateStr.length() >= 4) {
            metadata->m_year = dateStr.left(4);
        }
    } else if (properties.contains("YEAR")) {
        metadata->m_year = tagLibStringToQString(properties["YEAR"].front());
    }
    
    // Extract track number
    if (properties.contains("TRACKNUMBER")) {
        QString trackStr = tagLibStringToQString(properties["TRACKNUMBER"].front());
        // Handle "3/12" format
        int slashPos = trackStr.indexOf('/');
        if (slashPos > 0) {
            metadata->m_trackNumber = trackStr.left(slashPos).toInt();
        } else {
            metadata->m_trackNumber = trackStr.toInt();
        }
    }
}

void MetadataReader::extractCoverArt(TagLib::File* file, const QString& filePath, TrackMetadata* metadata)
{
    if (!file || !metadata) return;
    
    QImage coverImage;
    
    // FLAC files
    if (filePath.endsWith(".flac", Qt::CaseInsensitive)) {
        TagLib::FLAC::File* flacFile = static_cast<TagLib::FLAC::File*>(file);
        if (flacFile && !flacFile->pictureList().isEmpty()) {
            const TagLib::List<TagLib::FLAC::Picture*>& pictures = flacFile->pictureList();
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
        }
    }
    // MP3 files with ID3v2 tags
    else if (filePath.endsWith(".mp3", Qt::CaseInsensitive)) {
        TagLib::MPEG::File* mpegFile = static_cast<TagLib::MPEG::File*>(file);
        if (mpegFile && mpegFile->ID3v2Tag()) {
            TagLib::ID3v2::Tag* tag = mpegFile->ID3v2Tag();
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
        TagLib::MP4::File* mp4File = static_cast<TagLib::MP4::File*>(file);
        if (mp4File && mp4File->tag()) {
            TagLib::MP4::Tag* tag = mp4File->tag();
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
    
    // Set the cover art if found
    if (!coverImage.isNull()) {
        metadata->m_coverArt = coverImage;
    }
}

TrackMetadata* MetadataReader::extractFromGeneric(TagLib::FileRef& fileRef, TrackMetadata* metadata, const QString& filePath)
{
    TagLib::PropertyMap properties = fileRef.file()->properties();
    extractFromProperties(properties, metadata);
    
    // Try to extract cover art from generic file if possible
    extractCoverArt(fileRef.file(), filePath, metadata);
    
    if (fileRef.audioProperties()) {
        TagLib::AudioProperties* props = fileRef.audioProperties();
        metadata->m_duration = props->lengthInMilliseconds();
    }
    
    return metadata;
}

QString MetadataReader::tagLibStringToQString(const TagLib::String& str)
{
    return QString::fromUtf8(str.to8Bit(true));
}
