#include "CollectionBrowseModel.h"
#include "library/LibraryDatabase.h"
#include <QMap>
#include <algorithm>

CollectionBrowseModel::CollectionBrowseModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CollectionBrowseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CollectionBrowseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();

    const Entry &e = m_entries.at(index.row());

    switch (role) {
    case EntryTypeRole: return e.entryType;
    case GroupTypeRole: return e.groupType;
    case GroupValueRole: return e.groupValue;
    case DisplayTextRole: return e.displayText;
    case SubtitleRole: return e.subtitle;
    case ChildCountRole: return e.childCount;
    case RepresentativeFilePathRole: return e.representativeFilePath;
    case ImagePathRole: return e.imagePath;
    case FilePathRole: return e.filePath;
    case TitleRole: return e.title;
    case ArtistRole: return e.artist;
    case AlbumRole: return e.album;
    case AlbumArtistRole: return e.albumArtist;
    case TrackNumberRole: return e.trackNumber;
    case DiscNumberRole: return e.discNumber;
    case DurationMsRole: return e.durationMs;
    case GenreRole: return e.genre;
    case YearRole: return e.year;
    case BitrateRole: return e.bitrate;
    case FileTypeRole: return e.fileType;
    }
    return QVariant();
}

QHash<int, QByteArray> CollectionBrowseModel::roleNames() const
{
    return {
        { EntryTypeRole, "entryType" },
        { GroupTypeRole, "groupType" },
        { GroupValueRole, "groupValue" },
        { DisplayTextRole, "displayText" },
        { SubtitleRole, "subtitle" },
        { ChildCountRole, "childCount" },
        { RepresentativeFilePathRole, "representativeFilePath" },
        { ImagePathRole, "imagePath" },
        { FilePathRole, "filePath" },
        { TitleRole, "title" },
        { ArtistRole, "artist" },
        { AlbumRole, "album" },
        { AlbumArtistRole, "albumArtist" },
        { TrackNumberRole, "trackNumber" },
        { DiscNumberRole, "discNumber" },
        { DurationMsRole, "durationMs" },
        { GenreRole, "genre" },
        { YearRole, "year" },
        { BitrateRole, "bitrate" },
        { FileTypeRole, "fileType" }
    };
}

void CollectionBrowseModel::setDatabase(LibraryDatabase *db)
{
    if (m_database == db)
        return;

    if (m_database)
        disconnect(m_database, nullptr, this, nullptr);

    m_database = db;

    if (m_database)
        connect(m_database, &LibraryDatabase::databaseChanged, this, &CollectionBrowseModel::refresh);

    emit databaseChanged();
    refresh();
}

void CollectionBrowseModel::setFilter(const QVariantList &filter)
{
    TrackFilter newFilter = trackFilterFromVariant(filter);
    if (m_filter.size() == newFilter.size()) {
        bool same = true;
        for (int i = 0; i < m_filter.size(); ++i) {
            if (m_filter[i].field != newFilter[i].field ||
                m_filter[i].op != newFilter[i].op ||
                m_filter[i].value != newFilter[i].value) {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }
    m_filter = newFilter;
    emit filterChanged();
    refresh();
}

void CollectionBrowseModel::setGroupBy(const QString &groupBy)
{
    if (m_groupBy == groupBy)
        return;
    m_groupBy = groupBy;
    emit groupByChanged();
    refresh();
}

void CollectionBrowseModel::setSortBy(const QString &sortBy)
{
    if (m_sortBy == sortBy)
        return;
    m_sortBy = sortBy;
    emit sortByChanged();
    applySearchAndSort();
}

void CollectionBrowseModel::setSortAscending(bool ascending)
{
    if (m_sortAscending == ascending)
        return;
    m_sortAscending = ascending;
    emit sortAscendingChanged();
    applySearchAndSort();
}

void CollectionBrowseModel::setSearchFilter(const QString &filter)
{
    if (m_searchFilter == filter)
        return;
    m_searchFilter = filter;
    emit searchFilterChanged();
    applySearchAndSort();
}

QString CollectionBrowseModel::entryKey(const Entry &e)
{
    // Unique key: for groups it's groupType+groupValue, for tracks it's filePath
    if (e.entryType == "group")
        return QStringLiteral("g:") + e.groupType + QStringLiteral(":") + e.groupValue.toString();
    return QStringLiteral("t:") + e.filePath;
}

bool CollectionBrowseModel::entriesEqual(const Entry &a, const Entry &b)
{
    // Compare all visible fields that would cause a visual change
    return a.entryType == b.entryType &&
           a.displayText == b.displayText &&
           a.subtitle == b.subtitle &&
           a.childCount == b.childCount &&
           a.year == b.year &&
           a.representativeFilePath == b.representativeFilePath;
}

void CollectionBrowseModel::applyIncrementalUpdate(const QVector<Entry> &newEntries)
{
    // Build maps for O(1) lookup
    QHash<QString, int> oldIndexByKey;
    for (int i = 0; i < m_entries.size(); ++i)
        oldIndexByKey.insert(entryKey(m_entries[i]), i);
    
    QHash<QString, int> newIndexByKey;
    for (int i = 0; i < newEntries.size(); ++i)
        newIndexByKey.insert(entryKey(newEntries[i]), i);
    
    // Phase 1: Remove entries that no longer exist (iterate backwards to preserve indices)
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        QString key = entryKey(m_entries[i]);
        if (!newIndexByKey.contains(key)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_entries.remove(i);
            endRemoveRows();
        }
    }
    
    // Rebuild old index map after removals
    oldIndexByKey.clear();
    for (int i = 0; i < m_entries.size(); ++i)
        oldIndexByKey.insert(entryKey(m_entries[i]), i);
    
    // Phase 2: Insert new entries and update existing ones
    for (int newIdx = 0; newIdx < newEntries.size(); ++newIdx) {
        const Entry &newEntry = newEntries[newIdx];
        QString key = entryKey(newEntry);
        
        if (!oldIndexByKey.contains(key)) {
            // New entry - insert at correct position
            beginInsertRows(QModelIndex(), newIdx, newIdx);
            m_entries.insert(newIdx, newEntry);
            endInsertRows();
            
            // Update indices for entries after insertion
            oldIndexByKey.clear();
            for (int i = 0; i < m_entries.size(); ++i)
                oldIndexByKey.insert(entryKey(m_entries[i]), i);
        } else {
            int oldIdx = oldIndexByKey.value(key);
            
            // Check if content changed
            if (!entriesEqual(m_entries[oldIdx], newEntry)) {
                m_entries[oldIdx] = newEntry;
                emit dataChanged(index(oldIdx), index(oldIdx));
            }
            
            // Check if position changed (needs move)
            if (oldIdx != newIdx && oldIdx < m_entries.size() && newIdx < m_entries.size()) {
                // Move to correct position
                if (beginMoveRows(QModelIndex(), oldIdx, oldIdx, QModelIndex(), newIdx > oldIdx ? newIdx + 1 : newIdx)) {
                    m_entries.move(oldIdx, newIdx);
                    endMoveRows();
                    
                    // Rebuild index map after move
                    oldIndexByKey.clear();
                    for (int i = 0; i < m_entries.size(); ++i)
                        oldIndexByKey.insert(entryKey(m_entries[i]), i);
                }
            }
        }
    }
}

void CollectionBrowseModel::applySearchAndSort(bool forceReset)
{
    // Build the new entries list
    QVector<Entry> newEntries = m_allEntries;
    
    // Apply search filter
    if (!m_searchFilter.isEmpty()) {
        QVector<Entry> filtered;
        for (const Entry &e : newEntries) {
            if (e.displayText.contains(m_searchFilter, Qt::CaseInsensitive) ||
                e.subtitle.contains(m_searchFilter, Qt::CaseInsensitive) ||
                e.artist.contains(m_searchFilter, Qt::CaseInsensitive) ||
                e.album.contains(m_searchFilter, Qt::CaseInsensitive)) {
                filtered.append(e);
            }
        }
        newEntries = filtered;
    }
    
    // Apply sorting
    std::sort(newEntries.begin(), newEntries.end(), [this](const Entry &a, const Entry &b) {
        int cmp = 0;
        if (m_sortBy == "name") {
            cmp = a.displayText.compare(b.displayText, Qt::CaseInsensitive);
        } else if (m_sortBy == "year") {
            cmp = a.year - b.year;
        } else if (m_sortBy == "count") {
            cmp = a.childCount - b.childCount;
        } else if (m_sortBy == "artist") {
            cmp = a.artist.compare(b.artist, Qt::CaseInsensitive);
        } else if (m_sortBy == "album") {
            cmp = a.album.compare(b.album, Qt::CaseInsensitive);
        } else {
            cmp = a.displayText.compare(b.displayText, Qt::CaseInsensitive);
        }
        return m_sortAscending ? (cmp < 0) : (cmp > 0);
    });
    
    // Decide: incremental update or full reset?
    // Use incremental if not forced and we have existing data (typical rescan case)
    bool useIncremental = !forceReset && !m_entries.isEmpty() && 
                          qAbs(newEntries.size() - m_entries.size()) < m_entries.size();
    
    if (useIncremental) {
        applyIncrementalUpdate(newEntries);
    } else {
        beginResetModel();
        m_entries = newEntries;
        endResetModel();
    }
    
    emit countChanged();
}

void CollectionBrowseModel::refresh()
{
    if (!m_database) {
        beginResetModel();
        m_entries.clear();
        m_allEntries.clear();
        endResetModel();
        emit countChanged();
        return;
    }

    QVector<LibraryTrack> tracks = m_database->tracksMatchingFilter(m_filter);

    m_allEntries.clear();

    if (m_groupBy.isEmpty() || m_groupBy == "none") {
        buildTracks(tracks);
        m_title = QString("%1 tracks").arg(tracks.size());
    } else {
        buildGroups(tracks);
        m_title = QString("%1 %2s").arg(m_allEntries.size()).arg(m_groupBy);
    }

    // Apply search filter and sorting
    applySearchAndSort();
    emit titleChanged();
}

QVariant CollectionBrowseModel::getGroupValue(const LibraryTrack &track, const QString &groupType) const
{
    if (groupType == "albumartist") return track.albumArtist.isEmpty() ? track.artist : track.albumArtist;
    if (groupType == "artist") return track.artist;
    if (groupType == "album") return track.album;
    if (groupType == "disc") return track.discNumber;
    if (groupType == "genre") return track.genre;
    if (groupType == "year") return track.year;
    if (groupType == "bitrate") return track.bitrate;
    if (groupType == "filetype") return track.fileType;
    return QVariant();
}

QString CollectionBrowseModel::formatGroupDisplay(const QString &groupType, const QVariant &value) const
{
    if (groupType == "disc") {
        int disc = value.toInt();
        return disc > 0 ? QString("Disc %1").arg(disc) : "Disc 1";
    }
    if (groupType == "year") {
        int year = value.toInt();
        return year > 0 ? QString::number(year) : "Unknown Year";
    }
    if (groupType == "bitrate") {
        int br = value.toInt();
        return br > 0 ? QString("%1 kbps").arg(br) : "Unknown Bitrate";
    }

    QString str = value.toString();
    if (str.isEmpty()) {
        if (groupType == "albumartist") return "Unknown Artist";
        if (groupType == "artist") return "Unknown Artist";
        if (groupType == "album") return "Unknown Album";
        if (groupType == "genre") return "Unknown Genre";
        if (groupType == "filetype") return "Unknown Type";
        return "Unknown";
    }
    return str;
}

void CollectionBrowseModel::buildGroups(const QVector<LibraryTrack> &tracks)
{
    struct GroupData {
        QVariant value;
        int count = 0;
        QString representativeFilePath;
        int year = 0;
        QString artist;
        QString album;
    };

    QMap<QString, GroupData> groups;

    for (const LibraryTrack &track : tracks) {
        QVariant val = getGroupValue(track, m_groupBy);
        QString key = val.toString();

        if (!groups.contains(key)) {
            GroupData gd;
            gd.value = val;
            gd.count = 1;
            gd.representativeFilePath = track.filePath;
            gd.year = track.year;
            gd.artist = track.albumArtist.isEmpty() ? track.artist : track.albumArtist;
            gd.album = track.album;
            groups.insert(key, gd);
        } else {
            groups[key].count++;
            // Keep earliest year for sorting
            if (track.year > 0 && (groups[key].year == 0 || track.year < groups[key].year))
                groups[key].year = track.year;
        }
    }

    QList<QString> keys = groups.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    for (const QString &key : keys) {
        const GroupData &gd = groups.value(key);
        Entry e;
        e.entryType = "group";
        e.groupType = m_groupBy;
        e.groupValue = gd.value;
        e.displayText = formatGroupDisplay(m_groupBy, gd.value);
        e.subtitle = QString("%1 tracks").arg(gd.count);
        e.childCount = gd.count;
        e.representativeFilePath = gd.representativeFilePath;
        e.year = gd.year;
        e.artist = gd.artist;
        e.album = gd.album;
        m_allEntries.append(e);
    }
}

void CollectionBrowseModel::buildTracks(const QVector<LibraryTrack> &tracks)
{
    for (const LibraryTrack &track : tracks) {
        Entry e;
        e.entryType = "track";
        e.filePath = track.filePath;
        e.title = track.title.isEmpty() ? track.fileName : track.title;
        e.displayText = e.title;
        e.artist = track.artist;
        e.album = track.album;
        e.albumArtist = track.albumArtist;
        e.trackNumber = track.trackNumber;
        e.discNumber = track.discNumber;
        e.durationMs = track.durationMs;
        e.genre = track.genre;
        e.year = track.year;
        e.bitrate = track.bitrate;
        e.fileType = track.fileType;
        e.representativeFilePath = track.filePath;
        m_allEntries.append(e);
    }
}

QVariantList CollectionBrowseModel::tracksForGroup(const QString &groupType, const QVariant &groupValue) const
{
    QVariantList result;
    if (!m_database)
        return result;

    TrackFilter extendedFilter = m_filter;
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    extendedFilter.append(cond);

    QVector<LibraryTrack> tracks = m_database->tracksMatchingFilter(extendedFilter);
    for (const LibraryTrack &track : tracks) {
        QVariantMap map;
        map["filePath"] = track.filePath;
        map["title"] = track.title;
        map["artist"] = track.artist;
        map["album"] = track.album;
        map["trackNumber"] = track.trackNumber;
        map["discNumber"] = track.discNumber;
        map["durationMs"] = track.durationMs;
        result.append(map);
    }
    return result;
}
