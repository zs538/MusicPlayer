#ifndef LIBRARYTREEMODEL_H
#define LIBRARYTREEMODEL_H

#include <QAbstractListModel>
#include <QVariantMap>
#include <QStringList>
#include <QTimer>
#include "LibraryDatabase.h"

class LibraryTreeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QStringList groupingLevels READ groupingLevels WRITE setGroupingLevels NOTIFY groupingLevelsChanged)
    Q_PROPERTY(QStringList availableGroupings READ availableGroupings CONSTANT)

public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,  // "group" or "track"
        DisplayTextRole,
        NodeKeyRole,
        IndentRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationMsRole,
        FilePathRole,
        AlbumRole,
        ArtistRole,
        AlbumArtistRole,
        YearRole,
        ChildCountRole
    };
    Q_ENUM(Roles)
    
    explicit LibraryTreeModel(LibraryDatabase *db, QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    int count() const { return m_items.count(); }
    
    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString &query);
    
    QStringList groupingLevels() const { return m_groupingLevels; }
    void setGroupingLevels(const QStringList &levels);
    
    QStringList availableGroupings() const;
    
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantList tracksForNode(const QString &nodeKey) const;

signals:
    void countChanged();
    void searchQueryChanged();
    void groupingLevelsChanged();

private slots:
    void onDatabaseChanged();

private:
    struct TreeItem {
        QString itemType;      // "group" or "track"
        QString displayText;
        QString nodeKey;       // Hierarchical key like "Artist|||Album"
        int indent = 0;
        int trackNumber = 0;
        int discNumber = 0;
        qint64 durationMs = 0;
        QString filePath;
        QString album;
        QString artist;
        QString albumArtist;
        int year = 0;
        int childCount = 0;    // For groups: number of direct children
    };
    
    struct TreeNode {
        QMap<QString, TreeNode> children;
        QVector<LibraryTrack> tracks;
        QString representativeFilePath;
        QString representativeAlbum;
        QString representativeArtist;
        int year = 0;
    };
    
    void rebuildTree();
    QString getGroupKey(const LibraryTrack &track, const QString &level) const;
    void buildTreeFromTracks(const QVector<LibraryTrack> &tracks);
    void collectTracksFromNode(const TreeNode &node, QVector<LibraryTrack> &tracks) const;
    const TreeNode* findNode(const QString &nodeKey) const;
    int countChildrenForNode(const TreeNode &node) const;
    
    LibraryDatabase *m_db;
    QVector<TreeItem> m_items;
    TreeNode m_rootNode;
    QString m_searchQuery;
    QStringList m_groupingLevels;
    QTimer *m_rebuildDebounceTimer = nullptr;
};

#endif // LIBRARYTREEMODEL_H
