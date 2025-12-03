#pragma once

#include <QAbstractListModel>
#include <QUuid>
#include <QDateTime>
#include <QVector>
#include <QHash>
#include <QStandardPaths>
#include <QDir>

#include "PlaylistModel.h"

/**
 * @brief Manages multiple playlist tabs and provides a model for the tab bar.
 * 
 * PlaylistManager serves dual purposes:
 * 1. A QAbstractListModel for the tab bar UI (exposes tab metadata)
 * 2. Owner and manager of all PlaylistModel instances
 * 
 * Key concepts:
 * - Active playlist: The one driving playback (only one at a time)
 * - Displayed playlist: The one currently shown in the UI (can differ from active)
 */
class PlaylistManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString activePlaylistId READ activePlaylistId NOTIFY activePlaylistChanged)
    Q_PROPERTY(QString displayedPlaylistId READ displayedPlaylistId WRITE setDisplayedPlaylistId NOTIFY displayedPlaylistChanged)
    Q_PROPERTY(PlaylistModel* displayedPlaylist READ displayedPlaylist NOTIFY displayedPlaylistChanged)
    Q_PROPERTY(PlaylistModel* activePlaylist READ activePlaylist NOTIFY activePlaylistChanged)
    Q_PROPERTY(int displayedIndex READ displayedIndex NOTIFY displayedPlaylistChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabCountChanged)

public:
    explicit PlaylistManager(QObject* parent = nullptr);
    ~PlaylistManager() override;

    // Tab metadata roles for QML
    enum Roles {
        UuidRole = Qt::UserRole + 1,
        NameRole,
        IsActiveRole,
        IsDirtyRole,
        IsTemporaryRole,
        TrackCountRole
    };

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Tab management (Q_INVOKABLE for QML)
    Q_INVOKABLE QString createNewTab(const QString& name = QString());
    Q_INVOKABLE void closeTab(const QString& uuid);
    Q_INVOKABLE void setActivePlaylist(const QString& uuid);
    Q_INVOKABLE void renameTab(const QString& uuid, const QString& name);
    Q_INVOKABLE void moveTab(int from, int to);
    Q_INVOKABLE QString duplicateTab(const QString& uuid);

    // Display control
    QString displayedPlaylistId() const;
    void setDisplayedPlaylistId(const QString& uuid);
    int displayedIndex() const;

    // Active playlist
    QString activePlaylistId() const;
    PlaylistModel* activePlaylist() const;
    PlaylistModel* displayedPlaylist() const;

    // Playlist access
    Q_INVOKABLE PlaylistModel* getPlaylistModel(const QString& uuid) const;
    int tabCount() const { return m_tabs.size(); }

    // Session persistence
    void saveSession();
    void loadSession();

signals:
    void activePlaylistChanged(const QString& uuid);
    void displayedPlaylistChanged(const QString& uuid);
    void tabCountChanged();
    void tabClosed(const QString& uuid);

private:
    struct PlaylistTab {
        QUuid uuid;
        QString name;
        bool isActive {false};
        bool isDirty {false};
        bool isTemporary {true};  // Session playlist (no backing file)
        QString backingFilePath;
        QDateTime lastModified;
        PlaylistModel* model {nullptr};
    };

    QString generateUniqueName() const;
    int findTabIndex(const QUuid& uuid) const;
    void emitTabDataChanged(int index);
    QString sessionFilePath() const;

    QVector<PlaylistTab> m_tabs;
    QUuid m_activePlaylistUuid;
    QUuid m_displayedPlaylistUuid;
    int m_nextPlaylistNumber {1};
};
