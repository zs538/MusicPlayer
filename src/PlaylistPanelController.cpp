#include "PlaylistPanelController.h"
#include "TrackListModel.h"
#include <algorithm>

PlaylistPanelController::PlaylistPanelController(QObject *parent)
    : QObject(parent)
    , m_selectionModel(new QItemSelectionModel(nullptr, this))
{
    connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, [this]() {
        updateSelectionCache();
        m_selectionGeneration++;
        emit selectionChanged();
    });
}

QAbstractItemModel *PlaylistPanelController::model() const
{
    return m_model;
}

void PlaylistPanelController::setModel(QAbstractItemModel *m)
{
    TrackListModel *trackModel = qobject_cast<TrackListModel*>(m);
    if (m_model == trackModel)
        return;
    
    m_model = trackModel;
    m_selectionModel->setModel(m_model);
    m_lastClickedRow = -1;
    updateSelectionCache();
    m_selectionGeneration++;
    
    emit modelChanged();
    emit selectionChanged();
}

int PlaylistPanelController::selectionGeneration() const
{
    return m_selectionGeneration;
}

int PlaylistPanelController::selectedCount() const
{
    return m_selectedRowsCache.size();
}

qint64 PlaylistPanelController::selectedDurationMs() const
{
    return m_selectedDurationMs;
}

void PlaylistPanelController::clickRow(int row, bool ctrl, bool shift)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;
    
    QModelIndex idx = m_model->index(row, 0);
    
    if (shift && m_lastClickedRow >= 0) {
        // Range selection from last clicked to current
        int start = qMin(m_lastClickedRow, row);
        int end = qMax(m_lastClickedRow, row);
        
        QItemSelection selection;
        selection.select(m_model->index(start, 0), m_model->index(end, 0));
        
        if (ctrl) {
            // Add to existing selection
            m_selectionModel->select(selection, QItemSelectionModel::Select);
        } else {
            // Replace selection with range
            m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
        }
    } else if (ctrl) {
        // Toggle single item
        m_selectionModel->select(idx, QItemSelectionModel::Toggle);
        m_lastClickedRow = row;
    } else {
        // Single select (clear others)
        m_selectionModel->select(idx, QItemSelectionModel::ClearAndSelect);
        m_lastClickedRow = row;
    }
}

void PlaylistPanelController::clearSelection()
{
    m_selectionModel->clearSelection();
    m_lastClickedRow = -1;
}

void PlaylistPanelController::selectAll()
{
    if (!m_model || m_model->rowCount() == 0)
        return;
    
    QItemSelection selection;
    selection.select(m_model->index(0, 0), m_model->index(m_model->rowCount() - 1, 0));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
}

bool PlaylistPanelController::isRowSelected(int row) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return false;
    
    return m_selectionModel->isSelected(m_model->index(row, 0));
}

QVariantList PlaylistPanelController::selectedRows() const
{
    QVariantList result;
    for (int row : m_selectedRowsCache) {
        result.append(row);
    }
    return result;
}

void PlaylistPanelController::removeSelected()
{
    if (!m_model || m_selectedRowsCache.isEmpty())
        return;
    
    // Remove from highest index to lowest to preserve indices
    QList<int> rows = m_selectedRowsCache;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    
    for (int row : rows) {
        m_model->removeTrack(row);
    }
    
    clearSelection();
}

void PlaylistPanelController::moveSelectedTo(int targetRow)
{
    if (!m_model || m_selectedRowsCache.isEmpty())
        return;
    
    // Get sorted selected rows
    QList<int> rows = m_selectedRowsCache;
    std::sort(rows.begin(), rows.end());
    
    // Calculate effective target after moves
    int insertPos = targetRow;
    
    // Collect tracks to move
    QVector<TrackInfo> tracksToMove;
    for (int row : rows) {
        tracksToMove.append(m_model->trackAt(row));
    }
    
    // Remove from highest to lowest
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row < insertPos) {
            insertPos--;
        }
        m_model->removeTrack(row);
    }
    
    // Insert at target position
    for (int i = 0; i < tracksToMove.size(); ++i) {
        m_model->insertTrack(insertPos + i, tracksToMove[i]);
    }
    
    // Select the moved rows
    clearSelection();
    QItemSelection selection;
    selection.select(m_model->index(insertPos, 0), 
                     m_model->index(insertPos + tracksToMove.size() - 1, 0));
    m_selectionModel->select(selection, QItemSelectionModel::Select);
}

void PlaylistPanelController::updateSelectionCache()
{
    m_selectedRowsCache.clear();
    m_selectedDurationMs = 0;
    
    if (!m_model)
        return;
    
    QModelIndexList selected = m_selectionModel->selectedIndexes();
    for (const QModelIndex &idx : selected) {
        int row = idx.row();
        m_selectedRowsCache.append(row);
        TrackInfo track = m_model->trackAt(row);
        m_selectedDurationMs += track.durationMs;
    }
    
    std::sort(m_selectedRowsCache.begin(), m_selectedRowsCache.end());
}
