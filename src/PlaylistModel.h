#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

class TrackMetadata;

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit PlaylistModel(QObject* parent = nullptr);

    enum Roles { 
        UrlRole = Qt::UserRole + 1, 
        DisplayRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        DurationRole,
        MetadataRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    Q_INVOKABLE void add(const QUrl& url);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void moveRowTo(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE int count() const { return m_items.size(); }
    Q_INVOKABLE QVariantMap get(int index) const;

    Q_INVOKABLE bool exportM3U8(const QUrl& url) const;
    Q_INVOKABLE bool importM3U8(const QUrl& url);
    
    // Metadata methods
    Q_INVOKABLE void updateMetadata(int index, TrackMetadata* metadata);
    Q_INVOKABLE TrackMetadata* getMetadata(int index) const;

private:
    struct Item { 
        QUrl url; 
        QString display; 
        TrackMetadata* metadata {nullptr}; 
    };
    QVector<Item> m_items;
};
