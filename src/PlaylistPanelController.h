#ifndef PLAYLISTPANELCONTROLLER_H
#define PLAYLISTPANELCONTROLLER_H

#include <QObject>
#include <QQmlEngine>
#include <QItemSelectionModel>
#include <QVariantList>

class QAbstractItemModel;
class TrackListModel;

/**
 * @brief PlaylistPanelController owns selection state for a playlist panel.
 * 
 * This is a QML element (instantiable per panel) that wraps QItemSelectionModel
 * and provides selection-related operations. QML should never store selection
 * state in JS Sets/arrays - it should use this controller.
 */
class PlaylistPanelController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int selectionGeneration READ selectionGeneration NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(qint64 selectedDurationMs READ selectedDurationMs NOTIFY selectionChanged)

public:
    explicit PlaylistPanelController(QObject *parent = nullptr);

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *m);

    int selectionGeneration() const;
    int selectedCount() const;
    qint64 selectedDurationMs() const;

    // QML calls these; C++ implements all policy
    Q_INVOKABLE void clickRow(int row, bool ctrl, bool shift);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectRange(int fromRow, int toRow);
    Q_INVOKABLE bool isRowSelected(int row) const;
    Q_INVOKABLE QVariantList selectedRows() const; // sorted
    Q_INVOKABLE int keyboardMoveSelection(int delta, bool extendSelection);

    // High-level actions (QML never calls model.removeRows directly)
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void moveSelectedTo(int targetRow);
    Q_INVOKABLE void sortByColumn(const QString &key, bool ascending = true);

signals:
    void modelChanged();
    void selectionChanged();

private:
    void updateSelectionCache();
    
    TrackListModel *m_model = nullptr;
    QItemSelectionModel *m_selectionModel = nullptr;
    int m_selectionGeneration = 0;
    int m_lastClickedRow = -1;
    int m_selectionAnchorRow = -1;
    
    // Cached selection data
    QList<int> m_selectedRowsCache;
    qint64 m_selectedDurationMs = 0;
};

#endif // PLAYLISTPANELCONTROLLER_H
