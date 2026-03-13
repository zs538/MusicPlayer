#include "CollectionBrowseModel.h"
#include "CollectionBrowseHelper.h"
#include "library/LibraryDatabase.h"
#include <algorithm>

namespace CBH = CollectionBrowseHelper;

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
        item.insert(QStringLiteral("label"), isHome ? QString() : CBH::breadcrumbLabelForFilter(trail[i].filter));
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
    const QString key = CBH::normalizedGroupKey(groupType, groupValue);
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries[i];
        if (e.entryType == QStringLiteral("group") &&
            CBH::normalizedGroupKey(e.groupType, e.groupValue) == key)
            return i;
    }
    return -1;
}

QString CollectionBrowseModel::entryIdForEntry(const Entry &entry) const
{
    if (entry.entryType == QStringLiteral("group"))
        return QStringLiteral("g:%1:%2").arg(entry.groupType, CBH::normalizedGroupKey(entry.groupType, entry.groupValue));
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

QVariantList CollectionBrowseModel::sortOptions() const
{
    return CBH::sortOptions(m_groupBy);
}

QVariantList CollectionBrowseModel::subtitleOptions() const
{
    return CBH::subtitleOptions(m_groupBy);
}

QVariantList CollectionBrowseModel::groupByOptions(const QStringList &customTagKeys) const
{
    return CBH::groupByOptions(customTagKeys);
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
        CBH::applySubtitleToEntries(cacheEntry.entries, m_subtitleKey);
        CBH::applySubtitleToEntries(cacheEntry.sorted, m_subtitleKey);
    }

    CBH::applySubtitleToEntries(m_allEntries, m_subtitleKey);
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

void CollectionBrowseModel::applySearchAndSort()
{
    beginResetModel();
    m_entries = CBH::filteredAndSortedEntries(m_allEntries, m_searchFilter, m_sortBy, m_sortAscending, m_groupBy);
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
        CBH::applySubtitleToEntries(m_allEntries, m_subtitleKey);
        cached->entries = m_allEntries;
        m_title = cached->title;

        if (!cached->sorted.isEmpty())
            CBH::applySubtitleToEntries(cached->sorted, m_subtitleKey);

        if (!cached->sorted.isEmpty() &&
            cached->sortBy == activeSortKey &&
            cached->sortAscending == activeSortAscending &&
            m_searchFilter.isEmpty()) {
            beginResetModel();
            m_entries = cached->sorted;
            endResetModel();
            emit countChanged();
        } else {
            const QVector<Entry> sortedEntries = CBH::filteredAndSortedEntries(m_allEntries, m_searchFilter, m_sortBy, m_sortAscending, m_groupBy);
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

    if (m_groupBy.isEmpty() || m_groupBy == QStringLiteral("none")) {
        m_allEntries = CBH::buildTracks(tracks, m_subtitleKey);
        m_title = QStringLiteral("%1 tracks").arg(tracks.size());
    } else {
        m_allEntries = CBH::buildGroups(tracks, m_groupBy, m_subtitleKey);
        m_title = QStringLiteral("%1 %2s").arg(m_allEntries.size()).arg(m_groupBy);
    }

    storeCache(key, m_allEntries, m_title);

    {
        const QVector<Entry> sortedEntries = CBH::filteredAndSortedEntries(m_allEntries, m_searchFilter, m_sortBy, m_sortAscending, m_groupBy);
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
}

void CollectionBrowseModel::setTrackListSort(const QString &sortKey, bool ascending)
{
    const QString normalizedKey = sortKey.trimmed();
    if (m_trackListSortKey == normalizedKey && m_trackListSortAscending == ascending)
        return;

    m_trackListSortKey = normalizedKey;
    m_trackListSortAscending = ascending;
    if (m_groupBy == QStringLiteral("none")) {
        beginResetModel();
        m_entries = CBH::filteredAndSortedEntries(m_allEntries, m_searchFilter, m_sortBy, m_sortAscending, m_groupBy);
        endResetModel();
        emit countChanged();
    }
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

