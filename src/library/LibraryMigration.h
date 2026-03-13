#ifndef LIBRARYMIGRATION_H
#define LIBRARYMIGRATION_H

class QSqlDatabase;
class QSqlQuery;

namespace LibraryMigration {

constexpr int TrackSchemaVersion = 2;

// Ensure the tracks/track_attributes schema is up to date, migrating if needed.
bool ensureTrackSchema(QSqlDatabase &db, QSqlQuery &query);

} // namespace LibraryMigration

#endif // LIBRARYMIGRATION_H
