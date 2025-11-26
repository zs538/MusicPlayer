#include "PlaybackQueue.h"
#include "PlaylistModel.h"

#include <QDebug>
#include <QRandomGenerator>
#include <algorithm>

PlaybackQueue::PlaybackQueue(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<RepeatMode>("RepeatMode");
}

PlaybackQueue::~PlaybackQueue() = default;

void PlaybackQueue::setActivePlaylist(PlaylistModel* model)
{
    if (m_playlist == model) return;
    
    // Disconnect from old playlist
    if (m_playlist) {
        disconnect(m_playlist, nullptr, this, nullptr);
    }
    
    m_playlist = model;
    m_currentIndex = -1;
    m_shuffleOrder.clear();
    m_shufflePosition = -1;
    
    // Connect to new playlist
    if (m_playlist) {
        connect(m_playlist, &QAbstractItemModel::rowsInserted,
                this, &PlaybackQueue::onPlaylistRowsInserted);
        connect(m_playlist, &QAbstractItemModel::rowsRemoved,
                this, &PlaybackQueue::onPlaylistRowsRemoved);
        connect(m_playlist, &QAbstractItemModel::rowsMoved,
                this, &PlaybackQueue::onPlaylistRowsMoved);
        connect(m_playlist, &QAbstractItemModel::modelReset,
                this, &PlaybackQueue::onPlaylistReset);
    }
    
    emit playlistChanged();
    emit currentIndexChanged(m_currentIndex);
}

void PlaybackQueue::setCurrentIndex(int index)
{
    if (!m_playlist) {
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
        return;
    }
    
    int count = m_playlist->rowCount();
    if (index < -1 || index >= count) {
        index = -1;
    }
    
    if (m_currentIndex == index) return;
    
    m_currentIndex = index;
    
    // Update shuffle position if shuffle is enabled
    if (m_shuffle && index >= 0) {
        int pos = m_shuffleOrder.indexOf(index);
        if (pos >= 0) {
            m_shufflePosition = pos;
        } else {
            // Index not in shuffle order, regenerate
            generateShuffleOrder();
        }
    }
    
    emit currentIndexChanged(m_currentIndex);
}

TrackRef PlaybackQueue::current() const
{
    return trackRefAt(m_currentIndex);
}

TrackRef PlaybackQueue::peekNext() const
{
    int next = nextIndex();
    return trackRefAt(next);
}

TrackRef PlaybackQueue::peekPrevious() const
{
    int prev = previousIndex();
    return trackRefAt(prev);
}

void PlaybackQueue::advance()
{
    int next = nextIndex();
    if (next >= 0) {
        setCurrentIndex(next);
    } else {
        // End of playlist
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    }
}

void PlaybackQueue::goBack()
{
    int prev = previousIndex();
    if (prev >= 0) {
        setCurrentIndex(prev);
    }
}

bool PlaybackQueue::hasNext() const
{
    return nextIndex() >= 0;
}

bool PlaybackQueue::hasPrevious() const
{
    return previousIndex() >= 0;
}

void PlaybackQueue::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode == mode) return;
    m_repeatMode = mode;
    emit repeatModeChanged(mode);
}

void PlaybackQueue::setShuffle(bool enabled)
{
    if (m_shuffle == enabled) return;
    m_shuffle = enabled;
    
    if (enabled) {
        generateShuffleOrder();
    } else {
        m_shuffleOrder.clear();
        m_shufflePosition = -1;
    }
    
    emit shuffleChanged(enabled);
}

int PlaybackQueue::count() const
{
    return m_playlist ? m_playlist->rowCount() : 0;
}

bool PlaybackQueue::isEmpty() const
{
    return count() == 0;
}

TrackRef PlaybackQueue::trackRefAt(int index) const
{
    TrackRef ref;
    
    if (!m_playlist || index < 0 || index >= m_playlist->rowCount()) {
        return ref;
    }
    
    QModelIndex modelIndex = m_playlist->index(index, 0);
    QUrl url = m_playlist->data(modelIndex, PlaylistModel::UrlRole).toUrl();
    QString title = m_playlist->data(modelIndex, PlaylistModel::TitleRole).toString();
    
    if (url.isEmpty()) return ref;
    
    // Normalize URL
    if (url.scheme().isEmpty() && !url.path().isEmpty()) {
        url = QUrl::fromLocalFile(url.path());
    }
    
    ref.url = url;
    ref.displayName = title.isEmpty() ? url.fileName() : title;
    ref.playlistIndex = index;  // Store the playlist index for unique identification
    
    return ref;
}

int PlaybackQueue::nextIndex() const
{
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return -1;
    }
    
    int count = m_playlist->rowCount();
    
    // Repeat One: stay on current track
    if (m_repeatMode == RepeatMode::One && m_currentIndex >= 0) {
        return m_currentIndex;
    }
    
    // Shuffle mode
    if (m_shuffle && !m_shuffleOrder.isEmpty()) {
        int nextPos = m_shufflePosition + 1;
        
        if (nextPos < m_shuffleOrder.size()) {
            return m_shuffleOrder[nextPos];
        }
        
        // End of shuffle order
        if (m_repeatMode == RepeatMode::All) {
            return m_shuffleOrder.first();  // Wrap to beginning
        }
        return -1;  // End of playlist
    }
    
    // Normal sequential mode
    if (m_currentIndex < 0) {
        return 0;  // Start from beginning
    }
    
    int next = m_currentIndex + 1;
    
    if (next < count) {
        return next;
    }
    
    // End of playlist
    if (m_repeatMode == RepeatMode::All) {
        return 0;  // Wrap to beginning
    }
    
    return -1;  // No next track
}

int PlaybackQueue::previousIndex() const
{
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return -1;
    }
    
    int count = m_playlist->rowCount();
    
    // Repeat One: stay on current track
    if (m_repeatMode == RepeatMode::One && m_currentIndex >= 0) {
        return m_currentIndex;
    }
    
    // Shuffle mode
    if (m_shuffle && !m_shuffleOrder.isEmpty()) {
        int prevPos = m_shufflePosition - 1;
        
        if (prevPos >= 0) {
            return m_shuffleOrder[prevPos];
        }
        
        // Beginning of shuffle order
        if (m_repeatMode == RepeatMode::All) {
            return m_shuffleOrder.last();  // Wrap to end
        }
        return -1;
    }
    
    // Normal sequential mode
    if (m_currentIndex <= 0) {
        if (m_repeatMode == RepeatMode::All && count > 0) {
            return count - 1;  // Wrap to end
        }
        return -1;
    }
    
    return m_currentIndex - 1;
}

void PlaybackQueue::generateShuffleOrder()
{
    m_shuffleOrder.clear();
    m_shufflePosition = -1;
    
    if (!m_playlist || m_playlist->rowCount() == 0) {
        return;
    }
    
    int count = m_playlist->rowCount();
    
    // Create ordered list
    for (int i = 0; i < count; i++) {
        m_shuffleOrder.append(i);
    }
    
    // Fisher-Yates shuffle
    for (int i = count - 1; i > 0; i--) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(m_shuffleOrder[i], m_shuffleOrder[j]);
    }
    
    // If we have a current track, move it to the front
    if (m_currentIndex >= 0) {
        int pos = m_shuffleOrder.indexOf(m_currentIndex);
        if (pos > 0) {
            m_shuffleOrder.move(pos, 0);
        }
        m_shufflePosition = 0;
    }
}

void PlaybackQueue::onPlaylistRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    
    // Adjust current index if items were inserted before it
    if (m_currentIndex >= first) {
        m_currentIndex += (last - first + 1);
        emit currentIndexChanged(m_currentIndex);
    }
    
    // Regenerate shuffle order if shuffle is enabled
    if (m_shuffle) {
        generateShuffleOrder();
    }
    
    emit queueModified();
}

void PlaybackQueue::onPlaylistRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    
    int removedCount = last - first + 1;
    
    // Adjust current index
    if (m_currentIndex >= first && m_currentIndex <= last) {
        // Current track was removed - don't change index, let it play out
        // PlayerController will handle what happens when track finishes
    } else if (m_currentIndex > last) {
        m_currentIndex -= removedCount;
        emit currentIndexChanged(m_currentIndex);
    }
    
    // Regenerate shuffle order if shuffle is enabled
    if (m_shuffle) {
        generateShuffleOrder();
    }
    
    emit queueModified();
}

void PlaybackQueue::onPlaylistRowsMoved(const QModelIndex& parent, int start, int end,
                                        const QModelIndex& destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(destination)
    
    int movedCount = end - start + 1;
    
    // Adjust current index based on move
    if (m_currentIndex >= start && m_currentIndex <= end) {
        // Current track was moved
        int offset = m_currentIndex - start;
        if (row > end) {
            m_currentIndex = row - movedCount + offset;
        } else {
            m_currentIndex = row + offset;
        }
        emit currentIndexChanged(m_currentIndex);
    } else if (m_currentIndex > end && m_currentIndex < row) {
        // Current track is between old and new position (moving down)
        m_currentIndex -= movedCount;
        emit currentIndexChanged(m_currentIndex);
    } else if (m_currentIndex >= row && m_currentIndex < start) {
        // Current track is between new and old position (moving up)
        m_currentIndex += movedCount;
        emit currentIndexChanged(m_currentIndex);
    }
    
    // Regenerate shuffle order if shuffle is enabled
    if (m_shuffle) {
        generateShuffleOrder();
    }
    
    emit queueModified();
}

void PlaybackQueue::onPlaylistReset()
{
    m_currentIndex = -1;
    m_shuffleOrder.clear();
    m_shufflePosition = -1;
    
    if (m_shuffle && m_playlist && m_playlist->rowCount() > 0) {
        generateShuffleOrder();
    }
    
    emit currentIndexChanged(m_currentIndex);
    emit queueModified();
}
