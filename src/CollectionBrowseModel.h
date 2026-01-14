#ifndef COLLECTIONBROWSEMODEL_H
#define COLLECTIONBROWSEMODEL_H

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>
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
        FileTypeRole
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

signals:
    void databaseChanged();
    void filterChanged();
    void groupByChanged();
    void sortByChanged();
    void sortAscendingChanged();
    void searchFilterChanged();
    void countChanged();
    void titleChanged();

private slots:
    void refresh();

private:
    void applySearchAndSort();
    void buildGroups(const QVector<struct LibraryTrack> &tracks);
    void buildTracks(const QVector<struct LibraryTrack> &tracks);
    QString formatGroupDisplay(const QString &groupType, const QVariant &value) const;
    QVariant getGroupValue(const struct LibraryTrack &track, const QString &groupType) const;

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
    };

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
