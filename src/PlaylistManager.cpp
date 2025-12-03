#include "PlaylistManager.h"
#include "PlaylistModel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

PlaylistManager::PlaylistManager(QObject* parent)
    : QAbstractListModel(parent)
{
    // Create initial default tab
    createNewTab();
    
    // Make the first tab active and displayed
    if (!m_tabs.isEmpty()) {
        m_activePlaylistUuid = m_tabs.first().uuid;
        m_displayedPlaylistUuid = m_tabs.first().uuid;
        m_tabs[0].isActive = true;
    }
}

PlaylistManager::~PlaylistManager()
{
    // PlaylistModel instances are children of this, Qt handles cleanup
}

int PlaylistManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_tabs.size();
}

QVariant PlaylistManager::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tabs.size())
        return {};

    const auto& tab = m_tabs.at(index.row());
    switch (role) {
    case UuidRole:
        return tab.uuid.toString(QUuid::WithoutBraces);
    case NameRole:
        return tab.name;
    case IsActiveRole:
        return tab.isActive;
    case IsDirtyRole:
        return tab.isDirty;
    case IsTemporaryRole:
        return tab.isTemporary;
    case TrackCountRole:
        return tab.model ? tab.model->rowCount() : 0;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistManager::roleNames() const
{
    return {
        {UuidRole, "uuid"},
        {NameRole, "name"},
        {IsActiveRole, "isActive"},
        {IsDirtyRole, "isDirty"},
        {IsTemporaryRole, "isTemporary"},
        {TrackCountRole, "trackCount"}
    };
}

QString PlaylistManager::generateUniqueName() const
{
    // Find the next available "Playlist X" number
    int maxNum = 0;
    for (const auto& tab : m_tabs) {
        if (tab.name.startsWith("Playlist ")) {
            bool ok;
            int num = tab.name.mid(9).toInt(&ok);
            if (ok && num > maxNum) {
                maxNum = num;
            }
        }
    }
    return QString("Playlist %1").arg(maxNum + 1);
}

int PlaylistManager::findTabIndex(const QUuid& uuid) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].uuid == uuid) return i;
    }
    return -1;
}

void PlaylistManager::emitTabDataChanged(int index)
{
    if (index >= 0 && index < m_tabs.size()) {
        QModelIndex mi = createIndex(index, 0);
        emit dataChanged(mi, mi);
    }
}

QString PlaylistManager::createNewTab(const QString& name)
{
    PlaylistTab tab;
    tab.uuid = QUuid::createUuid();
    tab.name = name.isEmpty() ? generateUniqueName() : name;
    tab.isTemporary = true;
    tab.isDirty = false;
    tab.lastModified = QDateTime::currentDateTime();
    tab.model = new PlaylistModel(this);

    beginInsertRows(QModelIndex(), m_tabs.size(), m_tabs.size());
    m_tabs.append(tab);
    endInsertRows();

    emit tabCountChanged();

    return tab.uuid.toString(QUuid::WithoutBraces);
}

void PlaylistManager::closeTab(const QString& uuid)
{
    QUuid qUuid(uuid);
    int index = findTabIndex(qUuid);
    if (index < 0) return;

    // Don't allow closing the last tab
    if (m_tabs.size() <= 1) {
        qWarning() << "Cannot close the last tab";
        return;
    }

    const auto& tab = m_tabs[index];
    bool wasActive = tab.isActive;
    bool wasDisplayed = (qUuid == m_displayedPlaylistUuid);

    // Clean up the model
    if (tab.model) {
        tab.model->deleteLater();
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_tabs.removeAt(index);
    endRemoveRows();

    emit tabCountChanged();
    emit tabClosed(uuid);

    // If we closed the active playlist, make another one active
    if (wasActive && !m_tabs.isEmpty()) {
        int newActiveIndex = qMin(index, m_tabs.size() - 1);
        setActivePlaylist(m_tabs[newActiveIndex].uuid.toString(QUuid::WithoutBraces));
    }

    // If we closed the displayed playlist, display another one
    if (wasDisplayed && !m_tabs.isEmpty()) {
        int newDisplayIndex = qMin(index, m_tabs.size() - 1);
        setDisplayedPlaylistId(m_tabs[newDisplayIndex].uuid.toString(QUuid::WithoutBraces));
    }
}

void PlaylistManager::setActivePlaylist(const QString& uuid)
{
    QUuid qUuid(uuid);
    if (qUuid == m_activePlaylistUuid) return;

    int oldIndex = findTabIndex(m_activePlaylistUuid);
    int newIndex = findTabIndex(qUuid);

    if (newIndex < 0) return;

    // Update old active tab
    if (oldIndex >= 0) {
        m_tabs[oldIndex].isActive = false;
        emitTabDataChanged(oldIndex);
    }

    // Update new active tab
    m_tabs[newIndex].isActive = true;
    m_activePlaylistUuid = qUuid;
    emitTabDataChanged(newIndex);

    emit activePlaylistChanged(uuid);
}

void PlaylistManager::renameTab(const QString& uuid, const QString& name)
{
    QUuid qUuid(uuid);
    int index = findTabIndex(qUuid);
    if (index < 0 || name.isEmpty()) return;

    m_tabs[index].name = name;
    m_tabs[index].isDirty = true;
    m_tabs[index].lastModified = QDateTime::currentDateTime();
    emitTabDataChanged(index);
}

void PlaylistManager::moveTab(int from, int to)
{
    if (from < 0 || from >= m_tabs.size() || from == to) return;
    to = qBound(0, to, m_tabs.size() - 1);
    if (from == to) return;

    beginMoveRows(QModelIndex(), from, from, QModelIndex(), (from < to) ? to + 1 : to);
    m_tabs.move(from, to);
    endMoveRows();
}

QString PlaylistManager::duplicateTab(const QString& uuid)
{
    QUuid qUuid(uuid);
    int index = findTabIndex(qUuid);
    if (index < 0) return {};

    const auto& sourceTab = m_tabs[index];
    
    // Create new tab with copied name
    QString newName = sourceTab.name + " (copy)";
    QString newUuid = createNewTab(newName);

    // Copy tracks from source to new playlist
    PlaylistModel* newModel = getPlaylistModel(newUuid);
    if (newModel && sourceTab.model) {
        for (int i = 0; i < sourceTab.model->rowCount(); ++i) {
            QUrl url = sourceTab.model->data(
                sourceTab.model->index(i), PlaylistModel::UrlRole).toUrl();
            newModel->add(url);
        }
    }

    return newUuid;
}

QString PlaylistManager::displayedPlaylistId() const
{
    return m_displayedPlaylistUuid.toString(QUuid::WithoutBraces);
}

void PlaylistManager::setDisplayedPlaylistId(const QString& uuid)
{
    QUuid qUuid(uuid);
    if (qUuid == m_displayedPlaylistUuid) return;

    int index = findTabIndex(qUuid);
    if (index < 0) return;

    m_displayedPlaylistUuid = qUuid;
    emit displayedPlaylistChanged(uuid);
}

int PlaylistManager::displayedIndex() const
{
    return findTabIndex(m_displayedPlaylistUuid);
}

QString PlaylistManager::activePlaylistId() const
{
    return m_activePlaylistUuid.toString(QUuid::WithoutBraces);
}

PlaylistModel* PlaylistManager::activePlaylist() const
{
    int index = findTabIndex(m_activePlaylistUuid);
    if (index < 0) return nullptr;
    return m_tabs[index].model;
}

PlaylistModel* PlaylistManager::displayedPlaylist() const
{
    int index = findTabIndex(m_displayedPlaylistUuid);
    if (index < 0) return nullptr;
    return m_tabs[index].model;
}

PlaylistModel* PlaylistManager::getPlaylistModel(const QString& uuid) const
{
    QUuid qUuid(uuid);
    int index = findTabIndex(qUuid);
    if (index < 0) return nullptr;
    return m_tabs[index].model;
}

QString PlaylistManager::sessionFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/session.json";
}

void PlaylistManager::saveSession()
{
    QJsonObject root;
    root["version"] = 1;
    root["activePlaylist"] = m_activePlaylistUuid.toString(QUuid::WithoutBraces);
    root["displayedPlaylist"] = m_displayedPlaylistUuid.toString(QUuid::WithoutBraces);

    QJsonArray tabsArray;
    for (const auto& tab : m_tabs) {
        QJsonObject tabObj;
        tabObj["uuid"] = tab.uuid.toString(QUuid::WithoutBraces);
        tabObj["name"] = tab.name;
        tabObj["isTemporary"] = tab.isTemporary;

        // Save tracks for session playlists
        if (tab.model) {
            QJsonArray tracksArray;
            for (int i = 0; i < tab.model->rowCount(); ++i) {
                QUrl url = tab.model->data(
                    tab.model->index(i), PlaylistModel::UrlRole).toUrl();
                tracksArray.append(url.toString());
            }
            tabObj["tracks"] = tracksArray;
        }

        tabsArray.append(tabObj);
    }
    root["tabs"] = tabsArray;

    QFile file(sessionFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        qDebug() << "Session saved to" << sessionFilePath();
    }
}

void PlaylistManager::loadSession()
{
    QFile file(sessionFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No session file found, using defaults";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    
    // Clear existing tabs (except we need at least one)
    beginResetModel();
    for (auto& tab : m_tabs) {
        if (tab.model) tab.model->deleteLater();
    }
    m_tabs.clear();

    // Load tabs
    QJsonArray tabsArray = root["tabs"].toArray();
    for (const auto& tabVal : tabsArray) {
        QJsonObject tabObj = tabVal.toObject();
        
        PlaylistTab tab;
        tab.uuid = QUuid(tabObj["uuid"].toString());
        tab.name = tabObj["name"].toString();
        tab.isTemporary = tabObj["isTemporary"].toBool(true);
        tab.model = new PlaylistModel(this);

        // Load tracks
        QJsonArray tracksArray = tabObj["tracks"].toArray();
        for (const auto& trackVal : tracksArray) {
            tab.model->add(QUrl(trackVal.toString()));
        }

        m_tabs.append(tab);
    }

    // Ensure at least one tab exists
    if (m_tabs.isEmpty()) {
        PlaylistTab tab;
        tab.uuid = QUuid::createUuid();
        tab.name = "Playlist 1";
        tab.isTemporary = true;
        tab.model = new PlaylistModel(this);
        m_tabs.append(tab);
    }

    endResetModel();

    // Restore active and displayed playlists
    QString activeId = root["activePlaylist"].toString();
    QString displayedId = root["displayedPlaylist"].toString();

    QUuid activeUuid(activeId);
    QUuid displayedUuid(displayedId);

    // Validate and set active playlist
    int activeIndex = findTabIndex(activeUuid);
    if (activeIndex >= 0) {
        m_activePlaylistUuid = activeUuid;
        m_tabs[activeIndex].isActive = true;
    } else if (!m_tabs.isEmpty()) {
        m_activePlaylistUuid = m_tabs.first().uuid;
        m_tabs[0].isActive = true;
    }

    // Validate and set displayed playlist
    int displayedIndex = findTabIndex(displayedUuid);
    if (displayedIndex >= 0) {
        m_displayedPlaylistUuid = displayedUuid;
    } else if (!m_tabs.isEmpty()) {
        m_displayedPlaylistUuid = m_tabs.first().uuid;
    }

    emit tabCountChanged();
    emit activePlaylistChanged(m_activePlaylistUuid.toString(QUuid::WithoutBraces));
    emit displayedPlaylistChanged(m_displayedPlaylistUuid.toString(QUuid::WithoutBraces));

    qDebug() << "Session loaded:" << m_tabs.size() << "tabs";
}
