#include "FileBrowserModel.h"
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

static const QSet<QString> kAudio = {
    "mp3","flac","wav","ogg","opus","aac","m4a","wma","aiff","aif","ape","wv","tta","mka","mp4","mkv","webm"
};
static const QSet<QString> kPlaylist = {"m3u","m3u8"};

FileBrowserModel::FileBrowserModel(QObject *parent) : QAbstractListModel(parent)
{
    goHome();
}

int FileBrowserModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant FileBrowserModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_entries.size()) return {};
    const auto &e = m_entries[idx.row()];
    switch (role) {
    case FileNameRole:  return e.name;
    case FilePathRole:  return e.path;
    case FileUrlRole:   return QUrl::fromLocalFile(e.path);
    case IsDirRole:     return e.isDir;
    case EntryTypeRole: return e.type;
    }
    return {};
}

QHash<int, QByteArray> FileBrowserModel::roleNames() const
{
    return {{FileNameRole,"fileName"},{FilePathRole,"filePath"},{FileUrlRole,"fileUrl"},{IsDirRole,"isDir"},{EntryTypeRole,"entryType"}};
}

QString FileBrowserModel::displayPath() const { return QDir::toNativeSeparators(m_path); }

void FileBrowserModel::goTo(const QString &p)
{
    QString path = p.startsWith("file:") ? QUrl(p).toLocalFile() : p;
    QFileInfo fi(path);
    navigate(fi.isFile() ? fi.absolutePath() : fi.absoluteFilePath(), true);
}

void FileBrowserModel::goBack()    { if (canGoBack())    { m_historyIdx--; navigate(m_history[m_historyIdx], false); } }
void FileBrowserModel::goForward() { if (canGoForward()) { m_historyIdx++; navigate(m_history[m_historyIdx], false); } }
void FileBrowserModel::goUp()      { QDir d(m_path); if (d.cdUp()) navigate(d.absolutePath(), true); }
void FileBrowserModel::goHome()    { navigate(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), true); }

void FileBrowserModel::navigate(const QString &path, bool addToHistory)
{
    QString p = QDir::cleanPath(path);
    QDir dir(p);
    if (!dir.exists() || p == m_path) return;
    p = dir.absolutePath();

    if (addToHistory) {
        m_history = m_history.mid(0, m_historyIdx + 1);
        m_history.append(p);
        m_historyIdx = m_history.size() - 1;
    }
    m_path = p;
    load();
    emit pathChanged();
}

void FileBrowserModel::load()
{
    QList<Entry> entries;
    QDir dir(m_path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    for (const auto &fi : dir.entryInfoList()) {
        QString type;
        if (fi.isDir()) type = "dir";
        else if (kAudio.contains(fi.suffix().toLower())) type = "audio";
        else if (kPlaylist.contains(fi.suffix().toLower())) type = "playlist";
        else continue;
        entries.append({fi.fileName(), fi.absoluteFilePath(), type, fi.isDir()});
    }

    QCollator coll; coll.setCaseSensitivity(Qt::CaseInsensitive); coll.setNumericMode(true);
    std::sort(entries.begin(), entries.end(), [&](const Entry &a, const Entry &b) {
        return a.isDir != b.isDir ? a.isDir : coll.compare(a.name, b.name) < 0;
    });

    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}
