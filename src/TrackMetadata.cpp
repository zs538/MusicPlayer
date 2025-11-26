#include "TrackMetadata.h"

#include <QBuffer>

TrackMetadata::TrackMetadata(QObject* parent)
    : QObject(parent)
{
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
    
    // Scale down large images for efficient data URLs (300px max is plenty for UI)
    static constexpr int MAX_COVER_SIZE = 300;
    QImage scaled = m_coverArt;
    if (scaled.width() > MAX_COVER_SIZE || scaled.height() > MAX_COVER_SIZE) {
        scaled = scaled.scaled(MAX_COVER_SIZE, MAX_COVER_SIZE, 
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    // Use JPEG for smaller data URLs (photos compress much better than PNG)
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    if (scaled.save(&buffer, "JPEG", 85)) {
        m_coverArtUrl = QString("data:image/jpeg;base64,%1")
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
