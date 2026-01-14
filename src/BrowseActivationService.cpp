#include "BrowseActivationService.h"
#include "AppViewModel.h"
#include "PlaylistStore.h"
#include "ViewedPlaylistRouter.h"
#include "TrackListModel.h"
#include "MetadataExtractor.h"
#include "Settings.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryTreeModel.h"
#include "TrackFilter.h"

BrowseActivationService::BrowseActivationService(QObject *parent)
    : QObject(parent)
{
}

void BrowseActivationService::initialize(AppViewModel *app, PlaylistStore *store,
                                          ViewedPlaylistRouter *router, LibraryDatabase *libraryDb,
                                          LibraryTreeModel *libraryTreeModel)
{
    m_app = app;
    m_store = store;
    m_router = router;  // Can be null - will use ViewedPlaylistRouter::instance() as fallback
    m_libraryDb = libraryDb;
    m_libraryTreeModel = libraryTreeModel;
}

ViewedPlaylistRouter *BrowseActivationService::router() const
{
    return m_router ? m_router : ViewedPlaylistRouter::instance();
}

void BrowseActivationService::activatePlaylistRow(int row)
{
    if (!m_app || !router())
        return;
    
    // Set active playlist to viewed playlist, then play
    router()->setActiveToViewed();
    m_app->playIndex(row);
}

void BrowseActivationService::activateCollectionEntry(const QString &entryId)
{
    if (entryId.isEmpty())
        return;
    
    // Parse entry ID format:
    // "t:<filePath>" for track - add to playlist and play
    if (entryId.startsWith("t:")) {
        // Track entry - add to playlist and play
        QString filePath = entryId.mid(2);
        applyTracksToPlaylist({filePath}, 0);
    }
    // Note: "g:" group entries are now handled via openCollectionGroup() with filter-based state
}

void BrowseActivationService::addCollectionEntryToViewed(const QString &entryId)
{
    if (entryId.isEmpty() || !m_libraryDb)
        return;
    
    QStringList filePaths;
    
    if (entryId.startsWith("t:")) {
        filePaths.append(entryId.mid(2));
    } else if (entryId.startsWith("g:") && m_libraryTreeModel) {
        QString nodeKey = entryId.mid(2);
        // Get tracks for this node from library tree model
        QVariantList tracks = m_libraryTreeModel->tracksForNode(nodeKey);
        for (const QVariant &v : tracks) {
            QVariantMap map = v.toMap();
            QString path = map.value("filePath").toString();
            if (!path.isEmpty())
                filePaths.append(path);
        }
    }
    
    if (!filePaths.isEmpty()) {
        // Just append, don't autoplay
        TrackListModel *model = m_store ? m_store->displayedPlaylist() : nullptr;
        if (model) {
            for (const QString &path : filePaths) {
                auto libOpt = m_libraryDb->trackByPath(path);
                if (libOpt.has_value()) {
                    model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                } else {
                    TrackInfo track = MetadataExtractor::extractTrackInfo(path);
                    if (track.isValid())
                        model->addTrack(track);
                }
            }
        }
    }
}

void BrowseActivationService::playCollectionEntryNow(const QString &entryId)
{
    if (entryId.isEmpty() || !m_libraryDb || !m_store || !router() || !m_app)
        return;
    
    QStringList filePaths;
    
    if (entryId.startsWith("t:")) {
        filePaths.append(entryId.mid(2));
    } else if (entryId.startsWith("g:") && m_libraryTreeModel) {
        QString nodeKey = entryId.mid(2);
        QVariantList tracks = m_libraryTreeModel->tracksForNode(nodeKey);
        for (const QVariant &v : tracks) {
            QVariantMap map = v.toMap();
            QString path = map.value("filePath").toString();
            if (!path.isEmpty())
                filePaths.append(path);
        }
    }
    
    if (filePaths.isEmpty())
        return;
    
    // Add tracks and play immediately
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    
    for (const QString &path : filePaths) {
        auto libOpt = m_libraryDb->trackByPath(path);
        if (libOpt.has_value()) {
            model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
        } else {
            TrackInfo track = MetadataExtractor::extractTrackInfo(path);
            if (track.isValid())
                model->addTrack(track);
        }
    }
    
    // Play from the first added track
    router()->setActiveToViewed();
    m_app->playIndex(startRow);
}

void BrowseActivationService::addTracksToViewed(const QVariantList &tracks)
{
    if (!m_store || tracks.isEmpty())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    for (const QVariant &v : tracks) {
        QVariantMap map = v.toMap();
        QString filePath = map.value("filePath").toString();
        if (filePath.isEmpty())
            continue;
        
        // Try library database first
        if (m_libraryDb) {
            auto libOpt = m_libraryDb->trackByPath(filePath);
            if (libOpt.has_value()) {
                model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                continue;
            }
        }
        
        // Fallback to metadata extraction
        TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
        if (track.isValid())
            model->addTrack(track);
    }
}

void BrowseActivationService::playTracksNow(const QVariantList &tracks)
{
    if (!m_store || !router() || !m_app || tracks.isEmpty())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    
    for (const QVariant &v : tracks) {
        QVariantMap map = v.toMap();
        QString filePath = map.value("filePath").toString();
        if (filePath.isEmpty())
            continue;
        
        if (m_libraryDb) {
            auto libOpt = m_libraryDb->trackByPath(filePath);
            if (libOpt.has_value()) {
                model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                continue;
            }
        }
        
        TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
        if (track.isValid())
            model->addTrack(track);
    }
    
    // Play from the first added track
    router()->setActiveToViewed();
    m_app->playIndex(startRow);
}

void BrowseActivationService::dropUrlsToViewed(const QList<QUrl> &urls)
{
    if (!m_store)
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        
        QString filePath = url.toLocalFile();
        
        // Try library database first
        if (m_libraryDb) {
            auto libOpt = m_libraryDb->trackByPath(filePath);
            if (libOpt.has_value()) {
                model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                continue;
            }
        }
        
        // Fallback to metadata extraction
        TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
        if (track.isValid())
            model->addTrack(track);
    }
}

void BrowseActivationService::applyTracksToPlaylist(const QStringList &filePaths, int startRow)
{
    if (!m_store || !router() || !m_app)
        return;
    
    Settings *settings = Settings::instance();
    if (!settings)
        return;
    
    TrackListModel *model = nullptr;
    QString targetId = resolveTargetPlaylistId();
    
    if (targetId.isEmpty())
        return;
    
    // Set viewed playlist to target
    router()->setViewedPlaylistId(targetId);
    model = m_store->displayedPlaylist();
    
    if (!model)
        return;
    
    // Handle replace policy
    int policy = settings->browseTargetPolicy();
    if (policy == Settings::ReplaceGeneratedPreferViewed) {
        int idx = m_store->indexOfUuid(QUuid(targetId));
        if (idx >= 0 && !m_store->tabIsUserCreated(idx)) {
            model->clear();
        }
    }
    
    int insertRow = model->count();
    
    // Add tracks
    for (const QString &path : filePaths) {
        if (m_libraryDb) {
            auto libOpt = m_libraryDb->trackByPath(path);
            if (libOpt.has_value()) {
                model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
                continue;
            }
        }
        TrackInfo track = MetadataExtractor::extractTrackInfo(path);
        if (track.isValid())
            model->addTrack(track);
    }
    
    // Handle autoplay
    if (shouldAutoplay()) {
        router()->setActiveToViewed();
        m_app->playIndex(insertRow + startRow);
    }
}

QString BrowseActivationService::resolveTargetPlaylistId()
{
    if (!m_store || !router())
        return QString();
    
    Settings *settings = Settings::instance();
    if (!settings)
        return router()->viewedPlaylistId();
    
    int policy = settings->browseTargetPolicy();
    
    switch (policy) {
    case Settings::AppendToViewed:
        return router()->viewedPlaylistId();
        
    case Settings::ReplaceGeneratedPreferViewed: {
        // If viewed is generated, use it; otherwise find any generated
        QString viewedId = router()->viewedPlaylistId();
        int idx = m_store->indexOfUuid(QUuid(viewedId));
        if (idx >= 0 && !m_store->tabIsUserCreated(idx)) {
            return viewedId;
        }
        QString generatedId = m_store->findGeneratedPlaylistId();
        if (!generatedId.isEmpty()) {
            return generatedId;
        }
        // Create new generated playlist
        return m_store->createGeneratedTab();
    }
        
    case Settings::NewPlaylist:
        return m_store->createNewTab();
        
    default:
        return router()->viewedPlaylistId();
    }
}

bool BrowseActivationService::shouldAutoplay() const
{
    Settings *settings = Settings::instance();
    if (!settings || !m_app)
        return false;
    
    int policy = settings->browseAutoplayPolicy();
    
    switch (policy) {
    case Settings::NeverStart:
        return false;
    case Settings::StartIfStopped:
        return m_app->playbackState() == AppViewModel::Stopped;
    case Settings::AlwaysStart:
        return true;
    default:
        return false;
    }
}

void BrowseActivationService::openCollectionGroup(const QVariantMap &currentPanelState,
                                                   const QString &groupType,
                                                   const QVariant &groupValue)
{
    // Build new panel state by extending current filter with the group condition
    QVariantList currentFilter = currentPanelState.value("filter").toList();
    
    QVariantMap newCondition;
    newCondition["field"] = groupType;
    newCondition["op"] = "=";
    newCondition["value"] = groupValue;
    currentFilter.append(newCondition);
    
    // Look up per-group-type settings from Settings
    Settings *settings = Settings::instance();
    QString nextGroupBy = settings ? settings->groupTypeNextGroupBy(groupType) : "none";
    QString openAction = settings ? settings->groupTypeOpenAction(groupType) : "openPanel";
    
    // Build display title
    QString title = groupValue.toString();
    if (title.isEmpty()) {
        title = QString("Unknown %1").arg(groupType);
    }
    
    // Build new panel state
    QVariantMap newPanelState;
    newPanelState["panelContextType"] = groupType;
    newPanelState["filter"] = currentFilter;
    newPanelState["groupBy"] = nextGroupBy;
    newPanelState["title"] = title;
    
    // Handle different open actions
    if (openAction == "addToPlaylist") {
        addFilteredTracksToViewed(currentPanelState.value("filter").toList(), groupType, groupValue);
        return;
    } else if (openAction == "playNow") {
        playFilteredTracksNow(currentPanelState.value("filter").toList(), groupType, groupValue);
        return;
    } else if (openAction == "replacePanel") {
        emit replaceCollectionPanelRequested(newPanelState);
        return;
    }
    
    // Default: open in new panel
    emit openCollectionPanelRequested(newPanelState);
}

void BrowseActivationService::addFilteredTracksToViewed(const QVariantList &filter,
                                                         const QString &groupType,
                                                         const QVariant &groupValue)
{
    if (!m_libraryDb || !m_store)
        return;
    
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    
    // Add the group condition
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    trackFilter.append(cond);
    
    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    for (const LibraryTrack &track : tracks) {
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    }
}

void BrowseActivationService::playFilteredTracksNow(const QVariantList &filter,
                                                     const QString &groupType,
                                                     const QVariant &groupValue)
{
    if (!m_libraryDb || !m_store || !router() || !m_app)
        return;
    
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    trackFilter.append(cond);
    
    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model || tracks.isEmpty())
        return;
    
    int startRow = model->count();
    
    for (const LibraryTrack &track : tracks) {
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    }
    
    router()->setActiveToViewed();
    m_app->playIndex(startRow);
}
