#include "PlaylistStore.h"
#include "TrackListModel.h"
#include "MetadataExtractor.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>

PlaylistStore::PlaylistStore(QObject *parent)
    : QObject(parent)
{
    createNewTab(QStringLiteral("Playlist 1"));
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

QString PlaylistStore::createGeneratedTab(const QString &name)
{
    QString tabName = name.isEmpty() ? QStringLiteral("Generated") : name;
    return createNewTab(tabName, false);
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
    
    if (m_activeId == tab.uuid && !m_tabs.isEmpty()) {
        m_activeId = m_tabs[qBound(0, idx - 1, m_tabs.size() - 1)].uuid;
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

QString PlaylistStore::findGeneratedPlaylistId() const
{
    for (const Tab &tab : m_tabs) {
        if (!tab.isUserCreated)
            return tab.uuid.toString();
    }
    return QString();
}

int PlaylistStore::indexOfUuid(const QUuid &uuid) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].uuid == uuid)
            return i;
    }
    return -1;
}

QString PlaylistStore::importPlaylist(const QString &filePath)
{
    QString uuid = createNewTab(QFileInfo(filePath).baseName());
    TrackListModel *model = getPlaylistModel(uuid);
    if (!model)
        return QString();
    
    bool utf8 = filePath.endsWith(".m3u8", Qt::CaseInsensitive);
    if (!importM3U(model, filePath, utf8)) {
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
    
    bool utf8 = filePath.endsWith(".m3u8", Qt::CaseInsensitive);
    return exportM3U(model, filePath, utf8);
}

bool PlaylistStore::importM3U(TrackListModel *model, const QString &filePath, bool utf8)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    
    QTextStream in(&file);
    in.setEncoding(utf8 ? QStringConverter::Utf8 : QStringConverter::Latin1);
    
    QString playlistDir = QFileInfo(filePath).absolutePath();
    
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
    QFile file(filePath);
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
