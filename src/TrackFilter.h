#ifndef TRACKFILTER_H
#define TRACKFILTER_H

#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct FilterCondition {
    QString field;
    QString op = "=";
    QVariant value;
};

using TrackFilter = QVector<FilterCondition>;

inline TrackFilter trackFilterFromVariant(const QVariantList &list)
{
    TrackFilter filter;
    for (const QVariant &item : list) {
        QVariantMap map = item.toMap();
        FilterCondition cond;
        cond.field = map.value("field").toString();
        cond.op = map.value("op", "=").toString();
        cond.value = map.value("value");
        if (!cond.field.isEmpty())
            filter.append(cond);
    }
    return filter;
}

inline QVariantList trackFilterToVariant(const TrackFilter &filter)
{
    QVariantList list;
    for (const FilterCondition &cond : filter) {
        QVariantMap map;
        map["field"] = cond.field;
        map["op"] = cond.op;
        map["value"] = cond.value;
        list.append(map);
    }
    return list;
}

inline bool isCustomGroupType(const QString &groupType)
{
    return groupType.startsWith("custom:") && groupType.size() > 7;
}

inline QString customGroupKey(const QString &groupType)
{
    return isCustomGroupType(groupType) ? groupType.mid(7).trimmed().toUpper() : QString();
}

inline QString groupTypeToSparseAttributeKey(const QString &groupType)
{
    if (groupType == "performer") return "_performer";
    if (groupType == "composer") return "_composer";
    if (groupType == "originalyear") return "_original_year";
    if (groupType == "bpm") return "_bpm";
    if (groupType == "initialkey") return "_initial_key";
    return QString();
}

inline QString groupTypeToColumn(const QString &groupType)
{
    // Use COALESCE for albumartist to fall back to artist when album_artist is empty
    if (groupType == "albumartist") return "COALESCE(NULLIF(album_artist, ''), artist)";
    if (groupType == "artist") return "artist";
    if (groupType == "album") return "album";
    if (groupType == "disc") return "disc_number";
    if (groupType == "genre") return "genre";
    if (groupType == "year") return "year";
    if (groupType == "bitrate") return "bitrate";
    if (groupType == "filetype") return "file_type";
    return QString();
}

#endif // TRACKFILTER_H
