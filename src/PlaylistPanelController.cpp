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
    m_selectionAnchorRow = -1;
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
    
    if (shift && m_selectionAnchorRow >= 0) {
        int start = qMin(m_selectionAnchorRow, row);
        int end = qMax(m_selectionAnchorRow, row);
        
        QItemSelection selection;
        selection.select(m_model->index(start, 0), m_model->index(end, 0));
        
        if (ctrl) {
            m_selectionModel->select(selection, QItemSelectionModel::Select);
        } else {
            m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
        }
        m_lastClickedRow = row;
    } else if (ctrl) {
        m_selectionModel->select(idx, QItemSelectionModel::Toggle);
        m_lastClickedRow = row;
        m_selectionAnchorRow = row;
    } else {
        m_selectionModel->select(idx, QItemSelectionModel::ClearAndSelect);
        m_lastClickedRow = row;
        m_selectionAnchorRow = row;
    }
}

void PlaylistPanelController::clearSelection()
{
    m_selectionModel->clearSelection();
    m_lastClickedRow = -1;
    m_selectionAnchorRow = -1;
}

void PlaylistPanelController::selectAll()
{
    if (!m_model || m_model->rowCount() == 0)
        return;
    
    QItemSelection selection;
    selection.select(m_model->index(0, 0), m_model->index(m_model->rowCount() - 1, 0));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
    m_lastClickedRow = 0;
    m_selectionAnchorRow = 0;
}

void PlaylistPanelController::selectRange(int fromRow, int toRow)
{
    if (!m_model || m_model->rowCount() == 0)
        return;
    
    int start = qBound(0, qMin(fromRow, toRow), m_model->rowCount() - 1);
    int end = qBound(0, qMax(fromRow, toRow), m_model->rowCount() - 1);
    
    QItemSelection selection;
    selection.select(m_model->index(start, 0), m_model->index(end, 0));
    m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
    m_lastClickedRow = end;
    m_selectionAnchorRow = start;
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

int PlaylistPanelController::keyboardMoveSelection(int delta, bool extendSelection)
{
    if (!m_model || m_model->rowCount() == 0 || delta == 0)
        return -1;

    const int rowCount = m_model->rowCount();
    int baseRow = m_lastClickedRow;
    if (baseRow < 0) {
        if (!m_selectedRowsCache.isEmpty())
            baseRow = delta > 0 ? m_selectedRowsCache.constLast() : m_selectedRowsCache.constFirst();
        else
            baseRow = delta > 0 ? -1 : rowCount;
    }

    const int targetRow = qBound(0, baseRow + delta, rowCount - 1);
    const QModelIndex targetIndex = m_model->index(targetRow, 0);

    if (extendSelection) {
        const int anchorRow = m_selectionAnchorRow >= 0 ? m_selectionAnchorRow : targetRow;
        QItemSelection selection;
        selection.select(m_model->index(qMin(anchorRow, targetRow), 0),
                         m_model->index(qMax(anchorRow, targetRow), 0));
        m_selectionModel->select(selection, QItemSelectionModel::ClearAndSelect);
        m_lastClickedRow = targetRow;
        if (m_selectionAnchorRow < 0)
            m_selectionAnchorRow = anchorRow;
    } else {
        m_selectionModel->select(targetIndex, QItemSelectionModel::ClearAndSelect);
        m_lastClickedRow = targetRow;
        m_selectionAnchorRow = targetRow;
    }

    return targetRow;
}

void PlaylistPanelController::removeSelected()
{
    if (!m_model || m_selectedRowsCache.isEmpty())
        return;

    const int focusRow = m_lastClickedRow >= 0 ? m_lastClickedRow : m_selectedRowsCache.constFirst();
    
    QList<int> rows = m_selectedRowsCache;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    
    for (int row : rows) {
        m_model->removeTrack(row);
    }

    if (m_model->rowCount() <= 0) {
        clearSelection();
        return;
    }

    const int targetRow = qMin(focusRow, m_model->rowCount() - 1);
    const QModelIndex targetIndex = m_model->index(targetRow, 0);
    m_selectionModel->select(targetIndex, QItemSelectionModel::ClearAndSelect);
    m_lastClickedRow = targetRow;
    m_selectionAnchorRow = targetRow;
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
    m_lastClickedRow = insertPos;
    m_selectionAnchorRow = insertPos;
}

void PlaylistPanelController::sortByColumn(const QString &key, bool ascending)
{
    if (!m_model || key.trimmed().isEmpty())
        return;

    clearSelection();
    m_model->sortByColumn(key, ascending);
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
