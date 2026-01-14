#include "QueueManager.h"

QueueManager::QueueManager(QObject *parent)
    : QObject(parent)
{
}

void QueueManager::setPlaylistModel(TrackListModel *model)
{
    if (m_model == model)
        return;
    
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    
    m_model = model;
    m_currentIndex = -1;
    
    if (m_model) {
        connect(m_model, &QAbstractItemModel::rowsInserted,
                this, &QueueManager::onRowsInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved,
                this, &QueueManager::onRowsRemoved);
        connect(m_model, &QAbstractItemModel::rowsMoved,
                this, &QueueManager::onRowsMoved);
        connect(m_model, &QAbstractItemModel::modelReset,
                this, &QueueManager::onModelReset);
    }
    
    emit currentIndexChanged(m_currentIndex);
}

void QueueManager::setCurrentIndex(int index)
{
    if (!m_model) {
        updateCurrentIndex(-1);
        return;
    }
    
    int count = m_model->rowCount();
    if (index < 0 || index >= count) {
        updateCurrentIndex(-1);
        return;
    }
    
    updateCurrentIndex(index);
}

TrackInfo QueueManager::currentTrack() const
{
    if (!m_model || m_currentIndex < 0 || m_currentIndex >= m_model->rowCount())
        return TrackInfo();
    return m_model->trackAt(m_currentIndex);
}

TrackInfo QueueManager::peekNextTrack() const
{
    if (!m_model || m_currentIndex < 0)
        return TrackInfo();
    
    int nextIndex = m_currentIndex + 1;
    if (nextIndex >= m_model->rowCount())
        return TrackInfo();
    
    return m_model->trackAt(nextIndex);
}

TrackInfo QueueManager::peekPreviousTrack() const
{
    if (!m_model || m_currentIndex <= 0)
        return TrackInfo();
    
    return m_model->trackAt(m_currentIndex - 1);
}

bool QueueManager::canAdvance() const
{
    if (!m_model || m_currentIndex < 0)
        return false;
    return m_currentIndex + 1 < m_model->rowCount();
}

bool QueueManager::canRetreat() const
{
    return m_currentIndex > 0;
}

void QueueManager::advance()
{
    if (!canAdvance())
        return;
    updateCurrentIndex(m_currentIndex + 1);
}

void QueueManager::retreat()
{
    if (!canRetreat())
        return;
    updateCurrentIndex(m_currentIndex - 1);
}

void QueueManager::reset()
{
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        m_currentTrackPath.clear();
        emit currentIndexChanged(m_currentIndex);
    }
}

void QueueManager::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(last)
    
    if (!m_model)
        return;
    
    // If we have a tracked path but lost the index, try to find it
    if (m_currentIndex < 0 && !m_currentTrackPath.isEmpty()) {
        findCurrentTrackByPath();
        return;
    }
    
    // Only adjust current index if items were inserted before it
    if (m_currentIndex >= 0 && m_currentIndex >= first) {
        int count = last - first + 1;
        m_currentIndex += count;
        emit currentIndexChanged(m_currentIndex);
    }
}

void QueueManager::onRowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent)
    
    if (m_currentIndex < 0)
        return;
    
    int count = last - first + 1;
    
    if (m_currentIndex >= first && m_currentIndex <= last) {
        // The currently playing track was removed from the list
        // Keep the path so we can find it if it's re-inserted (reordering)
        // Don't clear m_currentTrackPath - it will help us find the track after reorder
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    } else if (m_currentIndex > last) {
        m_currentIndex -= count;
        emit currentIndexChanged(m_currentIndex);
    }
}

void QueueManager::onRowsMoved(const QModelIndex &parent, int start, int end,
                                const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(destination)
    
    if (m_currentIndex < 0)
        return;
    
    int from = start;
    int to = row > start ? row - 1 : row;
    
    if (m_currentIndex == from) {
        m_currentIndex = to;
        emit currentIndexChanged(m_currentIndex);
    } else if (from < m_currentIndex && to >= m_currentIndex) {
        m_currentIndex--;
        emit currentIndexChanged(m_currentIndex);
    } else if (from > m_currentIndex && to <= m_currentIndex) {
        m_currentIndex++;
        emit currentIndexChanged(m_currentIndex);
    }
    
    Q_UNUSED(end)
}

void QueueManager::onModelReset()
{
    // Don't auto-select on model reset - playback is independent
    // Just invalidate the current position
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    }
}

void QueueManager::updateCurrentIndex(int newIndex)
{
    if (m_currentIndex != newIndex) {
        m_currentIndex = newIndex;
        
        // Update tracked path
        TrackInfo track = currentTrack();
        m_currentTrackPath = track.isValid() ? track.filePath : QString();
        
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged(track);
    }
}

void QueueManager::findCurrentTrackByPath()
{
    if (!m_model || m_currentTrackPath.isEmpty())
        return;
    
    int newIndex = m_model->indexOf(m_currentTrackPath);
    if (newIndex >= 0) {
        m_currentIndex = newIndex;
        emit currentIndexChanged(m_currentIndex);
    }
}
