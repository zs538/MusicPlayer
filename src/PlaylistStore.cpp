#include "PlaylistStore.h"
#include "TrackListModel.h"
#include "MetadataExtractor.h"
#include "Settings.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QtConcurrent>

PlaylistStore::PlaylistStore(QObject *parent)
    : QObject(parent)
{
    createNewTab(QStringLiteral("Playlist 1"));
    
    // Connect to settings to enforce generated playlist count when it changes
    Settings *settings = Settings::instance();
    if (settings) {
        connect(settings, &Settings::generatedPlaylistCountChanged, this, &PlaylistStore::enforceGeneratedPlaylistCount);
    }
}

void PlaylistStore::enforceGeneratedPlaylistCount()
{
    Settings *settings = Settings::instance();
    if (!settings)
        return;
    
    int maxCount = settings->generatedPlaylistCount();
    
    // Count generated playlists and collect their indices (oldest first)
    QVector<int> generatedIndices;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!m_tabs[i].isUserCreated)
            generatedIndices.append(i);
    }
    
    // Remove oldest generated playlists until we're at or below the limit
    while (generatedIndices.size() > maxCount) {
        int oldestIdx = generatedIndices.takeFirst();
        QString oldestId = m_tabs[oldestIdx].uuid.toString();
        closeTab(oldestId);
        
        // Recalculate indices after removal (they shift down)
        generatedIndices.clear();
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (!m_tabs[i].isUserCreated)
                generatedIndices.append(i);
        }
    }
}

PlaylistStore::~PlaylistStore()
{
    for (auto &tab : m_tabs) {
        delete tab.model;
    }
}

QString PlaylistStore::createNewTab(const QString &name, bool isUserCreated)
{
    QString tabName = name.isEmpty() ? QStringLiteral("Playlist %1").arg(m_tabs.size() + 1) : name;
    
    Tab tab;
    tab.uuid = QUuid::createUuid();
    tab.name = tabName;
    tab.model = new TrackListModel(this);
    tab.isUserCreated = isUserCreated;
    
    // Connect model mutation signals to emit playlistChanged() for auto-save
    // and tabDataChanged() for track count updates in PlaylistTabsModel.
    // Capture UUID (not index) since tabs can be reordered.
    QUuid tabUuid = tab.uuid;
    TrackListModel *model = tab.model;
    
    auto emitChanges = [this, tabUuid]() {
        emit playlistChanged();
        int idx = indexOfUuid(tabUuid);
        if (idx >= 0)
            emit tabDataChanged(idx);
    };
    auto emitStructuralChanges = [this, tabUuid, emitChanges]() {
        markGeneratedPlaylistDirty(tabUuid);
        emitChanges();
    };
    
    connect(model, &TrackListModel::rowsInserted, this, emitStructuralChanges);
    connect(model, &TrackListModel::rowsRemoved, this, emitStructuralChanges);
    connect(model, &TrackListModel::rowsMoved, this, emitStructuralChanges);
    connect(model, &TrackListModel::modelReset, this, emitStructuralChanges);
    connect(model, &TrackListModel::dataChanged, this, emitChanges);
    
    int idx = m_tabs.size();
    m_tabs.append(tab);
    emit tabInserted(idx);
    
    if (m_tabs.size() == 1) {
        m_activeId = tab.uuid;
        m_displayedId = tab.uuid;
        emit activePlaylistChanged(m_activeId);
        emit displayedPlaylistChanged(m_displayedId);
    }
    
    emit tabCountChanged();
    return tab.uuid.toString();
}

bool PlaylistStore::closeTab(const QString &uuid)
{
    if (m_tabs.size() <= 1)
        return false;
    
    int idx = indexOfUuid(QUuid(uuid));
    if (idx < 0)
        return false;
    
    Tab tab = m_tabs[idx];
    
    m_tabs.removeAt(idx);
    emit tabRemoved(idx);
    
    // Don't transfer active status - let it become invalid (playing indicator will disappear)
    if (m_activeId == tab.uuid) {
        m_activeId = QUuid();
        emit activePlaylistChanged(m_activeId);
    }
    if (m_displayedId == tab.uuid && !m_tabs.isEmpty()) {
        m_displayedId = m_tabs[qBound(0, idx - 1, m_tabs.size() - 1)].uuid;
        emit displayedPlaylistChanged(m_displayedId);
    }
    
    delete tab.model;
    emit tabCountChanged();
    return true;
}

bool PlaylistStore::renameTab(const QString &uuid, const QString &newName)
{
    int idx = indexOfUuid(QUuid(uuid));
    if (idx < 0)
        return false;
    
    m_tabs[idx].name = newName;
    emit tabDataChanged(idx);
    return true;
}

bool PlaylistStore::moveTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size() ||
        toIndex < 0 || toIndex >= m_tabs.size() || fromIndex == toIndex)
        return false;
    
    m_tabs.move(fromIndex, toIndex);
    emit tabMoved(fromIndex, toIndex);
    return true;
}

void PlaylistStore::setDisplayedPlaylistIdString(const QString &uuid)
{
    setDisplayedPlaylist(uuid);
}

int PlaylistStore::displayedIndex() const
{
    return indexOfUuid(m_displayedId);
}

void PlaylistStore::setActivePlaylist(const QString &uuid)
{
    QUuid id(uuid);
    if (m_activeId != id && indexOfUuid(id) >= 0) {
        int oldIdx = indexOfUuid(m_activeId);
        m_activeId = id;
        emit activePlaylistChanged(m_activeId);
        // Notify model of isActive role change
        if (oldIdx >= 0) emit tabDataChanged(oldIdx);
        emit tabDataChanged(indexOfUuid(id));
    }
}

void PlaylistStore::setDisplayedPlaylist(const QString &uuid)
{
    QUuid id(uuid);
    if (m_displayedId != id && indexOfUuid(id) >= 0) {
        int oldIdx = indexOfUuid(m_displayedId);
        m_displayedId = id;
        emit displayedPlaylistChanged(m_displayedId);
        // Notify model of isDisplayed role change
        if (oldIdx >= 0) emit tabDataChanged(oldIdx);
        emit tabDataChanged(indexOfUuid(id));
    }
}

TrackListModel *PlaylistStore::activePlaylist() const
{
    int idx = indexOfUuid(m_activeId);
    return idx >= 0 ? m_tabs[idx].model : nullptr;
}

TrackListModel *PlaylistStore::displayedPlaylist() const
{
    int idx = indexOfUuid(m_displayedId);
    return idx >= 0 ? m_tabs[idx].model : nullptr;
}

TrackListModel *PlaylistStore::getPlaylistModel(const QString &uuid) const
{
    int idx = indexOfUuid(QUuid(uuid));
    return idx >= 0 ? m_tabs[idx].model : nullptr;
}

TrackListModel *PlaylistStore::playlistModel(int index) const
{
    if (index >= 0 && index < m_tabs.size())
        return m_tabs[index].model;
    return nullptr;
}

QUuid PlaylistStore::tabUuid(int index) const
{
    if (index >= 0 && index < m_tabs.size())
        return m_tabs[index].uuid;
    return QUuid();
}

QString PlaylistStore::tabName(int index) const
{
    if (index >= 0 && index < m_tabs.size())
        return m_tabs[index].name;
    return QString();
}

bool PlaylistStore::tabIsUserCreated(int index) const
{
    if (index >= 0 && index < m_tabs.size())
        return m_tabs[index].isUserCreated;
    return true;
}

int PlaylistStore::generatedPlaylistCount() const
{
    int count = 0;
    for (const Tab &tab : m_tabs) {
        if (!tab.isUserCreated)
            ++count;
    }
    return count;
}

QString PlaylistStore::findGeneratedPlaylistByName(const QString &name) const
{
    QString tabName = name.isEmpty() ? QStringLiteral("Generated") : name;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!m_tabs[i].isUserCreated && m_tabs[i].name == tabName) {
            return m_tabs[i].uuid.toString();
        }
    }
    return QString();
}

bool PlaylistStore::setPlaylistUserCreated(const QString &uuid, bool isUserCreated)
{
    int idx = indexOfUuid(QUuid(uuid));
    if (idx < 0)
        return false;
    
    if (m_tabs[idx].isUserCreated != isUserCreated) {
        m_tabs[idx].isUserCreated = isUserCreated;
        emit tabDataChanged(idx);
    }
    return true;
}

void PlaylistStore::setGeneratedPlaylistDirtyTrackingSuppressed(const QString &uuid, bool suppressed)
{
    const QUuid id(uuid);
    if (id.isNull())
        return;

    if (suppressed)
        m_dirtyTrackingSuppressed.insert(id);
    else
        m_dirtyTrackingSuppressed.remove(id);
}

QString PlaylistStore::getOrCreateGeneratedPlaylist(const QString &name)
{
    QString tabName = name.isEmpty() ? QStringLiteral("Generated") : name;
    
    // Check if a generated playlist with this name already exists
    QString existingId = findGeneratedPlaylistByName(tabName);
    if (!existingId.isEmpty())
        return existingId;
    
    // Get max count from Settings
    Settings *settings = Settings::instance();
    int maxCount = settings ? settings->generatedPlaylistCount() : 5;
    
    // Count existing generated playlists and find oldest
    int genCount = 0;
    int oldestGenIdx = -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!m_tabs[i].isUserCreated) {
            ++genCount;
            if (oldestGenIdx < 0)
                oldestGenIdx = i;  // First generated = oldest
        }
    }
    
    // If at max, remove oldest generated playlist
    if (genCount >= maxCount && oldestGenIdx >= 0) {
        QString oldestId = m_tabs[oldestGenIdx].uuid.toString();
        closeTab(oldestId);
    }
    
    // Create new generated playlist
    return createNewTab(tabName, false);
}

void PlaylistStore::markGeneratedPlaylistDirty(const QUuid &uuid)
{
    if (uuid.isNull() || m_dirtyTrackingSuppressed.contains(uuid))
        return;

    const int idx = indexOfUuid(uuid);
    if (idx < 0 || m_tabs[idx].isUserCreated)
        return;

    if (!m_tabs[idx].name.endsWith(QLatin1Char('*')))
        m_tabs[idx].name.append(QLatin1Char('*'));
}

int PlaylistStore::indexOfUuid(const QUuid &uuid) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].uuid == uuid)
            return i;
    }
    return -1;
}

namespace {
QString normalizePlaylistFilePath(const QString &filePath)
{
    QString normalized = filePath.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QUrl url(normalized);
    if (url.isValid() && url.isLocalFile()) {
        normalized = url.toLocalFile();
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(normalized));
}
}

QString PlaylistStore::importPlaylist(const QString &filePath)
{
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    QString uuid = createNewTab(QFileInfo(normalizedPath).baseName());
    TrackListModel *model = getPlaylistModel(uuid);
    if (!model)
        return QString();
    
    bool utf8 = normalizedPath.endsWith(".m3u8", Qt::CaseInsensitive);
    if (!importM3U(model, normalizedPath, utf8)) {
        closeTab(uuid);
        return QString();
    }
    return uuid;
}

bool PlaylistStore::exportPlaylist(const QString &uuid, const QString &filePath)
{
    TrackListModel *model = getPlaylistModel(uuid);
    if (!model)
        return false;
    
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    if (normalizedPath.isEmpty())
        return false;
    
    bool utf8 = normalizedPath.endsWith(".m3u8", Qt::CaseInsensitive);
    return exportM3U(model, normalizedPath, utf8);
}

bool PlaylistStore::importM3U(TrackListModel *model, const QString &filePath, bool utf8)
{
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    
    QTextStream in(&file);
    in.setEncoding(utf8 ? QStringConverter::Utf8 : QStringConverter::Latin1);
    
    QString playlistDir = QFileInfo(normalizedPath).absolutePath();

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        QString fullPath = QDir::isAbsolutePath(line) ? line : QDir::cleanPath(playlistDir + '/' + line);
        TrackInfo track = MetadataExtractor::extractTrackInfo(fullPath);
        if (track.isValid())
            model->addTrack(track);
    }
    return true;
}

bool PlaylistStore::exportM3U(TrackListModel *model, const QString &filePath, bool utf8)
{
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    QFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(utf8 ? QStringConverter::Utf8 : QStringConverter::Latin1);
    out << "#EXTM3U\n";

    for (int i = 0; i < model->count(); ++i) {
        TrackInfo track = model->trackAt(i);
        out << "#EXTINF:" << track.durationMs / 1000 << "," << track.artist << " - " << track.title << "\n";
        out << track.filePath << "\n";
    }
    return true;
}

QString PlaylistStore::importPlaylistAsync(const QString &filePath)
{
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    QString uuid = createNewTab(QFileInfo(normalizedPath).baseName());
    bool utf8 = normalizedPath.endsWith(".m3u8", Qt::CaseInsensitive);

    // Run import in background thread
    m_importFuture = QtConcurrent::run([this, uuid, normalizedPath, utf8]() {
        importM3UAsync(uuid, normalizedPath, utf8);
    });

    return uuid;
}

void PlaylistStore::importM3UAsync(const QString &uuid, const QString &filePath, bool utf8)
{
    const QString normalizedPath = normalizePlaylistFilePath(filePath);
    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMetaObject::invokeMethod(this, [this, uuid]() {
            closeTab(uuid);
            emit importFinished(uuid, false);
        }, Qt::QueuedConnection);
        return;
    }

    // First pass: count lines for progress
    QTextStream countStream(&file);
    countStream.setEncoding(utf8 ? QStringConverter::Utf8 : QStringConverter::Latin1);
    int totalLines = 0;
    while (!countStream.atEnd()) {
        QString line = countStream.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            ++totalLines;
    }
    file.seek(0);

    // Second pass: extract metadata and batch insert
    QTextStream in(&file);
    in.setEncoding(utf8 ? QStringConverter::Utf8 : QStringConverter::Latin1);
    QString playlistDir = QFileInfo(normalizedPath).absolutePath();

    QVector<TrackInfo> batch;
    int imported = 0;
    constexpr int BATCH_SIZE = 50;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        
        QString fullPath = QDir::isAbsolutePath(line) ? line : QDir::cleanPath(playlistDir + '/' + line);
        TrackInfo track = MetadataExtractor::extractTrackInfo(fullPath);
        if (track.isValid()) {
            batch.append(track);
            ++imported;
        }
        
        // Send batch to UI thread when full
        if (batch.size() >= BATCH_SIZE) {
            QVector<TrackInfo> toSend = batch;
            batch.clear();
            int currentImported = imported;
            QMetaObject::invokeMethod(this, [this, uuid, toSend, currentImported, totalLines]() {
                TrackListModel *model = getPlaylistModel(uuid);
                if (model) {
                    model->addTracks(toSend);
                }
                emit importProgress(uuid, currentImported, totalLines);
            }, Qt::QueuedConnection);
        }
    }
    
    // Send remaining tracks
    if (!batch.isEmpty()) {
        QVector<TrackInfo> toSend = batch;
        int currentImported = imported;
        QMetaObject::invokeMethod(this, [this, uuid, toSend, currentImported, totalLines]() {
            TrackListModel *model = getPlaylistModel(uuid);
            if (model) {
                model->addTracks(toSend);
            }
            emit importProgress(uuid, currentImported, totalLines);
        }, Qt::QueuedConnection);
    }
    
    // Signal completion
    QMetaObject::invokeMethod(this, [this, uuid]() {
        emit importFinished(uuid, true);
    }, Qt::QueuedConnection);
}
