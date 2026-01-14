#include "TrackListModel.h"
#include <QFileInfo>

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tracks.count();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.count())
        return QVariant();
    
    const TrackInfo &track = m_tracks.at(index.row());
    
    switch (role) {
    case FilePathRole:
        return track.filePath;
    case TitleRole:
        return track.title;
    case ArtistRole:
        return track.artist;
    case AlbumRole:
        return track.album;
    case AlbumArtistRole:
        return track.albumArtist;
    case PerformerRole:
        return track.performer;
    case ComposerRole:
        return track.composer;
    case YearRole:
        return track.year;
    case OriginalYearRole:
        return track.originalYear;
    case TrackNumberRole:
        return track.trackNumber;
    case DiscNumberRole:
        return track.discNumber;
    case DurationMsRole:
        return track.durationMs;
    case GenreRole:
        return track.genre;
    case SampleRateRole:
        return track.sampleRate;
    case BitDepthRole:
        return track.bitDepth;
    case BitrateRole:
        return track.bitrate;
    case UrlRole:
        return track.url;
    case FileNameRole:
        return track.fileName;
    case FileSizeRole:
        return track.fileSize;
    case FileTypeRole:
        return track.fileType;
    case DateCreatedRole:
        return track.dateCreated;
    case DateModifiedRole:
        return track.dateModified;
    case CommentRole:
        return track.comment;
    case BpmRole:
        return track.bpm;
    case InitialKeyRole:
        return track.initialKey;
    case DisplayRole:
    case Qt::DisplayRole:
        return track.title.isEmpty() ? QFileInfo(track.filePath).fileName() : track.title;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {FilePathRole, "filePath"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {AlbumArtistRole, "albumArtist"},
        {PerformerRole, "performer"},
        {ComposerRole, "composer"},
        {YearRole, "year"},
        {OriginalYearRole, "originalYear"},
        {TrackNumberRole, "trackNumber"},
        {DiscNumberRole, "discNumber"},
        {DurationMsRole, "durationMs"},
        {GenreRole, "genre"},
        {SampleRateRole, "sampleRate"},
        {BitDepthRole, "bitDepth"},
        {BitrateRole, "bitrate"},
        {UrlRole, "url"},
        {FileNameRole, "fileName"},
        {FileSizeRole, "fileSize"},
        {FileTypeRole, "fileType"},
        {DateCreatedRole, "dateCreated"},
        {DateModifiedRole, "dateModified"},
        {CommentRole, "comment"},
        {BpmRole, "bpm"},
        {InitialKeyRole, "initialKey"},
        {DisplayRole, "display"}
    };
}

TrackInfo TrackListModel::trackAt(int index) const
{
    if (index >= 0 && index < m_tracks.count())
        return m_tracks.at(index);
    return TrackInfo();
}

int TrackListModel::indexOf(const QString &filePath) const
{
    for (int i = 0; i < m_tracks.count(); ++i) {
        if (m_tracks.at(i).filePath == filePath)
            return i;
    }
    return -1;
}

void TrackListModel::addTrack(const TrackInfo &track)
{
    beginInsertRows(QModelIndex(), m_tracks.count(), m_tracks.count());
    m_tracks.append(track);
    m_totalDurationMs += track.durationMs;
    endInsertRows();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::addTracks(const QVector<TrackInfo> &tracks)
{
    if (tracks.isEmpty())
        return;
    
    beginInsertRows(QModelIndex(), m_tracks.count(), m_tracks.count() + tracks.count() - 1);
    for (const TrackInfo &t : tracks) {
        m_totalDurationMs += t.durationMs;
    }
    m_tracks.append(tracks);
    endInsertRows();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::insertTrack(int index, const TrackInfo &track)
{
    index = qBound(0, index, m_tracks.count());
    beginInsertRows(QModelIndex(), index, index);
    m_tracks.insert(index, track);
    m_totalDurationMs += track.durationMs;
    endInsertRows();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::insertTrackData(int index, const QString &filePath, const QString &title,
                                      const QString &artist, const QString &album, qint64 durationMs, int trackNumber)
{
    TrackInfo track;
    track.filePath = filePath;
    track.title = title;
    track.artist = artist;
    track.album = album;
    track.durationMs = durationMs;
    track.trackNumber = trackNumber;
    insertTrack(index, track);
}

void TrackListModel::insertTrackFromMap(int index, const QVariantMap &trackData)
{
    TrackInfo track;
    track.filePath = trackData.value("filePath").toString();
    track.title = trackData.value("title").toString();
    track.artist = trackData.value("artist").toString();
    track.album = trackData.value("album").toString();
    track.albumArtist = trackData.value("albumArtist").toString();
    track.performer = trackData.value("performer").toString();
    track.composer = trackData.value("composer").toString();
    track.year = trackData.value("year").toInt();
    track.originalYear = trackData.value("originalYear").toInt();
    track.trackNumber = trackData.value("trackNumber").toInt();
    track.discNumber = trackData.value("discNumber").toInt();
    track.durationMs = trackData.value("durationMs").toLongLong();
    track.genre = trackData.value("genre").toString();
    track.sampleRate = trackData.value("sampleRate").toInt();
    track.bitDepth = trackData.value("bitDepth").toInt();
    track.bitrate = trackData.value("bitrate").toInt();
    track.url = trackData.value("url").toString();
    track.fileName = trackData.value("fileName").toString();
    track.fileSize = trackData.value("fileSize").toLongLong();
    track.fileType = trackData.value("fileType").toString();
    track.dateCreated = trackData.value("dateCreated").toDateTime();
    track.dateModified = trackData.value("dateModified").toDateTime();
    track.comment = trackData.value("comment").toString();
    track.bpm = trackData.value("bpm").toInt();
    track.initialKey = trackData.value("initialKey").toString();
    insertTrack(index, track);
}

void TrackListModel::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.count())
        return;
    
    m_totalDurationMs -= m_tracks.at(index).durationMs;
    beginRemoveRows(QModelIndex(), index, index);
    m_tracks.removeAt(index);
    endRemoveRows();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::removeRows(int index, int count)
{
    if (index < 0 || count <= 0 || index + count > m_tracks.count())
        return;
    
    for (int i = index; i < index + count; ++i) {
        m_totalDurationMs -= m_tracks.at(i).durationMs;
    }
    beginRemoveRows(QModelIndex(), index, index + count - 1);
    m_tracks.remove(index, count);
    endRemoveRows();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::moveRow(int from, int to)
{
    if (from < 0 || from >= m_tracks.count() || to < 0 || to >= m_tracks.count() || from == to)
        return;
    
    int destIndex = to > from ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destIndex))
        return;
    
    m_tracks.move(from, to);
    endMoveRows();
}

void TrackListModel::clear()
{
    if (m_tracks.isEmpty())
        return;
    
    beginResetModel();
    m_tracks.clear();
    m_totalDurationMs = 0;
    endResetModel();
    emit countChanged();
    emit totalsChanged();
}

void TrackListModel::updateTrackMetadata(int index, const TrackInfo &track)
{
    if (index < 0 || index >= m_tracks.count())
        return;
    
    qint64 oldDuration = m_tracks[index].durationMs;
    m_tracks[index] = track;
    if (track.durationMs != oldDuration) {
        m_totalDurationMs = m_totalDurationMs - oldDuration + track.durationMs;
        emit totalsChanged();
    }
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex);
}

void TrackListModel::updateTrackMetadata(int row, const QString &title, const QString &artist, const QString &album)
{
    if (row < 0 || row >= m_tracks.count())
        return;
    
    m_tracks[row].title = title;
    m_tracks[row].artist = artist;
    m_tracks[row].album = album;
    
    QModelIndex modelIndex = createIndex(row, 0);
    emit dataChanged(modelIndex, modelIndex);
}

void TrackListModel::recalculateTotals()
{
    m_totalDurationMs = 0;
    for (const TrackInfo &t : m_tracks) {
        m_totalDurationMs += t.durationMs;
    }
    emit totalsChanged();
}
