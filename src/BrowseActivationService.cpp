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

// --- Consolidated helpers ---

TrackFilter BrowseActivationService::buildGroupFilter(const QVariantList &filter,
                                                       const QString &groupType,
                                                       const QVariant &groupValue) const
{
    TrackFilter trackFilter = trackFilterFromVariant(filter);
    if (!groupType.isEmpty()) {
        FilterCondition cond;
        cond.field = groupType;
        cond.op = QStringLiteral("=");
        cond.value = groupValue;
        trackFilter.append(cond);
    }
    return trackFilter;
}

TrackInfo BrowseActivationService::resolveTrack(const QString &filePath) const
{
    if (m_libraryDb) {
        auto libOpt = m_libraryDb->trackByPath(filePath);
        if (libOpt.has_value())
            return MetadataExtractor::toTrackInfo(*libOpt);
    }
    return MetadataExtractor::extractTrackInfo(filePath);
}

QString BrowseActivationService::generatePlaylistName(const QString &groupType,
                                                       const QVariant &groupValue,
                                                       const QVector<LibraryTrack> &tracks) const
{
    QString name = groupValue.toString();
    if (name.isEmpty())
        name = QStringLiteral("Tracks");
    if (groupType == QStringLiteral("album") && !tracks.isEmpty()) {
        QString artist = tracks.first().albumArtist;
        if (artist.isEmpty())
            artist = tracks.first().artist;
        if (!artist.isEmpty())
            name = artist + QStringLiteral(" - ") + groupValue.toString();
    }
    return name;
}

void BrowseActivationService::populateModel(TrackListModel *model,
                                             const QVector<LibraryTrack> &tracks,
                                             const QString &playlistId)
{
    const bool suppress = !playlistId.isEmpty() && model->count() == 0;
    if (suppress)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(playlistId, true);

    for (const LibraryTrack &track : tracks)
        model->addTrack(MetadataExtractor::toTrackInfo(track));

    if (suppress)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(playlistId, false);
}

QString BrowseActivationService::entryIdToFilePath(const QString &entryId)
{
    return entryId.startsWith(QStringLiteral("t:")) ? entryId.mid(2) : entryId;
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
    
    if (entryId.startsWith(QStringLiteral("t:")))
        applyTracksToPlaylist({entryIdToFilePath(entryId)}, 0);
}

void BrowseActivationService::dropUrlsToViewed(const QList<QUrl> &urls)
{
    if (!m_store || !router() || urls.isEmpty())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        TrackInfo track = resolveTrack(url.toLocalFile());
        if (track.isValid())
            model->addTrack(track);
    }
    
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
    }
}

void BrowseActivationService::applyTracksToPlaylist(const QStringList &filePaths, int startRow)
{
    if (!m_store || !router() || !m_app)
        return;
    
    QString targetId = resolveTargetPlaylistId();
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int insertRow = model->count();
    
    for (const QString &path : filePaths) {
        TrackInfo track = resolveTrack(path);
        if (track.isValid())
            model->addTrack(track);
    }
    
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
    
    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;
    
    const QString playlistName = generatePlaylistName(groupType, groupValue, tracks);
    
    Settings *settings = Settings::instance();
    if (settings && settings->openingTracksAction() == Settings::OpeningCreateNewPlaylist) {
        QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
        if (!existingId.isEmpty()) {
            router()->setViewedPlaylistId(existingId);
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
    populateModel(model, tracks, targetId);

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

    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;

    const QString playlistName = generatePlaylistName(groupType, groupValue, tracks);

    Settings *settings = Settings::instance();
    if (settings && settings->openingTracksAction() == Settings::OpeningCreateNewPlaylist) {
        QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
        if (!existingId.isEmpty()) {
            router()->setViewedPlaylistId(existingId);
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
    
    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;
    
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    for (const LibraryTrack &track : tracks)
        model->addTrack(MetadataExtractor::toTrackInfo(track));
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
        TrackInfo track = resolveTrack(filePath);
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
    
    TrackInfo track = resolveTrack(entryIdToFilePath(entryId));
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
    
    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;
    
    TrackListModel *model = m_store->activePlaylist();
    if (!model)
        model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int insertPos = m_app->currentIndex() + 1;
    if (insertPos < 0 || insertPos > model->count())
        insertPos = model->count();
    
    for (int i = 0; i < tracks.size(); ++i)
        model->insertTrack(insertPos + i, MetadataExtractor::toTrackInfo(tracks[i]));
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
    
    int insertPos = m_app->currentIndex() + 1;
    if (insertPos < 0 || insertPos > model->count())
        insertPos = model->count();
    
    TrackInfo track = resolveTrack(entryIdToFilePath(entryId));
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
    
    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;
    
    const QString playlistName = generatePlaylistName(groupType, groupValue, tracks);
    
    QString existingId = m_store->findGeneratedPlaylistByName(playlistName);
    if (!existingId.isEmpty()) {
        router()->setViewedPlaylistId(existingId);
        if (shouldAutoplay() && m_app) {
            router()->setActiveToViewed();
            m_app->playIndex(0);
        }
        return;
    }
    
    QString targetId = m_store->getOrCreateGeneratedPlaylist(playlistName);
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    populateModel(model, tracks, targetId);
    
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

    const QVector<LibraryTrack> tracks = m_libraryDb->tracksMatchingFilter(buildGroupFilter(filter, groupType, groupValue));
    if (tracks.isEmpty())
        return;

    const QString playlistName = generatePlaylistName(groupType, groupValue, tracks);

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

    populateModel(model, tracks, targetId);

    router()->setActiveToViewed();
    m_app->playIndex(0);
}

void BrowseActivationService::openCollectionEntryInNewPlaylist(const QString &entryId)
{
    if (!m_store || !router())
        return;
    
    const QString filePath = entryIdToFilePath(entryId);
    TrackInfo track = resolveTrack(filePath);
    if (!track.isValid())
        return;
    
    QString playlistName = track.title;
    if (playlistName.isEmpty())
        playlistName = QFileInfo(filePath).baseName();
    
    const bool hadExisting = !m_store->findGeneratedPlaylistByName(playlistName).isEmpty();
    QString targetId = m_store->getOrCreateGeneratedPlaylist(playlistName);
    if (targetId.isEmpty())
        return;
    
    router()->setViewedPlaylistId(targetId);
    TrackListModel *model = m_store->displayedPlaylist();
    if (!model)
        return;
    
    int startRow = model->count();
    const bool suppress = !hadExisting && startRow == 0;
    if (suppress)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, true);
    model->addTrack(track);
    if (suppress)
        m_store->setGeneratedPlaylistDirtyTrackingSuppressed(targetId, false);
    
    if (shouldAutoplay() && m_app) {
        router()->setActiveToViewed();
        m_app->playIndex(startRow);
    }
}
