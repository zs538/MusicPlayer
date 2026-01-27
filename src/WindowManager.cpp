#include "WindowManager.h"
#include <QUuid>

// WindowListModel implementation

WindowListModel::WindowListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WindowListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_windows.count();
}

QVariant WindowListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_windows.count())
        return QVariant();
    
    const WindowInfo &info = m_windows[index.row()];
    
    switch (role) {
    case WindowIdRole: return info.windowId;
    case WindowTypeRole: return info.windowType;
    case TitleRole: return info.title;
    case PanelStateRole: return info.panelState;
    case GeometryRole: return info.geometry;
    default: return QVariant();
    }
}

QHash<int, QByteArray> WindowListModel::roleNames() const
{
    return {
        {WindowIdRole, "windowId"},
        {WindowTypeRole, "windowType"},
        {TitleRole, "title"},
        {PanelStateRole, "panelState"},
        {GeometryRole, "geometry"}
    };
}

void WindowListModel::addWindow(const WindowInfo &info)
{
    beginInsertRows(QModelIndex(), m_windows.count(), m_windows.count());
    m_windows.append(info);
    endInsertRows();
}

void WindowListModel::removeWindow(const QString &windowId)
{
    int idx = indexOf(windowId);
    if (idx < 0)
        return;
    
    beginRemoveRows(QModelIndex(), idx, idx);
    m_windows.removeAt(idx);
    endRemoveRows();
}

void WindowListModel::updateGeometry(const QString &windowId, const QVariantMap &geometry)
{
    int idx = indexOf(windowId);
    if (idx < 0)
        return;
    
    m_windows[idx].geometry = geometry;
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {GeometryRole});
}

int WindowListModel::indexOf(const QString &windowId) const
{
    for (int i = 0; i < m_windows.count(); ++i) {
        if (m_windows[i].windowId == windowId)
            return i;
    }
    return -1;
}

// WindowManager implementation

WindowManager *WindowManager::s_instance = nullptr;

WindowManager::WindowManager(QObject *parent)
    : QObject(parent)
    , m_windowsModel(new WindowListModel(this))
{
    s_instance = this;
}

WindowManager *WindowManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new WindowManager(qmlEngine);
    }
    return s_instance;
}

WindowManager *WindowManager::instance()
{
    return s_instance;
}

QString WindowManager::openCollectionWindow(const QVariantMap &panelState)
{
    QString windowId = QString("win_%1").arg(m_nextWindowId++);
    
    WindowListModel::WindowInfo info;
    info.windowId = windowId;
    info.windowType = "collection";
    info.title = panelState.value("title").toString();
    info.panelState = panelState;
    
    m_windowsModel->addWindow(info);
    emit windowOpened(windowId, panelState);
    
    return windowId;
}

void WindowManager::closeWindow(const QString &windowId)
{
    m_windowsModel->removeWindow(windowId);
    emit windowClosed(windowId);
}
