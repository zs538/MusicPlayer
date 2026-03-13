#include "TrackListModel.h"
#include <QFileInfo>
#include <algorithm>

namespace {
QVariantMap trackToVariantMap(const TrackInfo &track)
{
    QVariantMap map;
    map.insert(QStringLiteral("filePath"), track.filePath);
    map.insert(QStringLiteral("title"), track.title);
    map.insert(QStringLiteral("artist"), track.artist);
    map.insert(QStringLiteral("album"), track.album);
    map.insert(QStringLiteral("albumArtist"), track.albumArtist);
    map.insert(QStringLiteral("performer"), track.performer);
    map.insert(QStringLiteral("composer"), track.composer);
    map.insert(QStringLiteral("year"), track.year);
    map.insert(QStringLiteral("originalYear"), track.originalYear);
    map.insert(QStringLiteral("trackNumber"), track.trackNumber);
    map.insert(QStringLiteral("discNumber"), track.discNumber);
    map.insert(QStringLiteral("durationMs"), track.durationMs);
    map.insert(QStringLiteral("genre"), track.genre);
    map.insert(QStringLiteral("sampleRate"), track.sampleRate);
    map.insert(QStringLiteral("bitDepth"), track.bitDepth);
    map.insert(QStringLiteral("bitrate"), track.bitrate);
    map.insert(QStringLiteral("url"), track.url);
    map.insert(QStringLiteral("fileName"), track.fileName);
    map.insert(QStringLiteral("fileSize"), track.fileSize);
    map.insert(QStringLiteral("fileType"), track.fileType);
    map.insert(QStringLiteral("dateCreated"), track.dateCreated);
    map.insert(QStringLiteral("dateModified"), track.dateModified);
    map.insert(QStringLiteral("comment"), track.comment);
    map.insert(QStringLiteral("bpm"), track.bpm);
    map.insert(QStringLiteral("initialKey"), track.initialKey);
    map.insert(QStringLiteral("customTags"), track.customTags);
    map.insert(QStringLiteral("display"), track.title.isEmpty() ? QFileInfo(track.filePath).fileName() : track.title);
    return map;
}

QString displayText(const TrackInfo &track)
{
    if (!track.title.trimmed().isEmpty())
        return track.title;
    if (!track.fileName.trimmed().isEmpty())
        return track.fileName;
    return QFileInfo(track.filePath).fileName();
}

QString variantToSortText(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return {};
    if (value.metaType().id() == QMetaType::QStringList)
        return value.toStringList().join(QStringLiteral("; "));
    if (value.metaType().id() == QMetaType::QVariantList) {
        QStringList parts;
        const QVariantList values = value.toList();
        parts.reserve(values.size());
        for (const QVariant &entry : values)
            parts.append(entry.toString());
        return parts.join(QStringLiteral("; "));
    }
    return value.toString();
}

QString customTagSortText(const QVariantMap &customTags, const QString &tagKey)
{
    if (tagKey.isEmpty())
        return {};
    if (customTags.contains(tagKey))
        return variantToSortText(customTags.value(tagKey));
    for (auto it = customTags.constBegin(); it != customTags.constEnd(); ++it) {
        if (it.key().compare(tagKey, Qt::CaseInsensitive) == 0)
            return variantToSortText(it.value());
    }
    return {};
}

int compareTextValue(const QString &left, const QString &right)
{
    const QString a = left.trimmed();
    const QString b = right.trimmed();
    const bool hasA = !a.isEmpty();
    const bool hasB = !b.isEmpty();
    if (hasA != hasB)
        return hasA ? -1 : 1;
    return QString::compare(a, b, Qt::CaseInsensitive);
}

int compareOptionalInt(int left, int right)
{
    const bool hasLeft = left > 0;
    const bool hasRight = right > 0;
    if (hasLeft != hasRight)
        return hasLeft ? -1 : 1;
    if (!hasLeft)
        return 0;
    return (left > right) - (left < right);
}

int compareOptionalLongLong(qint64 left, qint64 right)
{
    const bool hasLeft = left > 0;
    const bool hasRight = right > 0;
    if (hasLeft != hasRight)
        return hasLeft ? -1 : 1;
    if (!hasLeft)
        return 0;
    return (left > right) - (left < right);
}

int compareDateTimeValue(const QDateTime &left, const QDateTime &right)
{
    const bool hasLeft = left.isValid();
    const bool hasRight = right.isValid();
    if (hasLeft != hasRight)
        return hasLeft ? -1 : 1;
    if (!hasLeft)
        return 0;
    const qint64 a = left.toMSecsSinceEpoch();
    const qint64 b = right.toMSecsSinceEpoch();
    return (a > b) - (a < b);
}

int compareTrackByKey(const TrackInfo &left, const TrackInfo &right, const QString &key)
{
    const QString normalizedKey = key.trimmed().toLower();
    if (normalizedKey == QStringLiteral("title") || normalizedKey == QStringLiteral("display"))
        return compareTextValue(displayText(left), displayText(right));
    if (normalizedKey == QStringLiteral("artist"))
        return compareTextValue(left.artist, right.artist);
    if (normalizedKey == QStringLiteral("album"))
        return compareTextValue(left.album, right.album);
    if (normalizedKey == QStringLiteral("albumartist"))
        return compareTextValue(left.albumArtist, right.albumArtist);
    if (normalizedKey == QStringLiteral("performer"))
        return compareTextValue(left.performer, right.performer);
    if (normalizedKey == QStringLiteral("composer"))
        return compareTextValue(left.composer, right.composer);
    if (normalizedKey == QStringLiteral("genre"))
        return compareTextValue(left.genre, right.genre);
    if (normalizedKey == QStringLiteral("filepath"))
        return compareTextValue(left.filePath, right.filePath);
    if (normalizedKey == QStringLiteral("filename"))
        return compareTextValue(left.fileName.isEmpty() ? QFileInfo(left.filePath).fileName() : left.fileName,
                                right.fileName.isEmpty() ? QFileInfo(right.filePath).fileName() : right.fileName);
    if (normalizedKey == QStringLiteral("filetype"))
        return compareTextValue(left.fileType, right.fileType);
    if (normalizedKey == QStringLiteral("comment"))
        return compareTextValue(left.comment, right.comment);
    if (normalizedKey == QStringLiteral("initialkey"))
        return compareTextValue(left.initialKey, right.initialKey);
    if (normalizedKey == QStringLiteral("url"))
        return compareTextValue(left.url, right.url);
    if (normalizedKey == QStringLiteral("tracknumber"))
        return compareOptionalInt(left.trackNumber, right.trackNumber);
    if (normalizedKey == QStringLiteral("discnumber"))
        return compareOptionalInt(left.discNumber, right.discNumber);
    if (normalizedKey == QStringLiteral("year"))
        return compareOptionalInt(left.year, right.year);
    if (normalizedKey == QStringLiteral("originalyear"))
        return compareOptionalInt(left.originalYear, right.originalYear);
    if (normalizedKey == QStringLiteral("samplerate"))
        return compareOptionalInt(left.sampleRate, right.sampleRate);
    if (normalizedKey == QStringLiteral("bitdepth"))
        return compareOptionalInt(left.bitDepth, right.bitDepth);
    if (normalizedKey == QStringLiteral("bitrate"))
        return compareOptionalInt(left.bitrate, right.bitrate);
    if (normalizedKey == QStringLiteral("bpm"))
        return compareOptionalInt(left.bpm, right.bpm);
    if (normalizedKey == QStringLiteral("durationms"))
        return compareOptionalLongLong(left.durationMs, right.durationMs);
    if (normalizedKey == QStringLiteral("filesize"))
        return compareOptionalLongLong(left.fileSize, right.fileSize);
    if (normalizedKey == QStringLiteral("datecreated"))
        return compareDateTimeValue(left.dateCreated, right.dateCreated);
    if (normalizedKey == QStringLiteral("datemodified"))
        return compareDateTimeValue(left.dateModified, right.dateModified);
    if (normalizedKey.startsWith(QStringLiteral("custom:")))
        return compareTextValue(customTagSortText(left.customTags, normalizedKey.mid(7)),
                                customTagSortText(right.customTags, normalizedKey.mid(7)));
    return compareTextValue(displayText(left), displayText(right));
}
}

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
    case TrackDataRole:
        return trackToVariantMap(track);
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
        {TrackDataRole, "trackData"},
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

void TrackListModel::sortByColumn(const QString &key, bool ascending)
{
    const QString normalizedKey = key.trimmed();
    if (normalizedKey.isEmpty() || m_tracks.size() < 2)
        return;

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [normalizedKey, ascending](const TrackInfo &left, const TrackInfo &right) {
        const int cmp = compareTrackByKey(left, right, normalizedKey);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
    endResetModel();
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
