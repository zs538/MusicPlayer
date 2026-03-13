#include "CollectionBrowseModel.h"
#include "library/LibraryDatabase.h"
#include <QDateTime>
#include <QMap>
#include <algorithm>

namespace {
QVariantMap libraryTrackToVariantMap(const LibraryTrack &track)
{
    QVariantMap map;
    map.insert(QStringLiteral("filePath"), track.filePath);
    map.insert(QStringLiteral("title"), track.title.isEmpty() ? track.fileName : track.title);
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
    map.insert(QStringLiteral("dateCreated"), QDateTime::fromSecsSinceEpoch(track.createdTime));
    map.insert(QStringLiteral("dateModified"), QDateTime::fromSecsSinceEpoch(track.modifiedTime));
    map.insert(QStringLiteral("comment"), track.comment);
    map.insert(QStringLiteral("bpm"), track.bpm);
    map.insert(QStringLiteral("initialKey"), track.initialKey);
    map.insert(QStringLiteral("customTags"), track.customTags);
    map.insert(QStringLiteral("display"), track.title.isEmpty() ? track.fileName : track.title);
    return map;
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

QString customTagSortText(const QVariantMap &trackData, const QString &tagKey)
{
    const QVariantMap customTags = trackData.value(QStringLiteral("customTags")).toMap();
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

QString formatDurationMs(qint64 ms)
{
    if (ms <= 0)
        return {};

    const qint64 totalSeconds = ms / 1000;
    const qint64 seconds = totalSeconds % 60;
    const qint64 totalMinutes = totalSeconds / 60;
    const qint64 minutes = totalMinutes % 60;
    const qint64 hours = totalMinutes / 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(totalMinutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatModifiedDate(qint64 secsSinceEpoch)
{
    if (secsSinceEpoch <= 0)
        return {};
    return QDateTime::fromSecsSinceEpoch(secsSinceEpoch).date().toString(Qt::ISODate);
}

QString joinSubtitleParts(const QStringList &parts)
{
    QStringList filteredParts;
    filteredParts.reserve(parts.size());

    for (const QString &part : parts) {
        if (!part.isEmpty())
            filteredParts.append(part);
    }

    return filteredParts.join(QStringLiteral(" - "));
}
}

CollectionBrowseModel::CollectionBrowseModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// --- LRU result cache ---

bool CollectionBrowseModel::CacheKey::operator==(const CacheKey &o) const
{
    if (groupBy != o.groupBy || filter.size() != o.filter.size())
        return false;
    for (int i = 0; i < filter.size(); ++i) {
        if (filter[i].field != o.filter[i].field ||
            filter[i].op != o.filter[i].op ||
            filter[i].value != o.filter[i].value)
            return false;
    }
    return true;
}

CollectionBrowseModel::CacheKey CollectionBrowseModel::currentCacheKey() const
{
    return CacheKey{m_filter, m_groupBy};
}

CollectionBrowseModel::CacheEntry *CollectionBrowseModel::findCache(const CacheKey &key)
{
    for (int i = 0; i < m_cache.size(); ++i) {
        if (m_cache[i].key == key) {
            // Move to front (most recently used)
            if (i > 0)
                m_cache.move(i, 0);
            return &m_cache[0];
        }
    }
    return nullptr;
}

void CollectionBrowseModel::storeCache(const CacheKey &key, const QVector<Entry> &entries, const QString &title)
{
    // Check if already cached (update in place)
    for (int i = 0; i < m_cache.size(); ++i) {
        if (m_cache[i].key == key) {
            m_cache[i].entries = entries;
            m_cache[i].sorted.clear();
            m_cache[i].title = title;
            if (i > 0)
                m_cache.move(i, 0);
            return;
        }
    }
    // Evict LRU if full
    if (m_cache.size() >= MaxCacheEntries)
        m_cache.removeLast();
    m_cache.prepend(CacheEntry{key, entries, {}, {}, true, title});
}

void CollectionBrowseModel::invalidateCache()
{
    m_cache.clear();
}

// --- History navigation ---

bool CollectionBrowseModel::canGoBack() const { return !m_backStack.isEmpty(); }
bool CollectionBrowseModel::canGoForward() const { return !m_forwardStack.isEmpty(); }

QVector<CollectionBrowseModel::HistoryEntry> CollectionBrowseModel::historyTrail(qreal currentScrollY,
                                                                                const QString &currentSelectedEntryId) const
{
    QVector<HistoryEntry> trail = m_backStack;
    trail.append({m_filter, m_groupBy, currentScrollY, currentSelectedEntryId});
    for (int i = m_forwardStack.size() - 1; i >= 0; --i)
        trail.append(m_forwardStack[i]);
    return trail;
}

QVariantList CollectionBrowseModel::breadcrumbPath() const
{
    QVariantList result;
    const QVector<HistoryEntry> trail = historyTrail(0, QString());
    result.reserve(trail.size());

    for (int i = 0; i < trail.size(); ++i) {
        QVariantMap item;
        const bool isHome = i == 0 && trail[i].filter.isEmpty();
        item.insert(QStringLiteral("isHome"), isHome);
        item.insert(QStringLiteral("label"), isHome ? QString() : breadcrumbLabelForFilter(trail[i].filter));
        result.append(item);
    }

    return result;
}

void CollectionBrowseModel::jumpToBreadcrumb(int index, qreal currentScrollY, const QString &currentSelectedEntryId)
{
    const QVector<HistoryEntry> trail = historyTrail(currentScrollY, currentSelectedEntryId);
    const int currentIndex = m_backStack.size();

    if (index < 0 || index >= trail.size() || index == currentIndex)
        return;

    QVector<HistoryEntry> newBackStack;
    newBackStack.reserve(index);
    for (int i = 0; i < index; ++i)
        newBackStack.append(trail[i]);

    QVector<HistoryEntry> newForwardStack;
    newForwardStack.reserve(trail.size() - index - 1);
    for (int i = trail.size() - 1; i > index; --i)
        newForwardStack.append(trail[i]);

    const HistoryEntry target = trail[index];
    m_backStack = newBackStack;
    m_forwardStack = newForwardStack;
    m_pendingScrollY = target.scrollY;
    m_pendingSelectedEntryId = target.selectedEntryId;
    applyState(target.filter, target.groupBy);
    emit historyChanged();
    emit pendingScrollYChanged();
    emit pendingSelectedEntryIdChanged();
}

void CollectionBrowseModel::navigate(const QVariantList &filter, const QString &groupBy,
                                     qreal currentScrollY, const QString &currentSelectedEntryId)
{
    m_backStack.append({m_filter, m_groupBy, currentScrollY, currentSelectedEntryId});
    m_forwardStack.clear();
    m_pendingScrollY = 0;
    m_pendingSelectedEntryId.clear();
    applyState(trackFilterFromVariant(filter), groupBy);
    emit historyChanged();
    emit pendingScrollYChanged();
    emit pendingSelectedEntryIdChanged();
}

void CollectionBrowseModel::goBack(qreal currentScrollY, const QString &currentSelectedEntryId)
{
    if (m_backStack.isEmpty()) return;
    m_forwardStack.append({m_filter, m_groupBy, currentScrollY, currentSelectedEntryId});
    auto entry = m_backStack.takeLast();
    m_pendingScrollY = entry.scrollY;
    m_pendingSelectedEntryId = entry.selectedEntryId;
    applyState(entry.filter, entry.groupBy);
    emit historyChanged();
    emit pendingScrollYChanged();
    emit pendingSelectedEntryIdChanged();
}

void CollectionBrowseModel::goForward(qreal currentScrollY, const QString &currentSelectedEntryId)
{
    if (m_forwardStack.isEmpty()) return;
    m_backStack.append({m_filter, m_groupBy, currentScrollY, currentSelectedEntryId});
    auto entry = m_forwardStack.takeLast();
    m_pendingScrollY = entry.scrollY;
    m_pendingSelectedEntryId = entry.selectedEntryId;
    applyState(entry.filter, entry.groupBy);
    emit historyChanged();
    emit pendingScrollYChanged();
    emit pendingSelectedEntryIdChanged();
}

void CollectionBrowseModel::applyState(const TrackFilter &filter, const QString &groupBy)
{
    m_filter = filter;
    m_groupBy = groupBy;
    refresh();
    emit filterChanged();
    emit groupByChanged();
}

int CollectionBrowseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CollectionBrowseModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_entries.size())
        return QVariant();

    const Entry &e = m_entries[row];

    switch (role) {
    case Qt::DisplayRole: return e.displayText;
    case EntryTypeRole: return e.entryType;
    case GroupTypeRole: return e.groupType;
    case GroupValueRole: return e.groupValue;
    case DisplayTextRole: return e.displayText;
    case SubtitleRole: return e.subtitle;
    case ChildCountRole: return e.childCount;
    case RepresentativeFilePathRole: return e.representativeFilePath;
    case CoverFilePathsRole: return e.coverFilePaths;
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
    case TrackDataRole: return e.trackData;
    case TotalDurationMsRole: return e.totalDurationMs;
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
        { CoverFilePathsRole, "coverFilePaths" },
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
        { FileTypeRole, "fileType" },
        { TrackDataRole, "trackData" },
        { TotalDurationMsRole, "totalDurationMs" }
    };
}

int CollectionBrowseModel::findGroupRow(const QString &groupType, const QVariant &groupValue) const
{
    const QString key = normalizedGroupKey(groupType, groupValue);
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries[i];
        if (e.entryType == QStringLiteral("group") &&
            normalizedGroupKey(e.groupType, e.groupValue) == key)
            return i;
    }
    return -1;
}

QString CollectionBrowseModel::entryIdForEntry(const Entry &entry) const
{
    if (entry.entryType == QStringLiteral("group"))
        return QStringLiteral("g:%1:%2").arg(entry.groupType, normalizedGroupKey(entry.groupType, entry.groupValue));
    if (entry.entryType == QStringLiteral("track"))
        return QStringLiteral("t:%1").arg(entry.filePath);
    return QString();
}

QString CollectionBrowseModel::entryIdAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return QString();
    return entryIdForEntry(m_entries.at(row));
}

int CollectionBrowseModel::indexOfEntryId(const QString &entryId) const
{
    if (entryId.isEmpty())
        return -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (entryIdForEntry(m_entries.at(i)) == entryId)
            return i;
    }
    return -1;
}

void CollectionBrowseModel::setDatabase(LibraryDatabase *db)
{
    if (m_database == db)
        return;

    if (m_database)
        disconnect(m_database, nullptr, this, nullptr);

    m_database = db;

    if (m_database) {
        connect(m_database, &LibraryDatabase::databaseChanged, this, &CollectionBrowseModel::invalidateCache);
        connect(m_database, &LibraryDatabase::databaseChanged, this, &CollectionBrowseModel::refresh);
    }

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
    refresh();
    emit groupByChanged();
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

void CollectionBrowseModel::setSubtitleKey(const QString &subtitleKey)
{
    const QString normalizedKey = subtitleKey.trimmed();
    if (m_subtitleKey == normalizedKey)
        return;

    m_subtitleKey = normalizedKey;

    for (CacheEntry &cacheEntry : m_cache) {
        applySubtitleToEntries(cacheEntry.entries);
        applySubtitleToEntries(cacheEntry.sorted);
    }

    applySubtitleToEntries(m_allEntries);
    emit subtitleKeyChanged();
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

QVector<CollectionBrowseModel::Entry> CollectionBrowseModel::filteredAndSortedEntries() const
{
    QVector<Entry> entries = m_allEntries;

    if (!m_searchFilter.isEmpty()) {
        QVector<Entry> filtered;
        filtered.reserve(entries.size());
        for (const Entry &entry : entries) {
            if (entry.displayText.contains(m_searchFilter, Qt::CaseInsensitive) ||
                entry.subtitle.contains(m_searchFilter, Qt::CaseInsensitive) ||
                entry.artist.contains(m_searchFilter, Qt::CaseInsensitive) ||
                entry.album.contains(m_searchFilter, Qt::CaseInsensitive)) {
                filtered.append(entry);
            }
        }
        entries = filtered;
    }

    if (m_groupBy == QStringLiteral("none")) {
        QString trackSortKey = m_sortBy;
        if (trackSortKey == QStringLiteral("name"))
            trackSortKey = QStringLiteral("title");
        else if (trackSortKey == QStringLiteral("duration"))
            trackSortKey = QStringLiteral("durationMs");
        else if (trackSortKey == QStringLiteral("dateUpdated"))
            trackSortKey = QStringLiteral("dateModified");
        else if (trackSortKey == QStringLiteral("count"))
            trackSortKey = QStringLiteral("title");

        std::stable_sort(entries.begin(), entries.end(), [this, trackSortKey](const Entry &left, const Entry &right) {
            int cmp = compareTrackMapsByKey(left.trackData, right.trackData, trackSortKey);
            if (cmp == 0)
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            return m_sortAscending ? (cmp < 0) : (cmp > 0);
        });
    } else {
        std::sort(entries.begin(), entries.end(), [this](const Entry &left, const Entry &right) {
            int cmp = 0;
            if (m_sortBy == QStringLiteral("name")) {
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            } else if (m_sortBy == QStringLiteral("year")) {
                cmp = (left.year > right.year) - (left.year < right.year);
            } else if (m_sortBy == QStringLiteral("duration")) {
                const qint64 durationLeft = left.entryType == QStringLiteral("group") ? left.totalDurationMs : left.durationMs;
                const qint64 durationRight = right.entryType == QStringLiteral("group") ? right.totalDurationMs : right.durationMs;
                cmp = (durationLeft > durationRight) - (durationLeft < durationRight);
            } else if (m_sortBy == QStringLiteral("count")) {
                cmp = (left.childCount > right.childCount) - (left.childCount < right.childCount);
            } else if (m_sortBy == QStringLiteral("dateUpdated")) {
                cmp = (left.modifiedTime > right.modifiedTime) - (left.modifiedTime < right.modifiedTime);
            } else {
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            }
            if (cmp == 0)
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            return m_sortAscending ? (cmp < 0) : (cmp > 0);
        });
    }

    return entries;
}

void CollectionBrowseModel::applySearchAndSort()
{
    const QVector<Entry> sortedEntries = filteredAndSortedEntries();
    beginResetModel();
    m_entries = sortedEntries;
    endResetModel();
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

    const CacheKey key = currentCacheKey();
    const QString activeSortKey = m_sortBy;
    const bool activeSortAscending = m_sortAscending;

    CacheEntry *cached = findCache(key);
    if (cached) {
        m_allEntries = cached->entries;
        applySubtitleToEntries(m_allEntries);
        cached->entries = m_allEntries;
        m_title = cached->title;

        if (!cached->sorted.isEmpty())
            applySubtitleToEntries(cached->sorted);

        if (!cached->sorted.isEmpty() &&
            cached->sortBy == activeSortKey &&
            cached->sortAscending == activeSortAscending &&
            m_searchFilter.isEmpty()) {
            beginResetModel();
            m_entries = cached->sorted;
            endResetModel();
            emit countChanged();
        } else {
            const QVector<Entry> sortedEntries = filteredAndSortedEntries();
            beginResetModel();
            m_entries = sortedEntries;
            endResetModel();
            emit countChanged();
            if (m_searchFilter.isEmpty()) {
                cached->sorted = sortedEntries;
                cached->sortBy = activeSortKey;
                cached->sortAscending = activeSortAscending;
            }
        }

        emit titleChanged();
        return;
    }

    const QVector<LibraryTrack> tracks = m_database->tracksMatchingFilter(m_filter);

    m_allEntries.clear();

    if (m_groupBy.isEmpty() || m_groupBy == QStringLiteral("none")) {
        buildTracks(tracks);
        m_title = QStringLiteral("%1 tracks").arg(tracks.size());
    } else {
        buildGroups(tracks);
        m_title = QStringLiteral("%1 %2s").arg(m_allEntries.size()).arg(m_groupBy);
    }

    storeCache(key, m_allEntries, m_title);

    const QVector<Entry> sortedEntries = filteredAndSortedEntries();
    beginResetModel();
    m_entries = sortedEntries;
    endResetModel();
    emit countChanged();

    if (m_searchFilter.isEmpty()) {
        CacheEntry *justCached = findCache(key);
        if (justCached) {
            justCached->sorted = sortedEntries;
            justCached->sortBy = activeSortKey;
            justCached->sortAscending = activeSortAscending;
        }
    }

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
    if (groupType == "performer") return track.performer;
    if (groupType == "composer") return track.composer;
    if (groupType == "originalyear") return track.originalYear > 0 ? QVariant(track.originalYear) : QVariant();
    if (groupType == "bpm") return track.bpm > 0 ? QVariant(track.bpm) : QVariant();
    if (groupType == "initialkey") return track.initialKey;
    if (groupType == "bitrate") return track.bitrate;
    if (groupType == "filetype") return track.fileType;
    if (isCustomGroupType(groupType)) {
        const QString key = customGroupKey(groupType);
        const QVariant value = track.customTags.value(key);
        if (!value.isValid())
            return QVariant();
        if (value.typeId() == QMetaType::QStringList) {
            const QStringList values = value.toStringList();
            return values.isEmpty() ? QVariant() : QVariant(values.first());
        }
        if (value.typeId() == QMetaType::QVariantList) {
            const QVariantList values = value.toList();
            return values.isEmpty() ? QVariant() : values.first();
        }
        const QString single = value.toString().trimmed();
        return single.isEmpty() ? QVariant() : QVariant(single);
    }
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
    if (groupType == "originalyear") {
        int year = value.toInt();
        return year > 0 ? QString::number(year) : "Unknown Year";
    }
    if (groupType == "bpm") {
        int bpm = value.toInt();
        return bpm > 0 ? QString("%1 BPM").arg(bpm) : "Unknown BPM";
    }

    QString str = value.toString();
    if (str.isEmpty()) {
        if (groupType == "albumartist") return "Unknown Artist";
        if (groupType == "artist") return "Unknown Artist";
        if (groupType == "album") return "Unknown Album";
        if (groupType == "genre") return "Unknown Genre";
        if (groupType == "performer") return "Unknown Performer";
        if (groupType == "composer") return "Unknown Composer";
        if (groupType == "initialkey") return "Unknown Key";
        if (groupType == "filetype") return "Unknown Type";
        return "Unknown";
    }
    return str;
}

QString CollectionBrowseModel::formatSubtitle(const Entry &entry) const
{
    const QString key = m_subtitleKey.trimmed();
    const QString yearText = entry.year > 0 ? QString::number(entry.year) : QString();
    const QString dateUpdatedText = formatModifiedDate(entry.modifiedTime);

    if (entry.entryType == QStringLiteral("group")) {
        const QString countText = QStringLiteral("%1 tracks").arg(entry.childCount);
        const QString durationText = formatDurationMs(entry.totalDurationMs);

        if (key == QStringLiteral("albumArtist"))
            return entry.artist;
        if (key == QStringLiteral("albumArtistYear"))
            return joinSubtitleParts(QStringList{entry.artist, yearText});
        if (key == QStringLiteral("albumArtistCount"))
            return joinSubtitleParts(QStringList{entry.artist, countText});
        if (key == QStringLiteral("yearAlbumArtist"))
            return joinSubtitleParts(QStringList{yearText, entry.artist});
        if (key == QStringLiteral("duration"))
            return durationText;
        if (key == QStringLiteral("countDuration"))
            return joinSubtitleParts(QStringList{countText, durationText});
        if (key == QStringLiteral("year"))
            return yearText;
        if (key == QStringLiteral("yearCount"))
            return joinSubtitleParts(QStringList{yearText, countText});
        if (key == QStringLiteral("dateUpdated"))
            return dateUpdatedText;
        return countText;
    }

    const QString durationText = formatDurationMs(entry.durationMs);
    const QString albumArtistText = entry.albumArtist.isEmpty() ? entry.artist : entry.albumArtist;
    const QString trackNumberText = entry.trackNumber > 0 ? QStringLiteral("%1").arg(entry.trackNumber) : QString();

    if (key == QStringLiteral("trackNumber"))
        return trackNumberText;
    if (key == QStringLiteral("trackNumberDuration"))
        return joinSubtitleParts(QStringList{trackNumberText, durationText});
    if (key == QStringLiteral("trackNumberAlbum"))
        return joinSubtitleParts(QStringList{trackNumberText, entry.album});
    if (key == QStringLiteral("artist"))
        return entry.artist;
    if (key == QStringLiteral("albumArtist"))
        return albumArtistText;
    if (key == QStringLiteral("album"))
        return entry.album;
    if (key == QStringLiteral("artistAlbum"))
        return joinSubtitleParts(QStringList{entry.artist, entry.album});
    if (key == QStringLiteral("albumArtistAlbum"))
        return joinSubtitleParts(QStringList{albumArtistText, entry.album});
    if (key == QStringLiteral("year"))
        return yearText;
    if (key == QStringLiteral("fileType"))
        return entry.fileType;
    if (key == QStringLiteral("bitrate"))
        return entry.bitrate > 0 ? QStringLiteral("%1 kbps").arg(entry.bitrate / 1000) : QString();
    if (key == QStringLiteral("dateUpdated"))
        return dateUpdatedText;
    return durationText;
}

void CollectionBrowseModel::applySubtitleToEntries(QVector<Entry> &entries) const
{
    for (Entry &entry : entries)
        entry.subtitle = formatSubtitle(entry);
}

QString CollectionBrowseModel::breadcrumbLabelForFilter(const TrackFilter &filter) const
{
    if (filter.isEmpty())
        return QString();

    const FilterCondition &cond = filter.constLast();
    return formatGroupDisplay(cond.field, cond.value);
}

void CollectionBrowseModel::buildGroups(const QVector<LibraryTrack> &tracks)
{
    struct GroupData {
        QVariant value;
        int count = 0;
        QString representativeFilePath;
        QStringList coverFilePaths;
        int year = 0;
        QString artist;
        QString album;
        qint64 totalDurationMs = 0;
        qint64 modifiedTime = 0;
        QVector<QVariantMap> tracks;
    };

    QMap<QString, GroupData> groups;

    for (const LibraryTrack &track : tracks) {
        const QVariant value = getGroupValue(track, m_groupBy);
        const QString key = normalizedGroupKey(m_groupBy, value);
        const QVariantMap trackMap = libraryTrackToVariantMap(track);

        if (!groups.contains(key)) {
            GroupData groupData;
            groupData.value = value;
            groupData.count = 1;
            groupData.representativeFilePath = track.filePath;
            if (!track.filePath.isEmpty())
                groupData.coverFilePaths.append(track.filePath);
            groupData.year = track.year;
            groupData.artist = track.albumArtist.isEmpty() ? track.artist : track.albumArtist;
            groupData.album = track.album;
            groupData.totalDurationMs = track.durationMs;
            groupData.modifiedTime = track.modifiedTime;
            groups.insert(key, groupData);
        } else {
            GroupData &groupData = groups[key];
            groupData.count++;
            if (!track.filePath.isEmpty() && !groupData.coverFilePaths.contains(track.filePath))
                groupData.coverFilePaths.append(track.filePath);
            groupData.totalDurationMs += track.durationMs;
            if (track.year > 0 && (groupData.year == 0 || track.year < groupData.year))
                groupData.year = track.year;
            if (track.modifiedTime > groupData.modifiedTime)
                groupData.modifiedTime = track.modifiedTime;
        }
    }

    QList<QString> keys = groups.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    for (const QString &key : keys) {
        const GroupData &groupData = groups.value(key);
        Entry entry;
        entry.entryType = QStringLiteral("group");
        entry.groupType = m_groupBy;
        entry.groupValue = groupData.value;
        entry.displayText = formatGroupDisplay(m_groupBy, groupData.value);
        entry.childCount = groupData.count;
        entry.representativeFilePath = groupData.representativeFilePath;
        entry.coverFilePaths = groupData.coverFilePaths;
        entry.year = groupData.year;
        entry.artist = groupData.artist;
        entry.album = groupData.album;
        entry.totalDurationMs = groupData.totalDurationMs;
        entry.modifiedTime = groupData.modifiedTime;
        m_allEntries.append(entry);
    }

    applySubtitleToEntries(m_allEntries);
}

void CollectionBrowseModel::buildTracks(const QVector<LibraryTrack> &tracks)
{
    for (const LibraryTrack &track : tracks) {
        Entry entry;
        entry.entryType = QStringLiteral("track");
        entry.filePath = track.filePath;
        entry.title = track.title.isEmpty() ? track.fileName : track.title;
        entry.displayText = entry.title;
        entry.artist = track.artist;
        entry.album = track.album;
        entry.albumArtist = track.albumArtist;
        entry.trackNumber = track.trackNumber;
        entry.discNumber = track.discNumber;
        entry.durationMs = track.durationMs;
        entry.genre = track.genre;
        entry.year = track.year;
        entry.bitrate = track.bitrate;
        entry.fileType = track.fileType;
        entry.trackData = libraryTrackToVariantMap(track);
        entry.representativeFilePath = track.filePath;
        if (!track.filePath.isEmpty())
            entry.coverFilePaths = { track.filePath };
        entry.modifiedTime = track.modifiedTime;
        m_allEntries.append(entry);
    }

    applySubtitleToEntries(m_allEntries);
}

QString CollectionBrowseModel::normalizedGroupKey(const QString &groupType, const QVariant &groupValue) const
{
    return groupType.trimmed() + QStringLiteral(":") + variantToSortText(groupValue);
}

void CollectionBrowseModel::setTrackListSort(const QString &sortKey, bool ascending)
{
    const QString normalizedKey = sortKey.trimmed();
    if (m_trackListSortKey == normalizedKey && m_trackListSortAscending == ascending)
        return;

    m_trackListSortKey = normalizedKey;
    m_trackListSortAscending = ascending;
    if (m_groupBy == QStringLiteral("none"))
        applySearchAndSort();
}

QStringList CollectionBrowseModel::displayedFilePaths() const
{
    if (!m_database)
        return {};

    QStringList filePaths;
    QSet<QString> seenFilePaths;

    for (const Entry &entry : m_entries) {
        if (entry.entryType == QStringLiteral("track")) {
            if (!entry.filePath.isEmpty() && !seenFilePaths.contains(entry.filePath)) {
                seenFilePaths.insert(entry.filePath);
                filePaths.append(entry.filePath);
            }
            continue;
        }

        if (entry.entryType != QStringLiteral("group") || entry.groupType.isEmpty())
            continue;

        TrackFilter trackFilter = m_filter;
        FilterCondition condition;
        condition.field = entry.groupType;
        condition.op = QStringLiteral("=");
        condition.value = entry.groupValue;
        trackFilter.append(condition);

        const QVector<LibraryTrack> tracks = m_database->tracksMatchingFilter(trackFilter);
        for (const LibraryTrack &track : tracks) {
            if (track.filePath.isEmpty() || seenFilePaths.contains(track.filePath))
                continue;
            seenFilePaths.insert(track.filePath);
            filePaths.append(track.filePath);
        }
    }

    return filePaths;
}

int CollectionBrowseModel::compareTrackMapsByKey(const QVariantMap &left, const QVariantMap &right, const QString &key)
{
    const QString normalizedKey = key.trimmed().toLower();
    if (normalizedKey == QStringLiteral("title") || normalizedKey == QStringLiteral("display"))
        return compareTextValue(left.value(QStringLiteral("title")).toString(),
                                right.value(QStringLiteral("title")).toString());
    if (normalizedKey == QStringLiteral("artist"))
        return compareTextValue(left.value(QStringLiteral("artist")).toString(),
                                right.value(QStringLiteral("artist")).toString());
    if (normalizedKey == QStringLiteral("album"))
        return compareTextValue(left.value(QStringLiteral("album")).toString(),
                                right.value(QStringLiteral("album")).toString());
    if (normalizedKey == QStringLiteral("albumartist"))
        return compareTextValue(left.value(QStringLiteral("albumArtist")).toString(),
                                right.value(QStringLiteral("albumArtist")).toString());
    if (normalizedKey == QStringLiteral("performer"))
        return compareTextValue(left.value(QStringLiteral("performer")).toString(),
                                right.value(QStringLiteral("performer")).toString());
    if (normalizedKey == QStringLiteral("composer"))
        return compareTextValue(left.value(QStringLiteral("composer")).toString(),
                                right.value(QStringLiteral("composer")).toString());
    if (normalizedKey == QStringLiteral("genre"))
        return compareTextValue(left.value(QStringLiteral("genre")).toString(),
                                right.value(QStringLiteral("genre")).toString());
    if (normalizedKey == QStringLiteral("filepath"))
        return compareTextValue(left.value(QStringLiteral("filePath")).toString(),
                                right.value(QStringLiteral("filePath")).toString());
    if (normalizedKey == QStringLiteral("filename"))
        return compareTextValue(left.value(QStringLiteral("fileName")).toString(),
                                right.value(QStringLiteral("fileName")).toString());
    if (normalizedKey == QStringLiteral("filetype"))
        return compareTextValue(left.value(QStringLiteral("fileType")).toString(),
                                right.value(QStringLiteral("fileType")).toString());
    if (normalizedKey == QStringLiteral("comment"))
        return compareTextValue(left.value(QStringLiteral("comment")).toString(),
                                right.value(QStringLiteral("comment")).toString());
    if (normalizedKey == QStringLiteral("initialkey"))
        return compareTextValue(left.value(QStringLiteral("initialKey")).toString(),
                                right.value(QStringLiteral("initialKey")).toString());
    if (normalizedKey == QStringLiteral("url"))
        return compareTextValue(left.value(QStringLiteral("url")).toString(),
                                right.value(QStringLiteral("url")).toString());
    if (normalizedKey == QStringLiteral("tracknumber"))
        return compareOptionalInt(left.value(QStringLiteral("trackNumber")).toInt(),
                                  right.value(QStringLiteral("trackNumber")).toInt());
    if (normalizedKey == QStringLiteral("discnumber"))
        return compareOptionalInt(left.value(QStringLiteral("discNumber")).toInt(),
                                  right.value(QStringLiteral("discNumber")).toInt());
    if (normalizedKey == QStringLiteral("year"))
        return compareOptionalInt(left.value(QStringLiteral("year")).toInt(),
                                  right.value(QStringLiteral("year")).toInt());
    if (normalizedKey == QStringLiteral("originalyear"))
        return compareOptionalInt(left.value(QStringLiteral("originalYear")).toInt(),
                                  right.value(QStringLiteral("originalYear")).toInt());
    if (normalizedKey == QStringLiteral("samplerate"))
        return compareOptionalInt(left.value(QStringLiteral("sampleRate")).toInt(),
                                  right.value(QStringLiteral("sampleRate")).toInt());
    if (normalizedKey == QStringLiteral("bitdepth"))
        return compareOptionalInt(left.value(QStringLiteral("bitDepth")).toInt(),
                                  right.value(QStringLiteral("bitDepth")).toInt());
    if (normalizedKey == QStringLiteral("bitrate"))
        return compareOptionalInt(left.value(QStringLiteral("bitrate")).toInt(),
                                  right.value(QStringLiteral("bitrate")).toInt());
    if (normalizedKey == QStringLiteral("bpm"))
        return compareOptionalInt(left.value(QStringLiteral("bpm")).toInt(),
                                  right.value(QStringLiteral("bpm")).toInt());
    if (normalizedKey == QStringLiteral("durationms"))
        return compareOptionalLongLong(left.value(QStringLiteral("durationMs")).toLongLong(),
                                       right.value(QStringLiteral("durationMs")).toLongLong());
    if (normalizedKey == QStringLiteral("filesize"))
        return compareOptionalLongLong(left.value(QStringLiteral("fileSize")).toLongLong(),
                                       right.value(QStringLiteral("fileSize")).toLongLong());
    if (normalizedKey == QStringLiteral("datecreated"))
        return compareDateTimeValue(left.value(QStringLiteral("dateCreated")).toDateTime(),
                                    right.value(QStringLiteral("dateCreated")).toDateTime());
    if (normalizedKey == QStringLiteral("datemodified"))
        return compareDateTimeValue(left.value(QStringLiteral("dateModified")).toDateTime(),
                                    right.value(QStringLiteral("dateModified")).toDateTime());
    if (normalizedKey.startsWith(QStringLiteral("custom:")))
        return compareTextValue(customTagSortText(left, normalizedKey.mid(7)),
                                customTagSortText(right, normalizedKey.mid(7)));
    return compareTextValue(left.value(QStringLiteral("title")).toString(),
                            right.value(QStringLiteral("title")).toString());
}

QStringList CollectionBrowseModel::defaultTrackSortKeys(const QString &groupType)
{
    const QString normalizedGroupType = groupType.trimmed();
    if (normalizedGroupType == QStringLiteral("artist") || normalizedGroupType == QStringLiteral("albumartist"))
        return { QStringLiteral("trackNumber"), QStringLiteral("album") };
    if (normalizedGroupType == QStringLiteral("album"))
        return { QStringLiteral("trackNumber") };
    return { QStringLiteral("title") };
}
