#ifndef BROWSEACTIVATIONSERVICE_H
#define BROWSEACTIVATIONSERVICE_H

#include <QObject>
#include <QUrl>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

class AppViewModel;
class PlaylistStore;
class ViewedPlaylistRouter;
class LibraryDatabase;
class LibraryTreeModel;

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
                    ViewedPlaylistRouter *router, LibraryDatabase *libraryDb,
                    LibraryTreeModel *libraryTreeModel);

    // Playlist entry activation (double-click on playlist row)
    Q_INVOKABLE void activatePlaylistRow(int row);

    // Library/collection activation (entryId format: "t:filePath" for tracks, "g:nodeKey" for groups)
    Q_INVOKABLE void activateCollectionEntry(const QString &entryId);

    // Explicit context menu commands
    Q_INVOKABLE void addCollectionEntryToViewed(const QString &entryId);
    Q_INVOKABLE void playCollectionEntryNow(const QString &entryId);
    
    // Add tracks directly (for nested groups that store tracks inline)
    Q_INVOKABLE void addTracksToViewed(const QVariantList &tracks);
    Q_INVOKABLE void playTracksNow(const QVariantList &tracks);

    // Drop handling
    Q_INVOKABLE void dropUrlsToViewed(const QList<QUrl> &urls);

    // New collection browsing API (filter-based)
    Q_INVOKABLE void openCollectionGroup(const QVariantMap &currentPanelState,
                                          const QString &groupType,
                                          const QVariant &groupValue);
    Q_INVOKABLE void addFilteredTracksToViewed(const QVariantList &filter,
                                                const QString &groupType,
                                                const QVariant &groupValue);
    Q_INVOKABLE void playFilteredTracksNow(const QVariantList &filter,
                                            const QString &groupType,
                                            const QVariant &groupValue);

signals:
    void openCollectionPanelRequested(const QVariantMap &panelState);
    void replaceCollectionPanelRequested(const QVariantMap &panelState);

private:
    void applyTracksToPlaylist(const QStringList &filePaths, int startRow = 0);
    QString resolveTargetPlaylistId();
    bool shouldAutoplay() const;
    ViewedPlaylistRouter *router() const;
    
    AppViewModel *m_app = nullptr;
    PlaylistStore *m_store = nullptr;
    ViewedPlaylistRouter *m_router = nullptr;
    LibraryDatabase *m_libraryDb = nullptr;
    LibraryTreeModel *m_libraryTreeModel = nullptr;
};

#endif // BROWSEACTIVATIONSERVICE_H
