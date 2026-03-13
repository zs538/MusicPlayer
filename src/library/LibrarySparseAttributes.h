#ifndef LIBRARYSPARSEATTRIBUTES_H
#define LIBRARYSPARSEATTRIBUTES_H

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

class QSqlDatabase;
class QSqlQuery;
struct LibraryTrack;

namespace LibrarySparseAttributes {

QString normalizedSparseKey(const QString &key);
QStringList variantToStringList(const QVariant &value);

void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key, const QString &value);
void appendSparseAttribute(QVector<QPair<QString, QString>> &attributes,
                           const QString &key, qint64 value);

QVector<QPair<QString, QString>> sparseAttributesForTrack(const LibraryTrack &track);

bool replaceTrackAttributes(QSqlDatabase &db, qint64 trackId,
                            const QVector<QPair<QString, QString>> &attributes,
                            const char *context);

void applySparseAttribute(LibraryTrack &track, const QString &key, const QString &value);
void hydrateSparseAttributes(QSqlDatabase db, QVector<LibraryTrack> &tracks);
void hydrateSparseAttributes(QSqlDatabase db, LibraryTrack &track);

} // namespace LibrarySparseAttributes

#endif // LIBRARYSPARSEATTRIBUTES_H
