#ifndef COLLECTIONBROWSEMODEL_H
#define COLLECTIONBROWSEMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <QStringList>
#include <QPair>
#include <QSet>
#include <QHash>
#include "TrackFilter.h"
#include "CollectionBrowseHelper.h"

class LibraryDatabase;

using BrowseEntry = CollectionBrowseHelper::Entry;

class CollectionBrowseModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(LibraryDatabase* database READ database WRITE setDatabase NOTIFY databaseChanged)
    Q_PROPERTY(QVariantList filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString groupBy READ groupBy WRITE setGroupBy NOTIFY groupByChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(QString subtitleKey READ subtitleKey WRITE setSubtitleKey NOTIFY subtitleKeyChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(qreal pendingScrollY READ pendingScrollY NOTIFY pendingScrollYChanged)
    Q_PROPERTY(QString pendingSelectedEntryId READ pendingSelectedEntryId NOTIFY pendingSelectedEntryIdChanged)
    Q_PROPERTY(QVariantList breadcrumbPath READ breadcrumbPath NOTIFY historyChanged)
    Q_PROPERTY(int currentBreadcrumbIndex READ currentBreadcrumbIndex NOTIFY historyChanged)

public:
    enum Roles {
        EntryTypeRole = Qt::UserRole + 1,
        GroupTypeRole,
        GroupValueRole,
        DisplayTextRole,
        SubtitleRole,
        ChildCountRole,
        RepresentativeFilePathRole,
        CoverFilePathsRole,
        ImagePathRole,
        FilePathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationMsRole,
        GenreRole,
        YearRole,
        BitrateRole,
        FileTypeRole,
        TrackDataRole,
        TotalDurationMsRole
    };

    explicit CollectionBrowseModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    LibraryDatabase *database() const { return m_database; }
    void setDatabase(LibraryDatabase *db);

    QVariantList filter() const { return trackFilterToVariant(m_filter); }
    void setFilter(const QVariantList &filter);

    QString groupBy() const { return m_groupBy; }
    void setGroupBy(const QString &groupBy);

    QString sortBy() const { return m_sortBy; }
    void setSortBy(const QString &sortBy);

    bool sortAscending() const { return m_sortAscending; }
    void setSortAscending(bool ascending);

    QString subtitleKey() const { return m_subtitleKey; }
    void setSubtitleKey(const QString &subtitleKey);

    QString searchFilter() const { return m_searchFilter; }
    void setSearchFilter(const QString &filter);

    int count() const { return m_entries.size(); }
    QString title() const { return m_title; }

    Q_INVOKABLE void setTrackListSort(const QString &sortKey, bool ascending = true);
    Q_INVOKABLE QStringList displayedFilePaths() const;
    Q_INVOKABLE QString entryIdAt(int row) const;
    Q_INVOKABLE int indexOfEntryId(const QString &entryId) const;

    // Vocabulary option lists for toolbar menus
    Q_INVOKABLE QVariantList sortOptions() const;
    Q_INVOKABLE QVariantList subtitleOptions() const;
    Q_INVOKABLE QVariantList groupByOptions(const QStringList &customTagKeys) const;

    // History navigation — atomic filter+groupBy change, single refresh
    Q_INVOKABLE void navigate(const QVariantList &filter, const QString &groupBy, qreal currentScrollY = 0, const QString &currentSelectedEntryId = QString());
    Q_INVOKABLE void goBack(qreal currentScrollY = 0, const QString &currentSelectedEntryId = QString());
    Q_INVOKABLE void goForward(qreal currentScrollY = 0, const QString &currentSelectedEntryId = QString());
    Q_INVOKABLE void jumpToBreadcrumb(int index, qreal currentScrollY = 0, const QString &currentSelectedEntryId = QString());
    bool canGoBack() const;
    bool canGoForward() const;
    qreal pendingScrollY() const { return m_pendingScrollY; }
    QString pendingSelectedEntryId() const { return m_pendingSelectedEntryId; }
    QVariantList breadcrumbPath() const;
    int currentBreadcrumbIndex() const { return m_backStack.size(); }

signals:
    void databaseChanged();
    void filterChanged();
    void groupByChanged();
    void sortByChanged();
    void sortAscendingChanged();
    void subtitleKeyChanged();
    void searchFilterChanged();
    void countChanged();
    void titleChanged();
    void historyChanged();
    void pendingScrollYChanged();
    void pendingSelectedEntryIdChanged();

private slots:
    void refresh();

private:
    using Entry = BrowseEntry;

    // Flat list building
    void applySearchAndSort();

    // Find the row of a group entry in m_entries, and the range of its expanded children
    int findGroupRow(const QString &groupType, const QVariant &groupValue) const;

    // LRU result cache — keyed by filter+groupBy, stores pre-search/sort entries
    struct CacheKey {
        TrackFilter filter;
        QString groupBy;
        bool operator==(const CacheKey &o) const;
    };
    struct CacheEntry {
        CacheKey key;
        QVector<Entry> entries;       // pre-search/sort (m_allEntries)
        QVector<Entry> sorted;        // post-sort, no search filter
        QString sortBy;
        bool sortAscending = true;
        QString title;
    };
    QString entryIdForEntry(const Entry &entry) const;
    static constexpr int MaxCacheEntries = 16;
    QList<CacheEntry> m_cache;
    CacheKey currentCacheKey() const;
    CacheEntry *findCache(const CacheKey &key);
    void storeCache(const CacheKey &key, const QVector<Entry> &entries, const QString &title);
    void invalidateCache();

    struct HistoryEntry { TrackFilter filter; QString groupBy; qreal scrollY = 0; QString selectedEntryId; };
    QVector<HistoryEntry> historyTrail(qreal currentScrollY, const QString &currentSelectedEntryId) const;
    QVector<HistoryEntry> m_backStack;
    QVector<HistoryEntry> m_forwardStack;
    qreal m_pendingScrollY = 0;
    QString m_pendingSelectedEntryId;
    void applyState(const TrackFilter &filter, const QString &groupBy);

    LibraryDatabase *m_database = nullptr;
    TrackFilter m_filter;
    QString m_groupBy = "albumartist";
    QString m_sortBy = "name";  // "name", "year", "duration", "count", "dateUpdated"
    bool m_sortAscending = true;
    QString m_subtitleKey = "count";
    QString m_trackListSortKey;
    bool m_trackListSortAscending = true;
    QString m_searchFilter;
    QString m_title;
    QVector<Entry> m_entries;     // The flat displayed list
    QVector<Entry> m_allEntries;  // Pre-search-filter entries (groups or tracks, no expanded children)
};

#endif // COLLECTIONBROWSEMODEL_H
