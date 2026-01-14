#include "ViewedPlaylistRouter.h"
#include "PlaylistStore.h"
#include "AppViewModel.h"

ViewedPlaylistRouter *ViewedPlaylistRouter::s_instance = nullptr;

ViewedPlaylistRouter::ViewedPlaylistRouter(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

ViewedPlaylistRouter *ViewedPlaylistRouter::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new ViewedPlaylistRouter(qmlEngine);
    }
    return s_instance;
}

ViewedPlaylistRouter *ViewedPlaylistRouter::instance()
{
    return s_instance;
}

void ViewedPlaylistRouter::initialize(PlaylistStore *store, AppViewModel *app)
{
    m_app = app;
    connectToStore(store);
}

PlaylistStore *ViewedPlaylistRouter::store() const
{
    // Lazy initialization: connect signals on first access
    PlaylistStore *s = m_store ? m_store : (AppViewModel::instance() ? AppViewModel::instance()->playlistStore() : nullptr);
    if (s && !m_store) {
        // First time accessing store via AppViewModel - connect signals
        const_cast<ViewedPlaylistRouter*>(this)->connectToStore(s);
    }
    return s;
}

void ViewedPlaylistRouter::connectToStore(PlaylistStore *s)
{
    if (m_store == s)
        return;
    
    m_store = s;
    
    if (m_store) {
        connect(m_store, &PlaylistStore::displayedPlaylistChanged, this, [this]() {
            emit viewedPlaylistIdChanged();
            emit viewedPlaylistModelChanged();
        });
        connect(m_store, &PlaylistStore::activePlaylistChanged, this, [this]() {
            emit activePlaylistIdChanged();
            emit activePlaylistModelChanged();
        });
    }
}

QString ViewedPlaylistRouter::viewedPlaylistId() const
{
    PlaylistStore *s = store();
    return s ? s->displayedPlaylistIdString() : QString();
}

void ViewedPlaylistRouter::setViewedPlaylistId(const QString &id)
{
    PlaylistStore *s = store();
    if (s) {
        s->setDisplayedPlaylist(id);
    }
}

QAbstractItemModel *ViewedPlaylistRouter::viewedPlaylistModel() const
{
    PlaylistStore *s = store();
    return s ? s->displayedPlaylist() : nullptr;
}

QString ViewedPlaylistRouter::activePlaylistId() const
{
    PlaylistStore *s = store();
    return s ? s->activePlaylistIdString() : QString();
}

QAbstractItemModel *ViewedPlaylistRouter::activePlaylistModel() const
{
    PlaylistStore *s = store();
    return s ? s->activePlaylist() : nullptr;
}

void ViewedPlaylistRouter::setActiveToViewed()
{
    PlaylistStore *s = store();
    if (s) {
        s->setActivePlaylist(s->displayedPlaylistIdString());
    }
}

bool ViewedPlaylistRouter::hasViewedPlaylist() const
{
    PlaylistStore *s = store();
    return s && !s->displayedPlaylistIdString().isEmpty();
}
