#include "LibrarySparseAttributes.h"
#include "LibraryDatabase.h"
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <algorithm>

namespace LibrarySparseAttributes {

QString normalizedSparseKey(const QString &key)
{
    return key.trimmed().toUpper();
}

QStringList variantToStringList(const QVariant &value)
{
    if (!value.isValid())
        return {};
    if (value.typeId() == QMetaType::QStringList)
        return value.toStringList();
    if (value.typeId() == QMetaType::QVariantList) {
        QStringList values;
        const QVariantList list = value.toList();
        values.reserve(list.size());
        for (const QVariant &item : list) {
            const QString text = item.toString().trimmed();
            if (!text.isEmpty())
                values.append(text);
        }
        return values;
    }
    const QString single = value.toString().trimmed();
    return single.isEmpty() ? QStringList{} : QStringList{single};
}

void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key, const QString &value)
{
    const QString normalizedValue = value.trimmed();
    if (normalizedValue.isEmpty())
        return;
    attributes.append({key, normalizedValue});
}

void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key, qint64 value)
{
    if (value <= 0)
        return;
    attributes.append({key, QString::number(value)});
}

QVector<QPair<QString, QString>> sparseAttributesForTrack(const LibraryTrack &track)
{
    QVector<QPair<QString, QString>> attributes;
    appendSparseAttribute(attributes, QStringLiteral("_performer"), track.performer);
    appendSparseAttribute(attributes, QStringLiteral("_composer"), track.composer);
    appendSparseAttribute(attributes, QStringLiteral("_original_year"), track.originalYear);
    appendSparseAttribute(attributes, QStringLiteral("_sample_rate"), track.sampleRate);
    appendSparseAttribute(attributes, QStringLiteral("_bit_depth"), track.bitDepth);
    appendSparseAttribute(attributes, QStringLiteral("_channels"), track.channels);
    appendSparseAttribute(attributes, QStringLiteral("_url"), track.url);
    appendSparseAttribute(attributes, QStringLiteral("_created_time"), track.createdTime);
    appendSparseAttribute(attributes, QStringLiteral("_comment"), track.comment);
    appendSparseAttribute(attributes, QStringLiteral("_bpm"), track.bpm);
    appendSparseAttribute(attributes, QStringLiteral("_initial_key"), track.initialKey);
    appendSparseAttribute(attributes, QStringLiteral("_codec"), track.codec);

    for (auto it = track.customTags.constBegin(); it != track.customTags.constEnd(); ++it) {
        const QString nKey = normalizedSparseKey(it.key());
        if (nKey.isEmpty() || nKey.startsWith('_'))
            continue;
        const QStringList values = variantToStringList(it.value());
        for (const QString &v : values)
            appendSparseAttribute(attributes, nKey, v);
    }

    return attributes;
}

bool replaceTrackAttributes(QSqlDatabase &db, qint64 trackId,
                            const QVector<QPair<QString, QString>> &attributes,
                            const char *context)
{
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM track_attributes WHERE track_id = :track_id");
    deleteQuery.bindValue(":track_id", trackId);
    if (!deleteQuery.exec()) {
        qWarning() << context << deleteQuery.lastError().text();
        return false;
    }

    if (attributes.isEmpty())
        return true;

    QSqlQuery insertQuery(db);
    insertQuery.prepare("INSERT INTO track_attributes (track_id, key, value) VALUES (:track_id, :key, :value)");
    for (const auto &attribute : attributes) {
        insertQuery.bindValue(":track_id", trackId);
        insertQuery.bindValue(":key", attribute.first);
        insertQuery.bindValue(":value", attribute.second);
        if (!insertQuery.exec()) {
            qWarning() << context << insertQuery.lastError().text();
            return false;
        }
    }

    return true;
}

void applySparseAttribute(LibraryTrack &track, const QString &key, const QString &value)
{
    bool ok = false;
    if (key == "_performer") {
        track.performer = value;
    } else if (key == "_composer") {
        track.composer = value;
    } else if (key == "_original_year") {
        track.originalYear = value.toInt(&ok);
        if (!ok)
            track.originalYear = 0;
    } else if (key == "_sample_rate") {
        track.sampleRate = value.toInt(&ok);
        if (!ok)
            track.sampleRate = 0;
    } else if (key == "_bit_depth") {
        track.bitDepth = value.toInt(&ok);
        if (!ok)
            track.bitDepth = 0;
    } else if (key == "_channels") {
        track.channels = value.toInt(&ok);
        if (!ok)
            track.channels = 0;
    } else if (key == "_url") {
        track.url = value;
    } else if (key == "_created_time") {
        track.createdTime = value.toLongLong(&ok);
        if (!ok)
            track.createdTime = 0;
    } else if (key == "_comment") {
        track.comment = value;
    } else if (key == "_bpm") {
        track.bpm = value.toInt(&ok);
        if (!ok)
            track.bpm = 0;
    } else if (key == "_initial_key") {
        track.initialKey = value;
    } else if (key == "_codec") {
        track.codec = value;
    } else {
        const QString nKey = normalizedSparseKey(key);
        QStringList values = variantToStringList(track.customTags.value(nKey));
        if (!values.contains(value))
            values.append(value);
        track.customTags.insert(nKey, values);
    }
}

void hydrateSparseAttributes(QSqlDatabase db, QVector<LibraryTrack> &tracks)
{
    if (tracks.isEmpty())
        return;

    QHash<qint64, int> rowById;
    rowById.reserve(tracks.size());
    for (int i = 0; i < tracks.size(); ++i)
        rowById.insert(tracks[i].id, i);

    constexpr int chunkSize = 500;
    for (int offset = 0; offset < tracks.size(); offset += chunkSize) {
        const int end = std::min(offset + chunkSize, static_cast<int>(tracks.size()));
        QStringList placeholders;
        placeholders.reserve(end - offset);

        QSqlQuery query(db);
        for (int i = offset; i < end; ++i)
            placeholders.append(QString(":id%1").arg(i - offset));

        query.prepare(QString("SELECT track_id, key, value FROM track_attributes WHERE track_id IN (%1) ORDER BY track_id")
                          .arg(placeholders.join(", ")));

        for (int i = offset; i < end; ++i)
            query.bindValue(QString(":id%1").arg(i - offset), tracks[i].id);

        if (!query.exec())
            continue;

        while (query.next()) {
            const qint64 trackId = query.value(0).toLongLong();
            auto it = rowById.constFind(trackId);
            if (it == rowById.constEnd())
                continue;
            applySparseAttribute(tracks[it.value()], query.value(1).toString(), query.value(2).toString());
        }
    }
}

void hydrateSparseAttributes(QSqlDatabase db, LibraryTrack &track)
{
    if (track.id < 0)
        return;

    QSqlQuery query(db);
    query.prepare("SELECT key, value FROM track_attributes WHERE track_id = :track_id ORDER BY key, value");
    query.bindValue(":track_id", track.id);
    if (!query.exec())
        return;

    while (query.next())
        applySparseAttribute(track, query.value(0).toString(), query.value(1).toString());
}

} // namespace LibrarySparseAttributes
