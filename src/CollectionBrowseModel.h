#ifndef COLLECTIONBROWSEMODEL_H
#define COLLECTIONBROWSEMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>
#include <QList>
#include <QPair>
#include "TrackFilter.h"

class LibraryDatabase;

class CollectionBrowseModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(LibraryDatabase* database READ database WRITE setDatabase NOTIFY databaseChanged)
    Q_PROPERTY(QVariantList filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString groupBy READ groupBy WRITE setGroupBy NOTIFY groupByChanged)
    Q_PROPERTY(QString sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(qreal pendingScrollY READ pendingScrollY NOTIFY pendingScrollYChanged)

public:
    enum Roles {
        EntryTypeRole = Qt::UserRole + 1,
        GroupTypeRole,
        GroupValueRole,
        DisplayTextRole,
        SubtitleRole,
        ChildCountRole,
        RepresentativeFilePathRole,
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

    QString searchFilter() const { return m_searchFilter; }
    void setSearchFilter(const QString &filter);

    int count() const { return m_entries.size(); }
    QString title() const { return m_title; }

    Q_INVOKABLE QVariantList tracksForGroup(const QString &groupType, const QVariant &groupValue) const;

    // History navigation — atomic filter+groupBy change, single refresh
    Q_INVOKABLE void navigate(const QVariantList &filter, const QString &groupBy, qreal currentScrollY = 0);
    Q_INVOKABLE void goBack(qreal currentScrollY = 0);
    Q_INVOKABLE void goForward(qreal currentScrollY = 0);
    bool canGoBack() const;
    bool canGoForward() const;
    qreal pendingScrollY() const { return m_pendingScrollY; }

signals:
    void databaseChanged();
    void filterChanged();
    void groupByChanged();
    void sortByChanged();
    void sortAscendingChanged();
    void searchFilterChanged();
    void countChanged();
    void titleChanged();
    void historyChanged();
    void pendingScrollYChanged();

private slots:
    void refresh();

private:
    struct Entry {
        QString entryType;
        QString groupType;
        QVariant groupValue;
        QString displayText;
        QString subtitle;
        int childCount = 0;
        QString representativeFilePath;
        QString imagePath;
        QString filePath;
        QString title;
        QString artist;
        QString album;
        QString albumArtist;
        int trackNumber = 0;
        int discNumber = 0;
        qint64 durationMs = 0;
        QString genre;
        int year = 0;
        int bitrate = 0;
        QString fileType;
        qint64 totalDurationMs = 0;
    };

    void swapEntries(const QVector<Entry> &newEntries);
    void applySearchAndSort(bool forceReset = false);
    void applyIncrementalUpdate(const QVector<Entry> &newEntries);
    static QString entryKey(const Entry &e);
    static bool entriesEqual(const Entry &a, const Entry &b);
    void buildGroups(const QVector<struct LibraryTrack> &tracks);
    void buildTracks(const QVector<struct LibraryTrack> &tracks);
    QString formatGroupDisplay(const QString &groupType, const QVariant &value) const;
    QVariant getGroupValue(const struct LibraryTrack &track, const QString &groupType) const;

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
    static constexpr int MaxCacheEntries = 16;
    QList<CacheEntry> m_cache;
    CacheKey currentCacheKey() const;
    CacheEntry *findCache(const CacheKey &key);
    void storeCache(const CacheKey &key, const QVector<Entry> &entries, const QString &title);
    void invalidateCache();

    struct HistoryEntry { TrackFilter filter; QString groupBy; qreal scrollY = 0; };
    QVector<HistoryEntry> m_backStack;
    QVector<HistoryEntry> m_forwardStack;
    qreal m_pendingScrollY = 0;
    void applyState(const TrackFilter &filter, const QString &groupBy);

    LibraryDatabase *m_database = nullptr;
    TrackFilter m_filter;
    QString m_groupBy = "albumartist";
    QString m_sortBy = "name";  // "name", "year", "count", "artist", "album"
    bool m_sortAscending = true;
    QString m_searchFilter;
    QString m_title;
    QVector<Entry> m_entries;
    QVector<Entry> m_allEntries;  // Pre-search-filter entries for filtering
};

#endif // COLLECTIONBROWSEMODEL_H
