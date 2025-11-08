#include "PlaylistModel.h"
#include "TrackMetadata.h"

PlaylistModel::PlaylistModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const auto &it = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayRole:
        return it.display;
    case UrlRole:
        return it.url;
    case TitleRole:
        return it.metadata ? it.metadata->title() : QString();
    case ArtistRole:
        return it.metadata ? it.metadata->artist() : QString();
    case AlbumRole:
        return it.metadata ? it.metadata->album() : QString();
    case GenreRole:
        return it.metadata ? it.metadata->genre() : QString();
    case YearRole:
        return it.metadata ? it.metadata->year() : QString();
    case TrackNumberRole:
        return it.metadata ? it.metadata->trackNumber() : 0;
    case DurationRole:
        return it.metadata ? it.metadata->duration() : 0;
    case MetadataRole:
        return QVariant::fromValue(it.metadata);
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    QHash<int, QByteArray> r;
    r[UrlRole] = "url";
    r[DisplayRole] = "display";
    r[TitleRole] = "title";
    r[ArtistRole] = "artist";
    r[AlbumRole] = "album";
    r[GenreRole] = "genre";
    r[YearRole] = "year";
    r[TrackNumberRole] = "trackNumber";
    r[DurationRole] = "duration";
    r[MetadataRole] = "metadata";
    return r;
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

void PlaylistModel::add(const QUrl& url)
{
    QString disp;
    const QString localFile = url.toLocalFile();
    if (!localFile.isEmpty()) {
        QFileInfo fi(localFile);
        disp = fi.exists() ? fi.fileName() : url.toString();
    } else {
        disp = url.toString();
    }

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.push_back({url, disp});
    endInsertRows();
}

void PlaylistModel::removeAt(int index)
{
    if (index < 0 || index >= m_items.size()) return;
    
    // Don't delete metadata - it belongs to PlayerController
    // Just clear the reference
    m_items[index].metadata = nullptr;
    
    beginRemoveRows(QModelIndex(), index, index);
    m_items.removeAt(index);
    endRemoveRows();
}

void PlaylistModel::moveRowTo(int from, int to)
{
    if (from < 0 || from >= m_items.size() || from == to) return;
    
    to = qBound(0, to, m_items.size() - 1);
    if (from == to) return;
    
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), (from < to) ? to + 1 : to);
    auto it = m_items.takeAt(from);
    m_items.insert(to, it);
    endMoveRows();
}

void PlaylistModel::clear()
{
    if (m_items.isEmpty()) return;
    beginResetModel();
    
    // Don't delete metadata objects - they belong to PlayerController
    // Just clear the references
    for (auto& item : m_items) {
        item.metadata = nullptr;
    }
    
    m_items.clear();
    endResetModel();
}

QVariantMap PlaylistModel::get(int index) const
{
    QVariantMap m;
    if (index < 0 || index >= m_items.size()) return m;
    m["url"] = m_items.at(index).url;
    m["display"] = m_items.at(index).display;
    return m;
}

static QString toLocalOrString(const QUrl& url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

bool PlaylistModel::exportM3U8(const QUrl& url) const
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream out(&f);
    out << "#EXTM3U\n";
    for (const auto &it : m_items) {
        out << toLocalOrString(it.url) << "\n";
    }
    return true;
}

bool PlaylistModel::importM3U8(const QUrl& url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    
    QTextStream in(&f);
    QVector<Item> items;
    items.reserve(100); // Pre-allocate for better performance
    
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        QUrl u;
        if (line.startsWith("http://") || line.startsWith("https://")) {
            u = QUrl(line);
        } else {
            u = QUrl::fromLocalFile(line);
        }
        
        QString disp;
        const QString localFile = u.toLocalFile();
        if (!localFile.isEmpty()) {
            QFileInfo fi(localFile);
            disp = fi.exists() ? fi.fileName() : u.toString();
        } else {
            disp = u.toString();
        }
        
        items.push_back({u, disp});
    }
    
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    return true;
}

void PlaylistModel::updateMetadata(int index, TrackMetadata* metadata)
{
    if (index < 0 || index >= m_items.size() || !metadata) return;
    
    // Don't delete old metadata - it belongs to PlayerController
    // Just clear the reference
    m_items[index].metadata = nullptr;
    
    // Store reference to current metadata (don't copy, don't take ownership)
    m_items[index].metadata = metadata;
    
    // Update display text with current metadata
    if (!metadata->title().isEmpty() && !metadata->artist().isEmpty()) {
        m_items[index].display = QString("%1 - %2").arg(metadata->artist(), metadata->title());
    }
    
    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
}

TrackMetadata* PlaylistModel::getMetadata(int index) const
{
    if (index < 0 || index >= m_items.size()) return nullptr;
    return m_items.at(index).metadata;
}
