pragma Singleton
import QtQuick

QtObject {
    id: dragManager
    
    // Current drag state
    property bool isDragging: false
    property var draggedIndices: []  // Array of indices being dragged
    property var draggedItems: []    // Array of item data being dragged
    property string sourceId: ""     // Identifier of the source (e.g., playlist UUID)
    property int dropTargetIndex: -1 // Where items will be dropped
    property string dropTargetId: "" // Target section identifier
    
    // Start a drag operation
    function startDrag(indices, items, source) {
        draggedIndices = indices.slice().sort((a, b) => a - b)
        draggedItems = items.slice()
        sourceId = source
        isDragging = true
        dropTargetIndex = -1
        dropTargetId = ""
    }
    
    // Update drop target as user hovers
    function setDropTarget(targetId, index) {
        dropTargetId = targetId
        dropTargetIndex = index
    }
    
    // Clear drop target
    function clearDropTarget() {
        dropTargetIndex = -1
        dropTargetId = ""
    }
    
    // End drag and return drop info, then reset
    function endDrag() {
        var result = {
            indices: draggedIndices.slice(),
            items: draggedItems.slice(),
            sourceId: sourceId,
            targetId: dropTargetId,
            targetIndex: dropTargetIndex,
            valid: isDragging && dropTargetIndex >= 0
        }
        
        // Reset state
        isDragging = false
        draggedIndices = []
        draggedItems = []
        sourceId = ""
        dropTargetIndex = -1
        dropTargetId = ""
        
        return result
    }
    
    // Cancel drag without dropping
    function cancelDrag() {
        isDragging = false
        draggedIndices = []
        draggedItems = []
        sourceId = ""
        dropTargetIndex = -1
        dropTargetId = ""
    }
    
    // Check if an index is being dragged
    function isBeingDragged(index) {
        return isDragging && draggedIndices.indexOf(index) >= 0
    }
}
