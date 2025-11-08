#include "TrackMetadata.h"

#include <QBuffer>
#include <QImageWriter>

TrackMetadata::TrackMetadata(QObject* parent)
    : QObject(parent)
{
}

TrackMetadata* TrackMetadata::fromMediaMetaData(const QMediaMetaData& metaData, QObject* parent)
{
    TrackMetadata* metadata = new TrackMetadata(parent);
    metadata->loadFromMediaMetaData(metaData);
    return metadata;
}

void TrackMetadata::loadFromMediaMetaData(const QMediaMetaData& metaData)
{
    clear();
    
    if (metaData.isEmpty()) return;
    
    // Load title with multiple fallbacks
    m_title = metaData.stringValue(QMediaMetaData::Title);
    if (m_title.isEmpty()) {
        m_title = "Unknown Title";
    }
    
    // Load artist with multiple fallbacks
    m_artist = metaData.stringValue(QMediaMetaData::Author);
    if (m_artist.isEmpty()) {
        m_artist = metaData.stringValue(QMediaMetaData::AlbumArtist);
    }
    if (m_artist.isEmpty()) {
        m_artist = metaData.stringValue(QMediaMetaData::Composer);
    }
    if (m_artist.isEmpty()) {
        m_artist = "Unknown Artist";
    }
    
    // Load album
    m_album = metaData.stringValue(QMediaMetaData::AlbumTitle);
    
    // Load genre
    m_genre = metaData.stringValue(QMediaMetaData::Genre);
    
    // Load year/date
    m_year = metaData.stringValue(QMediaMetaData::Date);
    
    // Extract year from date if it's a full date string
    if (!m_year.isEmpty() && m_year.length() > 4) {
        m_year = m_year.left(4); // Take first 4 characters (year)
    }
    
    // Numeric metadata with better conversion
    QVariant trackVar = metaData.value(QMediaMetaData::TrackNumber);
    if (trackVar.isValid()) {
        m_trackNumber = trackVar.toInt();
    }
    
    QVariant durationVar = metaData.value(QMediaMetaData::Duration);
    if (durationVar.isValid()) {
        m_duration = durationVar.toLongLong();
    }
    
    // Cover art - try multiple keys
    m_coverArt = QImage();
    
    // Try CoverArtImage first (most common)
    QVariant coverArtVariant = metaData.value(QMediaMetaData::CoverArtImage);
    if (coverArtVariant.isValid()) {
        m_coverArt = coverArtVariant.value<QImage>();
    }
    
    // Try ThumbnailImage if CoverArtImage didn't work
    if (m_coverArt.isNull()) {
        coverArtVariant = metaData.value(QMediaMetaData::ThumbnailImage);
        if (coverArtVariant.isValid()) {
            m_coverArt = coverArtVariant.value<QImage>();
        }
    }
    
    // Clear cached URL
    m_coverArtUrl.clear();
    
    emit metadataChanged();
}

void TrackMetadata::clear()
{
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_genre.clear();
    m_year.clear();
    m_trackNumber = 0;
    m_duration = 0;
    m_coverArt = QImage();
    m_coverArtUrl.clear();
}

QString TrackMetadata::coverArtUrl() const
{
    if (m_coverArt.isNull()) {
        return QString();
    }
    
    // Use cached URL if available
    if (!m_coverArtUrl.isEmpty()) {
        return m_coverArtUrl;
    }
    
    // Generate base64 data URL
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    if (m_coverArt.save(&buffer, "PNG", 85)) {
        m_coverArtUrl = QString("data:image/png;base64,%1")
                        .arg(QString::fromLatin1(byteArray.toBase64()));
    }
    
    return m_coverArtUrl;
}

QString TrackMetadata::searchableText() const
{
    // Combine all metadata for searching
    QStringList searchTerms;
    searchTerms << m_title << m_artist << m_album << m_genre << m_year;
    
    // Add track number as string
    if (m_trackNumber > 0) {
        searchTerms << QString::number(m_trackNumber);
    }
    
    return searchTerms.join(" ").toLower();
}

bool TrackMetadata::hasMetadata() const
{
    return !m_title.isEmpty() || !m_artist.isEmpty() || !m_album.isEmpty() || 
           !m_genre.isEmpty() || !m_year.isEmpty() || m_trackNumber > 0;
}
