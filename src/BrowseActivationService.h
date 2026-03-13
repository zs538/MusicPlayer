#ifndef BROWSEACTIVATIONSERVICE_H
#define BROWSEACTIVATIONSERVICE_H

#include <QObject>
#include <QUrl>
#include <QList>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class AppViewModel;
class PlaylistStore;
class ViewedPlaylistRouter;
struct LibraryTrack;

#include "library/LibraryDatabase.h"
#include "TrackFilter.h"

class TrackListModel;
struct TrackInfo;

/**
 * @brief BrowseActivationService implements the global "double click play/queue" policy.
 * 
 * This is the single authority for browse activation decisions. QML calls methods here
 * instead of implementing activation logic itself.
 */
class BrowseActivationService : public QObject
{
    Q_OBJECT

public:
    explicit BrowseActivationService(QObject *parent = nullptr);

    void initialize(AppViewModel *app, PlaylistStore *store, 
                    ViewedPlaylistRouter *router, LibraryDatabase *libraryDb);

    // Playlist entry activation (double-click on playlist row)
    Q_INVOKABLE void activatePlaylistRow(int row);

    // Library/collection activation (entryId format: "t:filePath" for tracks)
    Q_INVOKABLE void activateCollectionEntry(const QString &entryId);

    // Drop handling
    Q_INVOKABLE void dropUrlsToViewed(const QList<QUrl> &urls);

    // Double-click queue action (uses settings for target playlist and autoplay)
    Q_INVOKABLE void addFilteredTracksToViewed(const QVariantList &filter,
                                                const QString &groupType,
                                                const QVariant &groupValue);

    // Double-click on expanded track: add whole group, start playing from specific track
    Q_INVOKABLE void addFilteredTracksToViewedStartingAt(const QVariantList &filter,
                                                          const QString &groupType,
                                                          const QVariant &groupValue,
                                                          const QString &startFilePath);
    
    // Context menu: append to viewed playlist (no autoplay)
    Q_INVOKABLE void appendFilteredTracksToViewed(const QVariantList &filter,
                                                   const QString &groupType,
                                                   const QVariant &groupValue);
    Q_INVOKABLE void appendFilePathsToViewed(const QStringList &filePaths);
    Q_INVOKABLE void appendCollectionEntryToViewed(const QString &entryId);
    
    // Context menu: append after currently playing track
    Q_INVOKABLE void appendFilteredTracksAfterPlaying(const QVariantList &filter,
                                                       const QString &groupType,
                                                       const QVariant &groupValue);
    Q_INVOKABLE void appendCollectionEntryAfterPlaying(const QString &entryId);
    
    // Context menu: open in new generated playlist (follows autoplay setting)
    Q_INVOKABLE void openFilteredTracksInNewPlaylist(const QVariantList &filter,
                                                      const QString &groupType,
                                                      const QVariant &groupValue);
    Q_INVOKABLE void playFilteredTracksInNewPlaylist(const QVariantList &filter,
                                                      const QString &groupType,
                                                      const QVariant &groupValue);
    Q_INVOKABLE void openCollectionEntryInNewPlaylist(const QString &entryId);

private:
    // Consolidated helpers
    TrackFilter buildGroupFilter(const QVariantList &filter, const QString &groupType, const QVariant &groupValue) const;
    TrackInfo resolveTrack(const QString &filePath) const;
    QString generatePlaylistName(const QString &groupType, const QVariant &groupValue,
                                 const QVector<LibraryTrack> &tracks) const;
    void populateModel(TrackListModel *model, const QVector<LibraryTrack> &tracks,
                       const QString &playlistId = QString());
    static QString entryIdToFilePath(const QString &entryId);

    void applyTracksToPlaylist(const QStringList &filePaths, int startRow = 0);
    QString resolveTargetPlaylistId(const QString &generatedName = QString());
    bool shouldAutoplay() const;
    ViewedPlaylistRouter *router() const;
    
    AppViewModel *m_app = nullptr;
    PlaylistStore *m_store = nullptr;
    ViewedPlaylistRouter *m_router = nullptr;
    LibraryDatabase *m_libraryDb = nullptr;
};

#endif // BROWSEACTIVATIONSERVICE_H
