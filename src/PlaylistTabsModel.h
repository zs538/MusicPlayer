#ifndef PLAYLISTTABSMODEL_H
#define PLAYLISTTABSMODEL_H

#include <QAbstractListModel>

class PlaylistStore;

/**
 * @brief PlaylistTabsModel is a pure UI model for the tab bar.
 * 
 * It reads tab data from PlaylistStore and exposes it to QML.
 * All actions are delegated to PlaylistStore.
 */
class PlaylistTabsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        UuidRole = Qt::UserRole + 1,
        NameRole,
        IsActiveRole,
        IsDisplayedRole,
        TrackCountRole,
        IsUserCreatedRole
    };
    Q_ENUM(Roles)

    explicit PlaylistTabsModel(PlaylistStore *store, QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void onTabInserted(int index);
    void onTabRemoved(int index);
    void onTabMoved(int from, int to);
    void onTabDataChanged(int index);

private:
    PlaylistStore *m_store;
};

#endif // PLAYLISTTABSMODEL_H
