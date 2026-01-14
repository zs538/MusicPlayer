#ifndef QUEUEMANAGER_H
#define QUEUEMANAGER_H

#include <QObject>
#include "TrackListModel.h"

class QueueManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

public:
    explicit QueueManager(QObject *parent = nullptr);
    
    void setPlaylistModel(TrackListModel *model);
    TrackListModel *playlistModel() const { return m_model; }
    
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    
    TrackInfo currentTrack() const;
    TrackInfo peekNextTrack() const;
    TrackInfo peekPreviousTrack() const;
    
    bool canAdvance() const;
    bool canRetreat() const;
    
    void advance();
    void retreat();
    void reset();

signals:
    void currentIndexChanged(int index);
    void currentTrackChanged(const TrackInfo &track);

private slots:
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void onRowsRemoved(const QModelIndex &parent, int first, int last);
    void onRowsMoved(const QModelIndex &parent, int start, int end, 
                     const QModelIndex &destination, int row);
    void onModelReset();

private:
    void updateCurrentIndex(int newIndex);
    void findCurrentTrackByPath();
    
    TrackListModel *m_model = nullptr;
    int m_currentIndex = -1;
    QString m_currentTrackPath;  // Track by path to survive reordering
};

#endif // QUEUEMANAGER_H
