#ifndef COLLECTIONBROWSEHELPER_H
#define COLLECTIONBROWSEHELPER_H

#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <QVector>
#include "TrackFilter.h"

struct LibraryTrack;

namespace CollectionBrowseHelper {

// --- Entry: the display unit for the collection browser model ---

struct Entry {
    QString entryType;
    QString groupType;
    QVariant groupValue;
    QString displayText;
    QString subtitle;
    int childCount = 0;
    QString representativeFilePath;
    QStringList coverFilePaths;
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
    QVariantMap trackData;
    qint64 totalDurationMs = 0;
    qint64 modifiedTime = 0;
};

// --- Vocabulary: group/sort/subtitle key interpretation ---

QVariant getGroupValue(const LibraryTrack &track, const QString &groupType);
QString formatGroupDisplay(const QString &groupType, const QVariant &value);
QString formatSubtitle(const Entry &entry, const QString &subtitleKey);
void applySubtitleToEntries(QVector<Entry> &entries, const QString &subtitleKey);
QString normalizedGroupKey(const QString &groupType, const QVariant &groupValue);
QString breadcrumbLabelForFilter(const TrackFilter &filter);
int compareTrackMapsByKey(const QVariantMap &left, const QVariantMap &right, const QString &key);

// --- Projection: LibraryTrack → Entry transformation ---

QVector<Entry> buildGroups(const QVector<LibraryTrack> &tracks, const QString &groupBy,
                           const QString &subtitleKey);
QVector<Entry> buildTracks(const QVector<LibraryTrack> &tracks, const QString &subtitleKey);
QVector<Entry> filteredAndSortedEntries(const QVector<Entry> &allEntries,
                                        const QString &searchFilter,
                                        const QString &sortBy, bool sortAscending,
                                        const QString &groupBy);

// --- Option lists: vocabulary for toolbar menus ---

QVariantList sortOptions(const QString &groupBy);
QVariantList subtitleOptions(const QString &groupBy);
QVariantList groupByOptions(const QStringList &customTagKeys);

// --- Utility ---

QVariantMap libraryTrackToVariantMap(const LibraryTrack &track);
QString formatDurationMs(qint64 ms);

} // namespace CollectionBrowseHelper

#endif // COLLECTIONBROWSEHELPER_H
