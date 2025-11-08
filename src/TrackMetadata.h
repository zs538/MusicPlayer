#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QMediaMetaData>

class TrackMetadata : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString genre READ genre NOTIFY metadataChanged)
    Q_PROPERTY(QString year READ year NOTIFY metadataChanged)
    Q_PROPERTY(int trackNumber READ trackNumber NOTIFY metadataChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY metadataChanged)
    Q_PROPERTY(QString coverArtUrl READ coverArtUrl NOTIFY metadataChanged)

    friend class MetadataReader;

public:
    explicit TrackMetadata(QObject* parent = nullptr);
    
    // Static factory method
    static TrackMetadata* fromMediaMetaData(const QMediaMetaData& metaData, QObject* parent = nullptr);
    
    // Load metadata and emit changes
    void loadFromMediaMetaData(const QMediaMetaData& metaData);
    
    // Clear all metadata
    void clear();
    
    // Getters
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString genre() const { return m_genre; }
    QString year() const { return m_year; }
    int trackNumber() const { return m_trackNumber; }
    qint64 duration() const { return m_duration; }
    QString coverArtUrl() const;
    
    // Collection-friendly methods
    QString displayTitle() const { return m_title.isEmpty() ? "Unknown Title" : m_title; }
    QString displayArtist() const { return m_artist.isEmpty() ? "Unknown Artist" : m_artist; }
    QString displayAlbum() const { return m_album.isEmpty() ? "" : m_album; }
    QString displayGenre() const { return m_genre.isEmpty() ? "" : m_genre; }
    QString displayYear() const { return m_year.isEmpty() ? "" : m_year; }
    
    // Search/sort helpers
    QString searchableText() const;
    bool hasMetadata() const;

signals:
    void metadataChanged();

private:
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_genre;
    QString m_year;
    int m_trackNumber = 0;
    qint64 m_duration = 0;
    QImage m_coverArt;
    mutable QString m_coverArtUrl; // Cached for performance
};
