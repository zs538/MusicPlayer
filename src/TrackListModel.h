#ifndef TRACKLISTMODEL_H
#define TRACKLISTMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QDateTime>

struct TrackInfo {
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString performer;
    QString composer;
    int year = 0;
    int originalYear = 0;
    int trackNumber = 0;
    int discNumber = 0;
    qint64 durationMs = 0;
    QString genre;
    int sampleRate = 0;
    int bitDepth = 0;
    int bitrate = 0;
    QString url;
    QString fileName;
    qint64 fileSize = 0;
    QString fileType;
    QDateTime dateCreated;
    QDateTime dateModified;
    QString comment;
    int bpm = 0;
    QString initialKey;
    QVariantMap customTags;
    
    bool isValid() const { return !filePath.isEmpty(); }
};

class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qint64 totalDurationMs READ totalDurationMs NOTIFY totalsChanged)

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        PerformerRole,
        ComposerRole,
        YearRole,
        OriginalYearRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationMsRole,
        GenreRole,
        SampleRateRole,
        BitDepthRole,
        BitrateRole,
        UrlRole,
        FileNameRole,
        FileSizeRole,
        FileTypeRole,
        DateCreatedRole,
        DateModifiedRole,
        CommentRole,
        BpmRole,
        InitialKeyRole,
        TrackDataRole,
        DisplayRole
    };
    Q_ENUM(Roles)
    
    explicit TrackListModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    int count() const { return m_tracks.count(); }
    qint64 totalDurationMs() const { return m_totalDurationMs; }
    
    TrackInfo trackAt(int index) const;
    int indexOf(const QString &filePath) const;
    
    void addTrack(const TrackInfo &track);
    void addTracks(const QVector<TrackInfo> &tracks);
    void insertTrack(int index, const TrackInfo &track);
    Q_INVOKABLE void insertTrackData(int index, const QString &filePath, const QString &title, 
                                      const QString &artist, const QString &album, qint64 durationMs, int trackNumber = 0);
    Q_INVOKABLE void removeTrack(int index);
    Q_INVOKABLE void removeRows(int index, int count);
    Q_INVOKABLE void moveRow(int from, int to);
    Q_INVOKABLE void sortByColumn(const QString &key, bool ascending = true);
    Q_INVOKABLE void clear();
    
    void updateTrackMetadata(int index, const TrackInfo &track);

signals:
    void countChanged();
    void totalsChanged();

private:
    QVector<TrackInfo> m_tracks;
    qint64 m_totalDurationMs = 0;
};

#endif // TRACKLISTMODEL_H
