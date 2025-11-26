#pragma once

#include <QObject>
#include "PlaybackEngine.h"

class PlaylistModel;

/**
 * @brief Repeat mode for playlist playback.
 */
enum class RepeatMode {
    Off,        ///< No repeat - stop at end of playlist
    One,        ///< Repeat current track
    All         ///< Repeat entire playlist
};
Q_DECLARE_METATYPE(RepeatMode)

/**
 * @brief Manages the playback queue and decides what track to play next.
 * 
 * This class owns the logical play order and integrates with PlaylistModel.
 * It knows about:
 * - The active playlist (one at a time)
 * - Current index within the playlist
 * - Repeat/shuffle modes
 * 
 * PlaybackQueue does NOT directly control playback - that's PlayerController's job.
 * It only provides information about what track is current and what comes next.
 */
class PlaybackQueue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)

public:
    explicit PlaybackQueue(QObject* parent = nullptr);
    ~PlaybackQueue() override;

    // Playlist management
    void setActivePlaylist(PlaylistModel* model);
    PlaylistModel* activePlaylist() const { return m_playlist; }

    // Current position
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    // Track access
    TrackRef current() const;
    TrackRef peekNext() const;
    TrackRef peekPrevious() const;

    // Navigation
    void advance();      ///< Move to next track based on repeat/shuffle
    void goBack();       ///< Move to previous track
    bool hasNext() const;
    bool hasPrevious() const;

    // Repeat/shuffle
    RepeatMode repeatMode() const { return m_repeatMode; }
    void setRepeatMode(RepeatMode mode);
    
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool enabled);

    // Queue size
    int count() const;
    bool isEmpty() const;

signals:
    void currentIndexChanged(int index);
    void repeatModeChanged(RepeatMode mode);
    void shuffleChanged(bool enabled);
    void playlistChanged();
    void queueModified();  ///< Emitted when playlist content changes

private slots:
    void onPlaylistRowsInserted(const QModelIndex& parent, int first, int last);
    void onPlaylistRowsRemoved(const QModelIndex& parent, int first, int last);
    void onPlaylistRowsMoved(const QModelIndex& parent, int start, int end,
                             const QModelIndex& destination, int row);
    void onPlaylistReset();

private:
    TrackRef trackRefAt(int index) const;
    int nextIndex() const;      ///< Calculate next index based on repeat/shuffle
    int previousIndex() const;  ///< Calculate previous index
    void generateShuffleOrder();

    PlaylistModel* m_playlist {nullptr};
    int m_currentIndex {-1};
    RepeatMode m_repeatMode {RepeatMode::Off};
    bool m_shuffle {false};
    
    // Shuffle state
    QVector<int> m_shuffleOrder;
    int m_shufflePosition {-1};
};
