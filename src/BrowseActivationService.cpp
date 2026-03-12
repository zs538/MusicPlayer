#include "BrowseActivationService.h"
#include "AppViewModel.h"
#include "PlaylistStore.h"
#include "ViewedPlaylistRouter.h"
#include "TrackListModel.h"
#include "MetadataExtractor.h"
#include "Settings.h"
#include "library/LibraryDatabase.h"
#include "TrackFilter.h"
#include <QFileInfo>

BrowseActivationService::BrowseActivationService(QObject *parent)
    : QObject(parent)
{
}

void BrowseActivationService::initialize(AppViewModel *app, PlaylistStore *store,
                                          ViewedPlaylistRouter *router, LibraryDatabase *libraryDb)
{
    m_app = app;
    m_store = store;
    m_router = router;  // Can be null - will use ViewedPlaylistRouter::instance() as fallback
    m_libraryDb = libraryDb;
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
}

void BrowseActivationService::dropUrlsToViewed(const QList<QUrl> &urls)
{
    if (!m_store || !router() || urls.isEmpty())
        return;
    
    // Always append to the currently viewed playlist (no setting override)
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    
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
    
    // Apply autoplay policy
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
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

QString BrowseActivationService::resolveTargetPlaylistId(const QString &generatedName)
{
    if (!m_store || !router())
        return QString();
    
    Settings *settings = Settings::instance();
    if (!settings)
        return router()->viewedPlaylistId();
    
    int action = settings->openingTracksAction();
    
    if (action == Settings::OpeningAppendToViewed) {
        return router()->viewedPlaylistId();
    } else {
        // OpeningCreateNewPlaylist - create a new generated playlist
        // This respects the max count and drops oldest when needed
        return m_store->getOrCreateGeneratedPlaylist(generatedName);
    }
}

bool BrowseActivationService::shouldAutoplay() const
{
    Settings *settings = Settings::instance();
    if (!settings || !m_app)
        return false;
    
    int policy = settings->addTracksPolicy();
    
    switch (policy) {
    case Settings::AddNeverStart:
        return false;
    case Settings::AddStartIfStopped:
        return m_app->playbackState() == AppViewModel::Stopped;
    case Settings::AddAlwaysStart:
        return true;
    default:
        return false;
    }
}

void BrowseActivationService::addFilteredTracksToViewed(const QVariantList &filter,
                                                         const QString &groupType,
                                                         const QVariant &groupValue)
{
    if (!m_libraryDb || !m_store || !router())
        return;
    
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    
    // Add the group condition
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    trackFilter.append(cond);
    
    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    if (tracks.isEmpty())
        return;
    
    // Build playlist name based on group type
    QString playlistName = groupValue.toString();
    if (groupType == "album" && !tracks.isEmpty()) {
        // For albums, use "Artist - Album" format
        QString artist = tracks.first().albumArtist;
        if (artist.isEmpty())
            artist = tracks.first().artist;
        if (!artist.isEmpty())
            playlistName = artist + " - " + groupValue.toString();
    }
    
    // Check if OpeningCreateNewPlaylist and a playlist with this name already exists
    Settings *settings = Settings::instance();
    if (settings && settings->openingTracksAction() == Settings::OpeningCreateNewPlaylist) {
        QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
        if (!existingId.isEmpty()) {
            // Playlist already exists - just redirect to it and play from start
            router()->setViewedPlaylistId(existingId);
            if (shouldAutoplay() && m_app) {
                router()->setActiveToViewed();
                m_app->playIndex(0);
            }
            return;
        }
    }
    
    // Resolve target playlist based on OpeningTracksAction setting
    QString targetId = resolveTargetPlaylistId(playlistName);
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    const bool suppressDirtyTracking = startRow == 0;
    if (suppressDirtyTracking)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, true);

    for (const LibraryTrack &track : tracks) {
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    }

    if (suppressDirtyTracking)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, false);

    // Apply autoplay policy
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
    }
}

void BrowseActivationService::addFilteredTracksToViewedStartingAt(const QVariantList &filter,
                                                                   const QString &groupType,
                                                                   const QVariant &groupValue,
                                                                   const QString &startFilePath)
{
    if (!m_libraryDb || !m_store || !router())
        return;

    TrackFilter trackFilter = trackFilterFromVariant(filter);
    if (!groupType.isEmpty()) {
        FilterCondition cond;
        cond.field = groupType;
        cond.op = "=";
        cond.value = groupValue;
        trackFilter.append(cond);
    }

    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    if (tracks.isEmpty())
        return;

    QString playlistName = groupValue.toString();
    if (playlistName.isEmpty())
        playlistName = "Tracks";
        
    if (groupType == "album" && !tracks.isEmpty()) {
        QString artist = tracks.first().albumArtist;
        if (artist.isEmpty()) artist = tracks.first().artist;
        if (!artist.isEmpty()) playlistName = artist + " - " + groupValue.toString();
    }

    Settings *settings = Settings::instance();
    if (settings && settings->openingTracksAction() == Settings::OpeningCreateNewPlaylist) {
        QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
        if (!existingId.isEmpty()) {
            router()->setViewedPlaylistId(existingId);
            // Find the track index within the existing playlist and play from there
            if (shouldAutoplay() && m_app) {
                router()->setActiveToViewed();
                m_app->playIndex(0);
            }
            return;
        }
    }

    QString targetId = resolveTargetPlaylistId(playlistName);
    if (targetId.isEmpty())
        return;

    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;

    int startRow = model->count();
    int playRow = startRow;

    for (int i = 0; i < tracks.size(); ++i) {
        model->addTrack(MetadataExtractor::toTrackInfo(tracks[i]));
        if (tracks[i].filePath == startFilePath)
            playRow = startRow + i;
    }

    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(playRow);
    }
}

// Context menu: Append to viewed playlist (no autoplay, no target resolution)
void BrowseActivationService::appendFilteredTracksToViewed(const QVariantList &filter,
                                                            const QString &groupType,
                                                            const QVariant &groupValue)
{
    if (!m_libraryDb || !m_store || !router())
        return;
    
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    trackFilter.append(cond);
    
    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    if (tracks.isEmpty())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    for (const LibraryTrack &track : tracks) {
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    }
}

void BrowseActivationService::appendFilePathsToViewed(const QStringList &filePaths)
{
    if (!m_store || filePaths.isEmpty())
        return;

    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;

    for (const QString &filePath : filePaths) {
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
}

void BrowseActivationService::appendCollectionEntryToViewed(const QString &entryId)
{
    if (!m_store || !router())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    QString filePath = entryId.startsWith("t:") ? entryId.mid(2) : entryId;
    
    if (m_libraryDb) {
        auto libOpt = m_libraryDb->trackByPath(filePath);
        if (libOpt.has_value()) {
            model->addTrack(MetadataExtractor::toTrackInfo(*libOpt));
            return;
        }
    }
    
    TrackInfo track = MetadataExtractor::extractTrackInfo(filePath);
    if (track.isValid())
        model->addTrack(track);
}

// Context menu: Append after currently playing track
void BrowseActivationService::appendFilteredTracksAfterPlaying(const QVariantList &filter,
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
    if (tracks.isEmpty())
        return;
    
    // Get the active playlist (where playback is happening)
    TrackListModel *model = m_store->activePlaylist();
    if (!model)
        model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    // Insert after current playing index
    int insertPos = m_app->currentIndex() + 1;
    if (insertPos < 0 || insertPos > model->count())
        insertPos = model->count();
    
    for (int i = 0; i < tracks.size(); ++i) {
        model->insertTrack(insertPos + i, MetadataExtractor::toTrackInfo(tracks[i]));
    }
}

void BrowseActivationService::appendCollectionEntryAfterPlaying(const QString &entryId)
{
    if (!m_store || !router() || !m_app)
        return;
    
    TrackListModel *model = m_store->activePlaylist();
    if (!model)
        model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    QString filePath = entryId.startsWith("t:") ? entryId.mid(2) : entryId;
    
    int insertPos = m_app->currentIndex() + 1;
    if (insertPos < 0 || insertPos > model->count())
        insertPos = model->count();
    
    TrackInfo track;
    if (m_libraryDb) {
        auto libOpt = m_libraryDb->trackByPath(filePath);
        if (libOpt.has_value()) {
            track = MetadataExtractor::toTrackInfo(*libOpt);
        }
    }
    if (!track.isValid()) {
        track = MetadataExtractor::extractTrackInfo(filePath);
    }
    
    if (track.isValid())
        model->insertTrack(insertPos, track);
}

// Context menu: Open in new playlist (follows autoplay setting)
void BrowseActivationService::openFilteredTracksInNewPlaylist(const QVariantList &filter,
                                                               const QString &groupType,
                                                               const QVariant &groupValue)
{
    if (!m_libraryDb || !m_store || !router())
        return;
    
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    FilterCondition cond;
    cond.field = groupType;
    cond.op = "=";
    cond.value = groupValue;
    trackFilter.append(cond);
    
    QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(trackFilter);
    if (tracks.isEmpty())
        return;
    
    // Build playlist name based on group type
    QString playlistName = groupValue.toString();
    if (groupType == "album" && !tracks.isEmpty()) {
        QString artist = tracks.first().albumArtist;
        if (artist.isEmpty())
            artist = tracks.first().artist;
        if (!artist.isEmpty())
            playlistName = artist + " - " + groupValue.toString();
    }
    
    // Check if a generated playlist with this name already exists
    QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
    if (!existingId.isEmpty()) {
        // Playlist already exists - switch to it and play from start
        router()->setViewedPlaylistId(existingId);
        if (shouldAutoplay() && m_app) {
            router()->setActiveToViewed();
            m_app->playIndex(0);
        }
        return;
    }
    
    // Create a new generated playlist
    QString targetId = m_store->getOrCreateGeneratedPlaylist(playlistName);
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    const bool suppressDirtyTracking = startRow == 0;
    if (suppressDirtyTracking)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, true);

    for (const LibraryTrack &track : tracks) {
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    }

    if (suppressDirtyTracking)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, false);
    
    // Apply autoplay policy
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
    }
}

void BrowseActivationService::playFilteredTracksInNewPlaylist(const QVariantList &filter,
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
    if (tracks.isEmpty())
        return;

    QString playlistName = groupValue.toString();
    if (groupType == "album" && !tracks.isEmpty()) {
        QString artist = tracks.first().albumArtist;
        if (artist.isEmpty())
            artist = tracks.first().artist;
        if (!artist.isEmpty())
            playlistName = artist + " - " + groupValue.toString();
    }

    const QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
    if (!existingId.isEmpty()) {
        router()->setViewedPlaylistId(existingId);
        router()->setActiveToViewed();
        m_app->playIndex(0);
        return;
    }

    const QString targetId = m_store->createNewTab(playlistName, false);
    if (targetId.isEmpty())
        return;

    m_store->enforceGeneratedPlaylistCount();
    router()->setViewedPlaylistId(targetId);

    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;

    m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, true);
    for (const LibraryTrack &track : tracks)
        model->addTrack(MetadataExtractor::toTrackInfo(track));
    m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, false);

    router()->setActiveToViewed();
    m_app->playIndex(0);
}

void BrowseActivationService::openCollectionEntryInNewPlaylist(const QString &entryId)
{
    if (!m_store || !router())
        return;
    
    QString filePath = entryId.startsWith("t:") ? entryId.mid(2) : entryId;
    
    TrackInfo track;
    if (m_libraryDb) {
        auto libOpt = m_libraryDb->trackByPath(filePath);
        if (libOpt.has_value()) {
            track = MetadataExtractor::toTrackInfo(*libOpt);
        }
    }
    if (!track.isValid()) {
        track = MetadataExtractor::extractTrackInfo(filePath);
    }
    if (!track.isValid())
        return;
    
    // Use track title or filename as playlist name
    QString playlistName = track.title;
    if (playlistName.isEmpty())
        playlistName = QFileInfo(filePath).baseName();
    
    const bool hadExistingPlaylist = !m_store->findGeneratedPlaylistByName(playlistName).isEmpty();
    QString targetId = m_store->getOrCreateGeneratedPlaylist(playlistName);
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    if (!hadExistingPlaylist && startRow == 0)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, true);
    model->addTrack(track);
    if (!hadExistingPlaylist && startRow == 0)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, false);
    
    // Apply autoplay policy
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
    }
}
