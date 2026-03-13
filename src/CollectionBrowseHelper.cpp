#include "CollectionBrowseHelper.h"
#include "library/LibraryDatabase.h"
#include <QDateTime>
#include <QMap>
#include <algorithm>

namespace {

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

QVariantMap optionItem(const QString &text, const QString &key)
{
    return {
        {QStringLiteral("text"), text},
        {QStringLiteral("key"), key},
    };
}

QVariantMap categorizedOptionItem(const QString &text, const QString &key, const QString &category)
{
    QVariantMap option = optionItem(text, key);
    option.insert(QStringLiteral("category"), category);
    return option;
}

} // anonymous namespace

namespace CollectionBrowseHelper {

// --- Utility ---

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

// --- Vocabulary ---

QVariant getGroupValue(const LibraryTrack &track, const QString &groupType)
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

QString formatGroupDisplay(const QString &groupType, const QVariant &value)
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

QString formatSubtitle(const Entry &entry, const QString &subtitleKey)
{
    const QString key = subtitleKey.trimmed();
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

void applySubtitleToEntries(QVector<Entry> &entries, const QString &subtitleKey)
{
    for (Entry &entry : entries)
        entry.subtitle = formatSubtitle(entry, subtitleKey);
}

QString normalizedGroupKey(const QString &groupType, const QVariant &groupValue)
{
    return groupType.trimmed() + QStringLiteral(":") + variantToSortText(groupValue);
}

QString breadcrumbLabelForFilter(const TrackFilter &filter)
{
    if (filter.isEmpty())
        return QString();

    const FilterCondition &cond = filter.constLast();
    return formatGroupDisplay(cond.field, cond.value);
}

int compareTrackMapsByKey(const QVariantMap &left, const QVariantMap &right, const QString &key)
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

// --- Projection ---

QVector<Entry> buildGroups(const QVector<LibraryTrack> &tracks, const QString &groupBy,
                           const QString &subtitleKey)
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
    };

    QMap<QString, GroupData> groups;

    for (const LibraryTrack &track : tracks) {
        const QVariant value = getGroupValue(track, groupBy);
        const QString key = normalizedGroupKey(groupBy, value);

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

    QVector<Entry> entries;
    entries.reserve(keys.size());

    for (const QString &key : keys) {
        const GroupData &groupData = groups.value(key);
        Entry entry;
        entry.entryType = QStringLiteral("group");
        entry.groupType = groupBy;
        entry.groupValue = groupData.value;
        entry.displayText = formatGroupDisplay(groupBy, groupData.value);
        entry.childCount = groupData.count;
        entry.representativeFilePath = groupData.representativeFilePath;
        entry.coverFilePaths = groupData.coverFilePaths;
        entry.year = groupData.year;
        entry.artist = groupData.artist;
        entry.album = groupData.album;
        entry.totalDurationMs = groupData.totalDurationMs;
        entry.modifiedTime = groupData.modifiedTime;
        entries.append(entry);
    }

    applySubtitleToEntries(entries, subtitleKey);
    return entries;
}

QVector<Entry> buildTracks(const QVector<LibraryTrack> &tracks, const QString &subtitleKey)
{
    QVector<Entry> entries;
    entries.reserve(tracks.size());

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
        entries.append(entry);
    }

    applySubtitleToEntries(entries, subtitleKey);
    return entries;
}

QVector<Entry> filteredAndSortedEntries(const QVector<Entry> &allEntries,
                                        const QString &searchFilter,
                                        const QString &sortBy, bool sortAscending,
                                        const QString &groupBy)
{
    QVector<Entry> entries = allEntries;

    if (!searchFilter.isEmpty()) {
        QVector<Entry> filtered;
        filtered.reserve(entries.size());
        for (const Entry &entry : entries) {
            if (entry.displayText.contains(searchFilter, Qt::CaseInsensitive) ||
                entry.subtitle.contains(searchFilter, Qt::CaseInsensitive) ||
                entry.artist.contains(searchFilter, Qt::CaseInsensitive) ||
                entry.album.contains(searchFilter, Qt::CaseInsensitive)) {
                filtered.append(entry);
            }
        }
        entries = filtered;
    }

    if (groupBy == QStringLiteral("none")) {
        QString trackSortKey = sortBy;
        if (trackSortKey == QStringLiteral("name"))
            trackSortKey = QStringLiteral("title");
        else if (trackSortKey == QStringLiteral("duration"))
            trackSortKey = QStringLiteral("durationMs");
        else if (trackSortKey == QStringLiteral("dateUpdated"))
            trackSortKey = QStringLiteral("dateModified");
        else if (trackSortKey == QStringLiteral("count"))
            trackSortKey = QStringLiteral("title");

        std::stable_sort(entries.begin(), entries.end(), [sortAscending, trackSortKey](const Entry &left, const Entry &right) {
            int cmp = compareTrackMapsByKey(left.trackData, right.trackData, trackSortKey);
            if (cmp == 0)
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            return sortAscending ? (cmp < 0) : (cmp > 0);
        });
    } else {
        std::sort(entries.begin(), entries.end(), [sortBy, sortAscending](const Entry &left, const Entry &right) {
            int cmp = 0;
            if (sortBy == QStringLiteral("name")) {
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            } else if (sortBy == QStringLiteral("year")) {
                cmp = (left.year > right.year) - (left.year < right.year);
            } else if (sortBy == QStringLiteral("duration")) {
                const qint64 durationLeft = left.entryType == QStringLiteral("group") ? left.totalDurationMs : left.durationMs;
                const qint64 durationRight = right.entryType == QStringLiteral("group") ? right.totalDurationMs : right.durationMs;
                cmp = (durationLeft > durationRight) - (durationLeft < durationRight);
            } else if (sortBy == QStringLiteral("count")) {
                cmp = (left.childCount > right.childCount) - (left.childCount < right.childCount);
            } else if (sortBy == QStringLiteral("dateUpdated")) {
                cmp = (left.modifiedTime > right.modifiedTime) - (left.modifiedTime < right.modifiedTime);
            } else {
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            }
            if (cmp == 0)
                cmp = left.displayText.compare(right.displayText, Qt::CaseInsensitive);
            return sortAscending ? (cmp < 0) : (cmp > 0);
        });
    }

    return entries;
}

QVariantList sortOptions(const QString &groupBy)
{
    if (groupBy == QStringLiteral("none")) {
        return {
            optionItem(QStringLiteral("Name"), QStringLiteral("name")),
            optionItem(QStringLiteral("Track Number"), QStringLiteral("trackNumber")),
            optionItem(QStringLiteral("Year"), QStringLiteral("year")),
            optionItem(QStringLiteral("Duration"), QStringLiteral("duration")),
            optionItem(QStringLiteral("Date Updated"), QStringLiteral("dateUpdated")),
        };
    }
    return {
        optionItem(QStringLiteral("Name"), QStringLiteral("name")),
        optionItem(QStringLiteral("Year"), QStringLiteral("year")),
        optionItem(QStringLiteral("Duration"), QStringLiteral("duration")),
        optionItem(QStringLiteral("Track Count"), QStringLiteral("count")),
        optionItem(QStringLiteral("Date Updated"), QStringLiteral("dateUpdated")),
    };
}

QVariantList subtitleOptions(const QString &groupBy)
{
    if (groupBy == QStringLiteral("none")) {
        return {
            optionItem(QStringLiteral("Track"), QStringLiteral("trackNumber")),
            optionItem(QStringLiteral("Duration"), QStringLiteral("duration")),
            optionItem(QStringLiteral("Track - Duration"), QStringLiteral("trackNumberDuration")),
            optionItem(QStringLiteral("Year"), QStringLiteral("year")),
            optionItem(QStringLiteral("Date Updated"), QStringLiteral("dateUpdated")),
            optionItem(QStringLiteral("Artist"), QStringLiteral("artist")),
            optionItem(QStringLiteral("Album Artist"), QStringLiteral("albumArtist")),
            optionItem(QStringLiteral("Album"), QStringLiteral("album")),
            optionItem(QStringLiteral("Track - Album"), QStringLiteral("trackNumberAlbum")),
            optionItem(QStringLiteral("Artist - Album"), QStringLiteral("artistAlbum")),
            optionItem(QStringLiteral("Album Artist - Album"), QStringLiteral("albumArtistAlbum")),
            optionItem(QStringLiteral("File Type"), QStringLiteral("fileType")),
            optionItem(QStringLiteral("Bitrate"), QStringLiteral("bitrate")),
        };
    }
    return {
        optionItem(QStringLiteral("Track Count"), QStringLiteral("count")),
        optionItem(QStringLiteral("Duration"), QStringLiteral("duration")),
        optionItem(QStringLiteral("Tracks - Duration"), QStringLiteral("countDuration")),
        optionItem(QStringLiteral("Album Artist"), QStringLiteral("albumArtist")),
        optionItem(QStringLiteral("Album Artist - Year"), QStringLiteral("albumArtistYear")),
        optionItem(QStringLiteral("Album Artist - Tracks"), QStringLiteral("albumArtistCount")),
        optionItem(QStringLiteral("Year - Album Artist"), QStringLiteral("yearAlbumArtist")),
        optionItem(QStringLiteral("Year"), QStringLiteral("year")),
        optionItem(QStringLiteral("Year - Tracks"), QStringLiteral("yearCount")),
        optionItem(QStringLiteral("Date Updated"), QStringLiteral("dateUpdated")),
    };
}

QVariantList groupByOptions(const QStringList &customTagKeys)
{
    QVariantList options = {
        categorizedOptionItem(QStringLiteral("Artist"), QStringLiteral("artist"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("Album Artist"), QStringLiteral("albumartist"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("Album"), QStringLiteral("album"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("Genre"), QStringLiteral("genre"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("Year"), QStringLiteral("year"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("None (Tracks)"), QStringLiteral("none"), QStringLiteral("main")),
        categorizedOptionItem(QStringLiteral("Disc"), QStringLiteral("disc"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("Performer"), QStringLiteral("performer"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("Composer"), QStringLiteral("composer"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("Original Year"), QStringLiteral("originalyear"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("BPM"), QStringLiteral("bpm"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("Initial Key"), QStringLiteral("initialkey"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("Bitrate"), QStringLiteral("bitrate"), QStringLiteral("other")),
        categorizedOptionItem(QStringLiteral("File Type"), QStringLiteral("filetype"), QStringLiteral("other")),
    };

    for (const QString &tag : customTagKeys)
        options.append(categorizedOptionItem(tag, QStringLiteral("custom:") + tag, QStringLiteral("custom")));

    return options;
}

} // namespace CollectionBrowseHelper
