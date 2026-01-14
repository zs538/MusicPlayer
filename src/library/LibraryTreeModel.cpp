#include "LibraryTreeModel.h"
#include <QFileInfo>
#include <algorithm>

LibraryTreeModel::LibraryTreeModel(LibraryDatabase *db, QObject *parent)
    : QAbstractListModel(parent)
    , m_db(db)
    , m_groupingLevels({"albumartist", "year-album", "disc"})
{
    connect(m_db, &LibraryDatabase::databaseChanged, this, &LibraryTreeModel::onDatabaseChanged);
    rebuildTree();
}

int LibraryTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.count();
}

QVariant LibraryTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.count())
        return QVariant();
    
    const TreeItem &item = m_items.at(index.row());
    
    switch (role) {
    case ItemTypeRole:
        return item.itemType;
    case DisplayTextRole:
    case Qt::DisplayRole:
        return item.displayText;
    case NodeKeyRole:
        return item.nodeKey;
    case IndentRole:
        return item.indent;
    case TrackNumberRole:
        return item.trackNumber;
    case DiscNumberRole:
        return item.discNumber;
    case DurationMsRole:
        return item.durationMs;
    case FilePathRole:
        return item.filePath;
    case AlbumRole:
        return item.album;
    case ArtistRole:
        return item.artist;
    case AlbumArtistRole:
        return item.albumArtist;
    case YearRole:
        return item.year;
    case ChildCountRole:
        return item.childCount;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LibraryTreeModel::roleNames() const
{
    return {
        {ItemTypeRole, "itemType"},
        {DisplayTextRole, "displayText"},
        {NodeKeyRole, "nodeKey"},
        {IndentRole, "indent"},
        {TrackNumberRole, "trackNumber"},
        {DiscNumberRole, "discNumber"},
        {DurationMsRole, "durationMs"},
        {FilePathRole, "filePath"},
        {AlbumRole, "album"},
        {ArtistRole, "artist"},
        {AlbumArtistRole, "albumArtist"},
        {YearRole, "year"},
        {ChildCountRole, "childCount"}
    };
}

void LibraryTreeModel::setSearchQuery(const QString &query)
{
    if (m_searchQuery != query) {
        m_searchQuery = query;
        emit searchQueryChanged();
        rebuildTree();
    }
}

void LibraryTreeModel::setGroupingLevels(const QStringList &levels)
{
    if (m_groupingLevels != levels) {
        m_groupingLevels = levels;
        emit groupingLevelsChanged();
        rebuildTree();
    }
}

QStringList LibraryTreeModel::availableGroupings() const
{
    return {
        "none",
        "artist",
        "albumartist",
        "album",
        "disc",
        "genre",
        "year",
        "year-album"
    };
}

void LibraryTreeModel::refresh()
{
    rebuildTree();
}

QVariantList LibraryTreeModel::tracksForNode(const QString &nodeKey) const
{
    QVariantList result;
    
    const TreeNode *node = findNode(nodeKey);
    if (!node)
        return result;
    
    QVector<LibraryTrack> tracks;
    collectTracksFromNode(*node, tracks);
    
    // Sort tracks according to the current grouping levels hierarchy
    // This respects the user's chosen organization (e.g., albumartist > year-album > disc)
    QStringList levels = m_groupingLevels;
    std::sort(tracks.begin(), tracks.end(), [this, &levels](const LibraryTrack &a, const LibraryTrack &b) {
        // Sort by each grouping level in order
        for (const QString &level : levels) {
            if (level == "none") continue;
            QString keyA = getGroupKey(a, level);
            QString keyB = getGroupKey(b, level);
            if (keyA != keyB)
                return keyA < keyB;
        }
        // Finally sort by disc number, then track number
        if (a.discNumber != b.discNumber)
            return a.discNumber < b.discNumber;
        return a.trackNumber < b.trackNumber;
    });
    
    for (const LibraryTrack &track : tracks) {
        QVariantMap map;
        map["filePath"] = track.filePath;
        map["title"] = track.title;
        map["artist"] = track.artist;
        map["album"] = track.album;
        map["albumArtist"] = track.albumArtist;
        map["performer"] = track.performer;
        map["composer"] = track.composer;
        map["year"] = track.year;
        map["originalYear"] = track.originalYear;
        map["trackNumber"] = track.trackNumber;
        map["discNumber"] = track.discNumber;
        map["durationMs"] = track.durationMs;
        map["genre"] = track.genre;
        map["sampleRate"] = track.sampleRate;
        map["bitDepth"] = track.bitDepth;
        map["bitrate"] = track.bitrate;
        map["url"] = track.url;
        map["fileName"] = track.fileName;
        map["fileSize"] = track.fileSize;
        map["fileType"] = track.fileType;
        map["dateCreated"] = QDateTime::fromSecsSinceEpoch(track.createdTime);
        map["dateModified"] = QDateTime::fromSecsSinceEpoch(track.modifiedTime);
        map["comment"] = track.comment;
        map["bpm"] = track.bpm;
        map["initialKey"] = track.initialKey;
        result.append(map);
    }
    
    return result;
}

void LibraryTreeModel::onDatabaseChanged()
{
    rebuildTree();
}

void LibraryTreeModel::rebuildTree()
{
    beginResetModel();
    m_items.clear();
    m_rootNode = TreeNode();
    
    QVector<LibraryTrack> tracks;
    if (!m_searchQuery.isEmpty()) {
        tracks = m_db->searchTracks(m_searchQuery);
    } else {
        tracks = m_db->allTracks();
    }
    
    buildTreeFromTracks(tracks);
    
    // Build flat list from top level only
    QStringList keys = m_rootNode.children.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    
    // Check if we have grouping
    bool hasGrouping = !m_groupingLevels.isEmpty() && m_groupingLevels[0] != "none";
    
    if (hasGrouping) {
        for (const QString &key : keys) {
            const TreeNode &node = m_rootNode.children[key];
            TreeItem item;
            item.itemType = "group";
            item.displayText = key;
            item.nodeKey = key;
            item.indent = 0;
            item.filePath = node.representativeFilePath;
            item.album = node.representativeAlbum;
            item.artist = node.representativeArtist;
            item.year = node.year;
            item.childCount = countChildrenForNode(node);
            m_items.append(item);
        }
    } else {
        // No grouping - flat list of tracks
        for (const LibraryTrack &track : tracks) {
            TreeItem item;
            item.itemType = "track";
            item.displayText = track.title.isEmpty() ? QFileInfo(track.filePath).fileName() : track.title;
            item.nodeKey = "";
            item.indent = 0;
            item.trackNumber = track.trackNumber;
            item.discNumber = track.discNumber;
            item.durationMs = track.durationMs;
            item.filePath = track.filePath;
            item.album = track.album;
            item.artist = track.artist;
            item.albumArtist = track.albumArtist;
            item.year = 0;
            m_items.append(item);
        }
    }
    
    endResetModel();
    emit countChanged();
}

QString LibraryTreeModel::getGroupKey(const LibraryTrack &track, const QString &level) const
{
    if (level == "artist")
        return track.artist.isEmpty() ? "Unknown Artist" : track.artist;
    if (level == "albumartist")
        return track.albumArtist.isEmpty() ? (track.artist.isEmpty() ? "Unknown Artist" : track.artist) : track.albumArtist;
    if (level == "album")
        return track.album.isEmpty() ? "Unknown Album" : track.album;
    if (level == "disc")
        return track.discNumber > 0 ? QString("Disc %1").arg(track.discNumber) : "Disc 1";
    if (level == "genre")
        return track.genre.isEmpty() ? "Unknown Genre" : track.genre;
    if (level == "year")
        return track.year > 0 ? QString::number(track.year) : "Unknown Year";
    if (level == "year-album") {
        QString yearPart = track.year > 0 ? QString::number(track.year) + " - " : "";
        QString albumPart = track.album.isEmpty() ? "Unknown Album" : track.album;
        return yearPart + albumPart;
    }
    return "";
}

void LibraryTreeModel::buildTreeFromTracks(const QVector<LibraryTrack> &tracks)
{
    m_rootNode = TreeNode();
    
    QStringList activeLevels;
    for (const QString &level : m_groupingLevels) {
        if (level != "none")
            activeLevels.append(level);
    }
    
    if (activeLevels.isEmpty()) {
        // No grouping - tracks go directly to root
        m_rootNode.tracks = tracks;
        return;
    }
    
    for (const LibraryTrack &track : tracks) {
        TreeNode *currentNode = &m_rootNode;
        
        for (int i = 0; i < activeLevels.count(); i++) {
            QString key = getGroupKey(track, activeLevels[i]);
            
            if (!currentNode->children.contains(key)) {
                TreeNode newNode;
                newNode.representativeFilePath = track.filePath;
                newNode.representativeAlbum = track.album;
                newNode.representativeArtist = track.albumArtist.isEmpty() ? track.artist : track.albumArtist;
                newNode.year = track.year;
                currentNode->children[key] = newNode;
            }
            
            currentNode = &currentNode->children[key];
        }
        
        // Add track to the deepest node
        currentNode->tracks.append(track);
    }
}

void LibraryTreeModel::collectTracksFromNode(const TreeNode &node, QVector<LibraryTrack> &tracks) const
{
    // Add direct tracks
    tracks.append(node.tracks);
    
    // Recursively add from children
    for (auto it = node.children.constBegin(); it != node.children.constEnd(); ++it) {
        collectTracksFromNode(it.value(), tracks);
    }
}

const LibraryTreeModel::TreeNode* LibraryTreeModel::findNode(const QString &nodeKey) const
{
    if (nodeKey.isEmpty())
        return &m_rootNode;
    
    QStringList parts = nodeKey.split("|||");
    const TreeNode *current = &m_rootNode;
    
    for (const QString &part : parts) {
        auto it = current->children.constFind(part);
        if (it == current->children.constEnd())
            return nullptr;
        current = &it.value();
    }
    
    return current;
}

int LibraryTreeModel::countChildrenForNode(const TreeNode &node) const
{
    int count = node.tracks.count();
    for (auto it = node.children.constBegin(); it != node.children.constEnd(); ++it) {
        count += countChildrenForNode(it.value());
    }
    return count;
}
