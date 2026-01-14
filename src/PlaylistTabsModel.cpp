#include "PlaylistTabsModel.h"
#include "PlaylistStore.h"
#include "TrackListModel.h"

PlaylistTabsModel::PlaylistTabsModel(PlaylistStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    connect(m_store, &PlaylistStore::tabInserted, this, &PlaylistTabsModel::onTabInserted);
    connect(m_store, &PlaylistStore::tabRemoved, this, &PlaylistTabsModel::onTabRemoved);
    connect(m_store, &PlaylistStore::tabMoved, this, &PlaylistTabsModel::onTabMoved);
    connect(m_store, &PlaylistStore::tabDataChanged, this, &PlaylistTabsModel::onTabDataChanged);
}

int PlaylistTabsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_store->tabCount();
}

QVariant PlaylistTabsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_store->tabCount())
        return QVariant();

    const auto &tabs = m_store->tabs();
    const auto &tab = tabs[index.row()];
    
    switch (role) {
    case UuidRole: return tab.uuid.toString();
    case NameRole: return tab.name;
    case IsActiveRole: return tab.uuid == m_store->activePlaylistId();
    case IsDisplayedRole: return tab.uuid == m_store->displayedPlaylistId();
    case TrackCountRole: return tab.model ? tab.model->count() : 0;
    case IsUserCreatedRole: return tab.isUserCreated;
    default: return QVariant();
    }
}

QHash<int, QByteArray> PlaylistTabsModel::roleNames() const
{
    return {
        {UuidRole, "uuid"},
        {NameRole, "name"},
        {IsActiveRole, "isActive"},
        {IsDisplayedRole, "isDisplayed"},
        {TrackCountRole, "trackCount"},
        {IsUserCreatedRole, "isUserCreated"}
    };
}

void PlaylistTabsModel::onTabInserted(int index)
{
    beginInsertRows(QModelIndex(), index, index);
    endInsertRows();
}

void PlaylistTabsModel::onTabRemoved(int index)
{
    beginRemoveRows(QModelIndex(), index, index);
    endRemoveRows();
}

void PlaylistTabsModel::onTabMoved(int from, int to)
{
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    endMoveRows();
}

void PlaylistTabsModel::onTabDataChanged(int index)
{
    QModelIndex mi = this->index(index);
    emit dataChanged(mi, mi);
}
