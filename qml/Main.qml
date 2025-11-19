import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import Qt.labs.folderlistmodel 2.15
import Qt.labs.platform 1.1

ApplicationWindow {
    id: window
    visible: true
    width: 1200
    height: 800
    title: "Music Player"

    // Tracks the index of the currently playing item to handle duplicates
    property int currentIdx: -1
    property int browserMode: 0 // 0 = Collection, 1 = Files

    function indexOfUrl(u) {
        const s = u.toString()
        for (let i = 0; i < playlist.count(); ++i) {
            const it = playlist.get(i)
            if (it && it.url.toString() === s)
                return i
        }
        return -1
    }

    function indexOfUrlFrom(u, startAt) {
        const s = u.toString()
        // forward search from startAt
        for (let i = Math.max(0, startAt); i < playlist.count(); ++i) {
            const it = playlist.get(i)
            if (it && it.url.toString() === s) return i
        }
        // fallback: from 0 to startAt-1
        for (let j = 0; j < Math.min(startAt, playlist.count()); ++j) {
            const it2 = playlist.get(j)
            if (it2 && it2.url.toString() === s) return j
        }
        return -1
    }

    function armNextFromQueue() {
        const idx = currentIdx
        if (idx >= 0 && idx + 1 < playlist.count()) {
            const it = playlist.get(idx + 1)
            if (it) player.setNextFile(it.url)
        } else {
            player.setNextFile("")
        }
    }

    function playIndex(i) {
        if (i < 0 || i >= playlist.count()) return
        const it = playlist.get(i)
        if (!it) return
        const u = it.url
        player.openFile(u)
        currentIdx = i
        if (i + 1 < playlist.count()) {
            const nx = playlist.get(i + 1)
            if (nx) player.setNextFile(nx.url)
        } else {
            player.setNextFile("")
        }
    }

    function nextFromQueue() {
        // Stop if playlist is empty
        if (playlist.count() === 0) {
            player.stop()
            currentIdx = -1
            return
        }
        
        const idx = currentIdx
        if (idx === -1) {
            if (playlist.count() > 0) playIndex(0)
            return
        }
        if (idx + 1 < playlist.count()) {
            playIndex(idx + 1)
        } else {
            player.setNextFile("")
            const endPos = Math.max(0, player.duration - 1)
            player.seek(endPos)
            if (!player.playing) {
                player.play()
            }
        }
    }

    function prevFromQueue() {
        const idx = currentIdx
        if (idx > 0) playIndex(idx - 1)
    }

    function handlePlayToggle() {
        if (player.playing) {
            player.pause()
            return
        }
        const idx = currentIdx
        const finished = player.duration > 0 && player.position >= (player.duration - 5)
        if (idx === -1) {
            if (playlist.count() > 0) {
                playIndex(0)
            } else {
                player.play()
            }
        } else if (idx === playlist.count() - 1 && finished) {
            playIndex(0)
        } else {
            player.play()
        }
    }

    // Main layout structure
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top section with resizable left/right panes
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Section: Playlist/Queue
            Rectangle {
                id: leftSection
                Layout.preferredWidth: parent.width * 0.4
                Layout.minimumWidth: 200
                Layout.maximumWidth: parent.width * 0.7
                Layout.fillHeight: true
                color: "#1a1a1a"
                border.color: "#333"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    // Playlist tabs area
                    TabBar {
                        id: playlistTabBar
                        Layout.fillWidth: true
                        height: 40
                        
                        TabButton {
                            text: "Queue"
                            width: implicitWidth
                        }
                        
                        TabButton {
                            text: "Playlist 1"
                            width: implicitWidth
                        }
                        
                        TabButton {
                            text: "+"
                            width: 30
                            font.pixelSize: 16
                        }
                    }

                    // Playlist content area
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: playlistTabBar.currentIndex

                        // Queue view - Simple ListView with full-width rows
                        Rectangle {
                            color: "#1a1a1a"
                            
                            // Column headers
                            Rectangle {
                                id: header
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 30
                                color: "#252525"
                                border.color: "#333"
                                border.width: 1
                                z: 10
                                
                                Row {
                                    anchors.fill: parent
                                    
                                    Rectangle {
                                        width: parent.width * 0.08
                                        height: 30
                                        color: "#252525"
                                        border.color: "#333"
                                        border.width: 1
                                        
                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 4
                                            text: "#"
                                            color: "#ccc"
                                            font.pixelSize: 12
                                            font.bold: true
                                            horizontalAlignment: Qt.AlignRight
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    
                                    Rectangle {
                                        width: parent.width * 0.48
                                        height: 30
                                        color: "#252525"
                                        border.color: "#333"
                                        border.width: 1
                                        
                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 4
                                            text: "Title"
                                            color: "#ccc"
                                            font.pixelSize: 12
                                            font.bold: true
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    
                                    Rectangle {
                                        width: parent.width * 0.32
                                        height: 30
                                        color: "#252525"
                                        border.color: "#333"
                                        border.width: 1
                                        
                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 4
                                            text: "Artist"
                                            color: "#ccc"
                                            font.pixelSize: 12
                                            font.bold: true
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    
                                    Rectangle {
                                        width: parent.width * 0.12
                                        height: 30
                                        color: "#252525"
                                        border.color: "#333"
                                        border.width: 1
                                        
                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 4
                                            text: "Year"
                                            color: "#ccc"
                                            font.pixelSize: 12
                                            font.bold: true
                                            horizontalAlignment: Qt.AlignCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                            
                            // Selection overlay - OUTSIDE ScrollView to avoid breaking layout
                            Rectangle {
                                id: selectionRect
                                visible: false
                                color: "#60a5fa"
                                border.color: "#3b82f6"
                                border.width: 2
                                opacity: 0.3
                                z: 1000
                                
                                function updateRect(startX, startY, endX, endY) {
                                    x = Math.min(startX, endX)
                                    y = Math.min(startY, endY) + header.height  // Account for header offset
                                    width = Math.abs(endX - startX)
                                    height = Math.abs(endY - startY)
                                }
                            }
                            
                            MouseArea {
                                id: areaSelectionMouseArea
                                anchors.top: header.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                acceptedButtons: Qt.LeftButton
                                z: 999
                                
                                property bool selecting: false
                                property real selectionStartX
                                property real selectionStartY
                                
                                onPressed: function(mouse) {
                                    // Map coordinates to ListView content space
                                    const scrollViewCoords = mapToItem(scrollView, mouse.x, mouse.y)
                                    const listViewCoords = scrollView.mapToItem(listView, scrollViewCoords.x, scrollViewCoords.y)
                                    
                                    // Check if clicking on a row (account for scroll offset)
                                    const clickedItem = listView.itemAt(listViewCoords.x, listViewCoords.y)
                                    if (clickedItem) {
                                        // Let row handle the click
                                        mouse.accepted = false
                                        return
                                    }
                                    
                                    // Start area selection on empty space
                                    selecting = true
                                    selectionStartX = mouse.x
                                    selectionStartY = mouse.y
                                    selectionRect.visible = true
                                    selectionRect.updateRect(mouse.x, mouse.y, mouse.x, mouse.y)
                                }
                                
                                onPositionChanged: function(mouse) {
                                    if (selecting) {
                                        selectionRect.updateRect(selectionStartX, selectionStartY, mouse.x, mouse.y)
                                        
                                        // Real-time selection during drag
                                        const dragDistance = Math.sqrt(
                                            Math.pow(mouse.x - selectionStartX, 2) + 
                                            Math.pow(mouse.y - selectionStartY, 2)
                                        )
                                        
                                        if (dragDistance > 5) {
                                            // Map mouse coordinates to ListView for selection (avoid double offset)
                                            const mouseTop = mapToItem(scrollView, selectionStartX, selectionStartY)
                                            const mouseBottom = mapToItem(scrollView, mouse.x, mouse.y)
                                            const listViewTop = scrollView.mapToItem(listView, mouseTop.x, mouseTop.y)
                                            const listViewBottom = scrollView.mapToItem(listView, mouseBottom.x, mouseBottom.y)
                                            
                                            listView.selectArea(listViewTop.y, listViewBottom.y)
                                        }
                                    }
                                }
                                
                                onReleased: function(mouse) {
                                    if (selecting) {
                                        selecting = false
                                        selectionRect.visible = false
                                    }
                                }
                            }
                            
                            ScrollView {
                                id: scrollView
                                anchors.top: header.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                clip: true
                                
                                // NO DRAG SCROLLING - only scrollbar and wheel
                                contentWidth: availableWidth
                                
                                ListView {
                                    id: listView
                                    model: playlist
                                    
                                    // ABSOLUTELY NO DRAG SCROLLING
                                    interactive: false
                                    
                                    // Track selection state
                                    property var selectedRows: []
                                    property int lastSelectedIndex: -1
                                    property int shiftAnchorIndex: -1  // Most recently clicked row for shift selection
                                    
                                    // Drag state
                                    property bool isDragging: false
                                    property int dropTargetIndex: -1  // Index of row to drop AFTER
                                    
                                    signal selectionChanged()
                                    
                                    onSelectedRowsChanged: selectionChanged()
                                    
                                    function clearSelection() {
                                        selectedRows = []
                                        lastSelectedIndex = -1
                                        // Don't reset shiftAnchorIndex here - it should persist for shift-clicks
                                        for (let i = 0; i < count; i++) {
                                            const item = itemAtIndex(i)
                                            if (item) item.isSelected = false
                                        }
                                        // Force UI update
                                        selectedRowsChanged()
                                    }
                                    
                                    function selectRow(index, toggle, extend, additive = false) {
                                        if (toggle) {
                                            // Ctrl+click: toggle selection
                                            const pos = selectedRows.indexOf(index)
                                            if (pos >= 0) {
                                                selectedRows.splice(pos, 1)
                                                const item = itemAtIndex(index)
                                                if (item) item.isSelected = false
                                            } else {
                                                selectedRows.push(index)
                                                const item = itemAtIndex(index)
                                                if (item) item.isSelected = true
                                            }
                                            lastSelectedIndex = index
                                            shiftAnchorIndex = index  // Update shift anchor to most recent click
                                        } else if (extend) {
                                            // Shift+click: extend selection from most recently clicked row
                                            if (additive) {
                                                // Ctrl+Shift+click: add range to existing selection
                                                if (shiftAnchorIndex < 0) {
                                                    // No shift anchor, just add this row like Ctrl+click
                                                    if (selectedRows.indexOf(index) < 0) {
                                                        selectedRows.push(index)
                                                        const item = itemAtIndex(index)
                                                        if (item) item.isSelected = true
                                                    }
                                                } else {
                                                    // Add range to existing selection
                                                    const start = Math.min(shiftAnchorIndex, index)
                                                    const end = Math.max(shiftAnchorIndex, index)
                                                    for (let i = start; i <= end; i++) {
                                                        if (selectedRows.indexOf(i) < 0) {
                                                            selectedRows.push(i)
                                                            const item = itemAtIndex(i)
                                                            if (item) item.isSelected = true
                                                        }
                                                    }
                                                }
                                                lastSelectedIndex = index
                                                shiftAnchorIndex = index  // Update anchor like Ctrl+click
                                                selectedRowsChanged()  // Force UI update for additive selection
                                            } else {
                                                // Normal Shift+click: replace selection with range
                                                if (shiftAnchorIndex < 0) {
                                                    // No shift anchor, just select this row
                                                    clearSelection()
                                                    selectedRows = [index]
                                                    const item = itemAtIndex(index)
                                                    if (item) item.isSelected = true
                                                    lastSelectedIndex = index
                                                    shiftAnchorIndex = index
                                                } else {
                                                    clearSelection()
                                                    const start = Math.min(shiftAnchorIndex, index)
                                                    const end = Math.max(shiftAnchorIndex, index)
                                                    for (let i = start; i <= end; i++) {
                                                        selectedRows.push(i)
                                                        const item = itemAtIndex(i)
                                                        if (item) item.isSelected = true
                                                    }
                                                    lastSelectedIndex = index
                                                    // Don't update shiftAnchorIndex on shift-click to maintain anchor
                                                }
                                            }
                                        } else {
                                            // Normal click: select single row
                                            clearSelection()
                                            selectedRows = [index]
                                            const item = itemAtIndex(index)
                                            if (item) item.isSelected = true
                                            lastSelectedIndex = index
                                            shiftAnchorIndex = index
                                        }
                                        
                                        // Force UI update
                                        selectedRowsChanged()
                                    }
                                    
                                    function selectArea(startY, endY) {
                                        clearSelection()
                                        const start = Math.min(startY, endY)
                                        const end = Math.max(startY, endY)
                                        
                                        for (let i = 0; i < count; i++) {
                                            const item = itemAtIndex(i)
                                            if (item) {
                                                const itemY = item.y
                                                const itemBottom = itemY + item.height
                                                
                                                if ((itemY >= start && itemY <= end) || 
                                                    (itemBottom >= start && itemBottom <= end) ||
                                                    (itemY <= start && itemBottom >= end)) {
                                                    selectedRows.push(i)
                                                    item.isSelected = true
                                                }
                                            }
                                        }
                                        
                                        if (selectedRows.length > 0) {
                                            lastSelectedIndex = selectedRows[selectedRows.length - 1]
                                            shiftAnchorIndex = lastSelectedIndex
                                        }
                                        
                                        // Force UI update
                                        selectedRowsChanged()
                                    }
                                    
                                    function moveSelectedRows(beforeIndex) {
                                        if (selectedRows.length === 0) return
                                        
                                        console.log("=== moveSelectedRows DEBUG (USER'S EXACT ALGORITHM) ===")
                                        console.log("beforeIndex:", beforeIndex, "count:", count)
                                        console.log("selectedRows:", selectedRows)
                                        
                                        // Check if currently playing row is being moved
                                        const playingRowMoved = selectedRows.includes(currentIdx)
                                        const originalCurrentIdx = currentIdx
                                        
                                        // Sort selected rows to maintain order
                                        const rowsToMove = [...selectedRows].sort((a, b) => a - b)
                                        console.log("rowsToMove sorted:", rowsToMove)
                                        
                                        // STEP 1: Calculate final order array [unmoved_before, moved_block, unmoved_after]
                                        let finalOrder = []
                                        let movedBlock = []
                                        
                                        // Add unmoved items before target position
                                        for (let i = 0; i < count; i++) {
                                            if (!selectedRows.includes(i) && i < beforeIndex) {
                                                finalOrder.push(i)
                                            }
                                        }
                                        
                                        // Add moved block
                                        for (const row of rowsToMove) {
                                            movedBlock.push(row)
                                        }
                                        finalOrder.push(...movedBlock)
                                        
                                        // Add unmoved items after target position
                                        for (let i = 0; i < count; i++) {
                                            if (!selectedRows.includes(i) && i >= beforeIndex) {
                                                finalOrder.push(i)
                                            }
                                        }
                                        
                                        console.log("Final order calculated:", finalOrder)
                                        
                                        // STEP 2: Apply final order by tracking current positions
                                        console.log("STEP 2: Applying final order with position tracking")
                                        
                                        // Track where each original index currently is
                                        let currentPositions = []
                                        for (let i = 0; i < count; i++) {
                                            currentPositions[i] = i
                                        }
                                        
                                        // Apply moves from bottom to top to minimize conflicts
                                        for (let pos = finalOrder.length - 1; pos >= 0; pos--) {
                                            const originalIdx = finalOrder[pos]
                                            const currentPos = currentPositions[originalIdx]
                                            
                                            if (currentPos !== pos) {
                                                console.log("Move: playlist.moveRowTo(", currentPos, ",", pos, ") [original:", originalIdx, "]")
                                                playlist.moveRowTo(currentPos, pos)
                                                
                                                // Update position tracking for all affected items
                                                const movedItem = originalIdx
                                                const fromPos = currentPos
                                                const toPos = pos
                                                
                                                if (fromPos < toPos) {
                                                    // Item moved right, shift items between left
                                                    for (let i = 0; i < currentPositions.length; i++) {
                                                        if (currentPositions[i] >= fromPos && currentPositions[i] <= toPos) {
                                                            if (i !== movedItem) {
                                                                currentPositions[i]--
                                                            }
                                                        }
                                                    }
                                                } else {
                                                    // Item moved left, shift items between right
                                                    for (let i = 0; i < currentPositions.length; i++) {
                                                        if (currentPositions[i] >= toPos && currentPositions[i] <= fromPos) {
                                                            if (i !== movedItem) {
                                                                currentPositions[i]++
                                                            }
                                                        }
                                                    }
                                                }
                                                currentPositions[movedItem] = toPos
                                            }
                                        }
                                        
                                        // Update currentIdx if needed
                                        if (playingRowMoved) {
                                            // Find new position of the moved playing row using final order
                                            const newCurrentIdx = finalOrder.indexOf(originalCurrentIdx)
                                            currentIdx = newCurrentIdx
                                            
                                            // Reload armed track since playing row moved
                                            armNextFromQueue()
                                            console.log("Playing row moved from", originalCurrentIdx, "to", newCurrentIdx, "- reloaded next track")
                                        }
                                        
                                        // Clear selection after move
                                        clearSelection()
                                    }
                                    
                                    function updateDropPosition(mouseY) {
                                        if (!isDragging) return
                                        
                                        // Find which row the mouse is over
                                        let targetIndex = -1
                                        let belowAllRows = true
                                        
                                        for (let i = 0; i < count; i++) {
                                            const item = itemAtIndex(i)
                                            if (item) {
                                                const itemTop = item.y
                                                const itemBottom = item.y + item.height
                                                
                                                if (mouseY >= itemTop && mouseY <= itemBottom) {
                                                    targetIndex = i
                                                    belowAllRows = false
                                                    break
                                                }
                                                targetIndex = i  // Default to last row if past all items
                                            }
                                        }
                                        
                                        // Special case: dropping below all rows (at bottom)
                                        if (belowAllRows) {
                                            // Place after last row
                                            dropTargetIndex = count
                                            return
                                        }
                                        
                                        // Normal case: place BEFORE target row
                                        // Don't allow dropping before a selected row
                                        if (targetIndex >= 0 && selectedRows.includes(targetIndex)) {
                                            // Find the next non-selected row after this
                                            let nextNonSelected = -1
                                            for (let i = targetIndex + 1; i < count; i++) {
                                                if (!selectedRows.includes(i)) {
                                                    nextNonSelected = i
                                                    break
                                                }
                                            }
                                            // If no non-selected row found (all remaining rows are selected), drop at end
                                            if (nextNonSelected === -1) {
                                                dropTargetIndex = count
                                            } else {
                                                dropTargetIndex = nextNonSelected
                                            }
                                        } else {
                                            dropTargetIndex = targetIndex
                                        }
                                    }
                                    
                                    // Visual drop indicator
                                    Rectangle {
                                        id: dropIndicator
                                        parent: listView
                                        width: listView.width
                                        height: 2
                                        color: "#3b82f6"
                                        visible: listView.isDragging && listView.dropTargetIndex >= 0
                                        z: 1000
                                        
                                        function updatePosition() {
                                            if (!listView.isDragging || listView.dropTargetIndex < 0) {
                                                visible = false
                                                return
                                            }
                                            
                                            visible = true
                                            
                                            if (listView.dropTargetIndex >= listView.count) {
                                                // Drop at end - show after last row
                                                if (listView.count > 0) {
                                                    const lastItem = listView.itemAtIndex(listView.count - 1)
                                                    y = lastItem.y + lastItem.height
                                                } else {
                                                    y = 0
                                                }
                                            } else if (listView.dropTargetIndex < 0) {
                                                // Drop at beginning - show at top
                                                y = 0
                                            } else {
                                                // Drop before target row - show at target row's top
                                                const targetItem = listView.itemAtIndex(listView.dropTargetIndex)
                                                if (targetItem) {
                                                    y = targetItem.y
                                                }
                                            }
                                        }
                                        
                                        onVisibleChanged: updatePosition()
                                        Component.onCompleted: updatePosition()
                                    }
                                    
                                    onDropTargetIndexChanged: dropIndicator.updatePosition()
                                    
                                    delegate: Rectangle {
                                        id: rowDelegate
                                        width: listView.width
                                        height: 24
                                        
                                        property bool isSelected: false
                                        property bool isCurrent: index === currentIdx
                
                                        color: {
                                            if (isSelected) return "#4a5568"
                                            if (isCurrent) return "#334155"
                                            return index % 2 === 0 ? "#1f2937" : "#111827"
                                        }
                                        
                                        Row {
                                            anchors.fill: parent
                                            
                                            // Track # column
                                            Rectangle {
                                                width: parent.width * 0.08
                                                height: 24
                                                color: "transparent"
                                                
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 4
                                                    anchors.rightMargin: 4
                                                    text: trackNumber || ""
                                                    color: "#e5e7eb"
                                                    font.pixelSize: 11
                                                    horizontalAlignment: Qt.AlignRight
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            
                                            // Title column
                                            Rectangle {
                                                width: parent.width * 0.48
                                                height: 24
                                                color: "transparent"
                                                
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 4
                                                    anchors.rightMargin: 4
                                                    text: title || display || ""
                                                    color: "#e5e7eb"
                                                    font.pixelSize: 11
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            
                                            // Artist column
                                            Rectangle {
                                                width: parent.width * 0.32
                                                height: 24
                                                color: "transparent"
                                                
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 4
                                                    anchors.rightMargin: 4
                                                    text: artist || ""
                                                    color: "#e5e7eb"
                                                    font.pixelSize: 11
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                            }
                                            
                                            // Year column
                                            Rectangle {
                                                width: parent.width * 0.12
                                                height: 24
                                                color: "transparent"
                                                
                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 4
                                                    anchors.rightMargin: 4
                                                    text: year || ""
                                                    color: "#e5e7eb"
                                                    font.pixelSize: 11
                                                    horizontalAlignment: Qt.AlignCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            acceptedButtons: Qt.LeftButton
                                            
                                            property bool isDragging: false
                                            property real pressY
                                            property real dragThreshold: 10
                                            property bool selectionPreserved: false
                                            
                                            onPressed: function(mouse) {
                                                // Handle selection clicks
                                                if ((mouse.modifiers & Qt.ControlModifier) && (mouse.modifiers & Qt.ShiftModifier)) {
                                                    // Ctrl+Shift+click: add range to existing selection
                                                    listView.selectRow(index, false, true, true)
                                                } else if (mouse.modifiers & Qt.ControlModifier) {
                                                    listView.selectRow(index, true, false)
                                                } else if (mouse.modifiers & Qt.ShiftModifier) {
                                                    if (listView.shiftAnchorIndex >= 0) {
                                                        listView.selectRow(index, false, true)
                                                    } else {
                                                        listView.selectRow(index, false, false)
                                                    }
                                                } else {
                                                    // Normal click - but preserve selection if clicking already selected row (for drag)
                                                    if (listView.selectedRows.includes(index)) {
                                                        // Don't clear selection - allow dragging multiple rows
                                                        selectionPreserved = true
                                                        console.log("Preserving multi-row selection for drag")
                                                    } else {
                                                        // Clear and select only this row
                                                        listView.selectRow(index, false, false)
                                                        selectionPreserved = false
                                                    }
                                                }
                                                
                                                // Initialize drag tracking
                                                isDragging = false
                                                pressY = mouse.y
                                            }
                                            
                                            onPositionChanged: function(mouse) {
                                                if (!listView.isDragging) {
                                                    // Check if drag should start
                                                    const dragDistance = Math.abs(mouse.y - pressY)
                                                    if (dragDistance > dragThreshold && listView.selectedRows.length > 0) {
                                                        // Start dragging
                                                        isDragging = true
                                                        listView.isDragging = true
                                                        parent.opacity = 0.7  // Visual feedback
                                                        
                                                        // Initialize drop position to current row
                                                        listView.dropTargetIndex = index
                                                    }
                                                }
                                                
                                                if (listView.isDragging) {
                                                    // Update drop position based on mouse Y in ListView coordinates
                                                    const listViewY = rowDelegate.y + mouse.y
                                                    listView.updateDropPosition(listViewY)
                                                }
                                            }
                                            
                                            onReleased: function(mouse) {
                                                if (listView.isDragging) {
                                                    // Move selected rows to drop position
                                                    listView.moveSelectedRows(listView.dropTargetIndex)
                                                    console.log("Moved rows after index:", listView.dropTargetIndex)
                                                } else {
                                                    // No drag occurred - handle click selection
                                                    const dragDistance = Math.abs(mouse.y - pressY)
                                                    if (dragDistance < dragThreshold && selectionPreserved) {
                                                        // Click without drag on selected row - clear other selections
                                                        listView.selectRow(index, false, false)
                                                        console.log("Click without drag - cleared other selections")
                                                    }
                                                }
                                                
                                                // Clean up drag state
                                                isDragging = false
                                                parent.opacity = 1.0
                                                listView.isDragging = false
                                                listView.dropTargetIndex = -1
                                                selectionPreserved = false
                                            }
                                            
                                            onDoubleClicked: {
                                                playIndex(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Other playlist tabs (placeholder)
                        Rectangle {
                            color: "#1a1a1a"
                            Text {
                                anchors.centerIn: parent
                                text: "Playlist content"
                                color: "#666"
                            }
                        }

                        // New playlist button content
                        Rectangle {
                            color: "#1a1a1a"
                            Text {
                                anchors.centerIn: parent
                                text: "Create new playlist"
                                color: "#666"
                            }
                        }
                    }

                    // Playlist control buttons
                    RowLayout {
                        Layout.fillWidth: true
                        height: 40
                        spacing: 8
                        
                        Button {
                            text: "Add"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                            onClicked: addDialog.open()
                        }
                        
                        Button {
                            text: "Remove"
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 30
                            enabled: listView.selectedRows.length > 0
                            onClicked: {
                                // Remove selected rows in reverse order to maintain indices
                                const sortedRows = listView.selectedRows.sort((a, b) => b - a)
                                for (let i = 0; i < sortedRows.length; i++) {
                                    playlist.removeAt(sortedRows[i])
                                    // Update currentIdx if needed
                                    if (currentIdx > sortedRows[i]) {
                                        currentIdx--
                                    } else if (currentIdx === sortedRows[i]) {
                                        currentIdx = -1
                                    }
                                }
                                
                                // Stop playback if playlist becomes empty
                                if (playlist.count() === 0) {
                                    player.stop()
                                    currentIdx = -1
                                }
                                
                                // Clear selection after removal
                                listView.clearSelection()
                            }
                        }
                        
                        Button {
                            text: "Save"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                        }
                        
                        Button {
                            text: "Export"
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 30
                            onClicked: exportDialog.open()
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: "Clear"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                            enabled: playlist.count() > 0
                            onClicked: {
                                playlist.clear()
                                currentIdx = -1
                            }
                        }
                    }
                }
            }

            // Resize handle
            Rectangle {
                width: 4
                Layout.fillHeight: true
                color: resizeHandleArea.pressed ? "#555" : "#333"
                MouseArea {
                    id: resizeHandleArea
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.minimumX: 200
                    drag.maximumX: window.width - 300
                    onPositionChanged: {
                        if (drag.active) {
                            leftSection.Layout.preferredWidth = parent.x
                        }
                    }
                }
            }

            // Right Section: Collection/Files Browser
            Rectangle {
                id: rightSection
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1a1a1a"
                border.color: "#333"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    // View mode toggle
                    RowLayout {
                        Layout.fillWidth: true
                        height: 40
                        spacing: 8
                        
                        Button {
                            text: "Collection"
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 30
                            checkable: true
                            checked: browserMode === 0
                            onClicked: browserMode = 0
                        }
                        
                        Button {
                            text: "Files"
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 30
                            checkable: true
                            checked: browserMode === 1
                            onClicked: browserMode = 1
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        ComboBox {
                            id: groupingCombo
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 30
                            model: ["Artist", "Album", "Year", "Genre"]
                        }
                        
                        Button {
                            text: "Settings"
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 30
                        }
                    }

                    // Navigation path bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 30
                        color: "#252525"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            text: browserMode === 1 ? filesModel.folder : "Album Artist > The Beatles > Abbey Road"
                            color: "#ccc"
                            font.pixelSize: 12
                        }
                    }

                    // Search bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 30
                        color: "#252525"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        
                        TextField {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            color: "#ccc"
                            font.pixelSize: 12
                            placeholderText: "Search by artist, album, or song..."
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }

                    FolderListModel {
                        id: filesModel
                        folder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
                        showDirs: true
                        showDotAndDotDot: false
                        showFiles: true
                        nameFilters: [
                            "*.wav", "*.flac", "*.mp3", "*.ogg", "*.opus", "*.aac", "*.m4a", "*.mp4", "*.mkv"
                        ]
                        sortField: FolderListModel.Name
                        sortReversed: false
                        caseSensitive: false
                    }

                    // Collection grid area
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1f1f1f"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        StackLayout {
                            anchors.fill: parent
                            currentIndex: browserMode
                            
                            GridView {
                                id: collectionGrid
                                anchors.fill: parent
                                anchors.margins: 4
                                cellWidth: 120
                                cellHeight: 150
                                model: 24
                                delegate: Rectangle {
                                    width: collectionGrid.cellWidth - 8
                                    height: collectionGrid.cellHeight - 8
                                    color: "#2a2a2a"
                                    border.color: "#444"
                                    border.width: 1
                                    radius: 4
                                    anchors.margins: 4
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4
                                        
                                        Rectangle {
                                            width: 60
                                            height: 60
                                            color: "#333"
                                            border.color: "#555"
                                            border.width: 1
                                            radius: 4
                                            Layout.alignment: Qt.AlignHCenter
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: "♪"
                                                color: "#666"
                                                font.pixelSize: 24
                                            }
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "Album " + (index + 1)
                                            color: "#ccc"
                                            font.pixelSize: 11
                                            font.bold: true
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "Artist Name"
                                            color: "#999"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "2000"
                                            color: "#666"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: console.log("Selected album", index)
                                    }
                                }
                                ScrollBar.vertical: ScrollBar {}
                            }

                            GridView {
                                id: filesGrid
                                anchors.fill: parent
                                anchors.margins: 4
                                cellWidth: 140
                                cellHeight: 140
                                model: filesModel
                                delegate: Rectangle {
                                    width: filesGrid.cellWidth - 12
                                    height: filesGrid.cellHeight - 12
                                    color: "#2a2a2a"
                                    border.color: "#444"
                                    border.width: 1
                                    radius: 4
                                    anchors.margins: 6
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 6
                                        
                                        Rectangle {
                                            width: 64
                                            height: 64
                                            color: "#333"
                                            border.color: "#555"
                                            border.width: 1
                                            radius: 4
                                            Layout.alignment: Qt.AlignHCenter
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: fileIsDir ? "📁" : "♪"
                                                color: "#bbb"
                                                font.pixelSize: 28
                                            }
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: fileName
                                            color: "#ccc"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton
                                        onClicked: {
                                            console.log("filesGrid clicked", fileName)
                                            const urlRole = (typeof fileURL !== 'undefined') ? fileURL : (typeof fileUrl !== 'undefined' ? fileUrl : null)
                                            if (urlRole === null) { console.warn('No fileURL role in delegate'); return }
                                            const asString = (typeof urlRole === 'string') ? urlRole : urlRole.toString()
                                            if (fileIsDir) {
                                                filesModel.folder = asString
                                            } else {
                                                const beforeCount = playlist.count()
                                                playlist.add(urlRole)
                                                const idx = currentIdx
                                                if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                                                    player.setNextFile(urlRole)
                                                }
                                            }
                                        }
                                        onDoubleClicked: {
                                            console.log("filesGrid doubleClicked", fileName)
                                            const urlRole = (typeof fileURL !== 'undefined') ? fileURL : (typeof fileUrl !== 'undefined' ? fileUrl : null)
                                            if (urlRole === null) { console.warn('No fileURL role in delegate'); return }
                                            const asString = (typeof urlRole === 'string') ? urlRole : urlRole.toString()
                                            if (fileIsDir) {
                                                filesModel.folder = asString
                                            } else {
                                                const beforeCount = playlist.count()
                                                playlist.add(urlRole)
                                                const idx = currentIdx
                                                if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                                                    player.setNextFile(urlRole)
                                                }
                                            }
                                        }
                                    }
                                }
                                ScrollBar.vertical: ScrollBar {}
                            }
                        }
                    }
                }
            }
        }

        // Playing Section (above controls)
        Rectangle {
            id: playingSection
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            Layout.minimumHeight: 60
            Layout.maximumHeight: 120
            color: "#252525"
            border.color: "#333"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Cover art
                Rectangle {
                    width: 56
                    height: 56
                    color: "#333"
                    border.color: "#555"
                    border.width: 1
                    radius: 4
                    
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        fillMode: Image.PreserveAspectFit
                        source: player.currentMetadata ? player.currentMetadata.coverArtUrl : ""
                        visible: source !== ""
                        
                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            color: "#666"
                            font.pixelSize: 20
                            visible: !parent.visible
                        }
                    }
                }

                // Track info
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: player.currentMetadata ? player.currentMetadata.title : "No track playing"
                        color: "#fff"
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: player.currentMetadata ? `${player.currentMetadata.artist || "Unknown Artist"} • ${player.currentMetadata.album || "Unknown Album"}` : ""
                        color: "#aaa"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                // Quick actions
                RowLayout {
                    spacing: 8

                    Button {
                        text: "♥"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        font.pixelSize: 14
                    }

                    Button {
                        text: "≡"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        font.pixelSize: 14
                    }
                }
            }
        }

        // Controls Section
        Rectangle {
            id: controlsSection
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            Layout.minimumHeight: 80
            Layout.maximumHeight: 120
            color: "#1a1a1a"
            border.color: "#333"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Progress bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: formatTime(player.position)
                        color: "#ccc"
                        font.pixelSize: 11
                        Layout.preferredWidth: 50
                    }

                    Slider {
                        id: positionSlider
                        Layout.fillWidth: true
                        from: 0
                        to: player.duration
                        value: player.position
                        onMoved: player.seek(value)
                    }

                    Text {
                        text: formatTime(player.duration)
                        color: "#ccc"
                        font.pixelSize: 11
                        Layout.preferredWidth: 50
                    }
                }

                // Playback controls
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // Left side controls
                    RowLayout {
                        spacing: 8

                        Button {
                            text: "⏮"
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            font.pixelSize: 14
                            onClicked: prevFromQueue()
                        }

                        Button {
                            text: player.playing ? "⏸" : "▶"
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            font.pixelSize: 16
                            onClicked: handlePlayToggle()
                        }

                        Button {
                            text: "⏭"
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            font.pixelSize: 14
                            onClicked: nextFromQueue()
                        }

                        Button {
                            text: "🔀"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                        }

                        Button {
                            text: "🔁"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Right side controls
                    RowLayout {
                        spacing: 8

                        Text {
                            text: "🔊"
                            color: "#ccc"
                            font.pixelSize: 14
                        }

                        Slider {
                            id: volumeSlider
                            from: 0
                            to: 1
                            value: 0.8
                            stepSize: 0.01
                            onValueChanged: player.volume = value
                            Layout.preferredWidth: 100
                        }

                        Button {
                            text: "≡"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }
    }

    // Helper function for time formatting
    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0) return "0:00"
        const seconds = Math.floor(milliseconds / 1000)
        const minutes = Math.floor(seconds / 60)
        const secs = seconds % 60
        return `${minutes}:${secs.toString().padStart(2, '0')}`
    }

    // File dialogs (kept for functionality)
    FileDialog {
        id: fileDialog
        title: "Open Audio File"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.openFile(selectedFile)
    }

    FileDialog {
        id: nextDialog
        title: "Select Next Track (Gapless)"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.setNextFile(selectedFile)
    }

    FileDialog {
        id: addDialog
        title: "Add to Playlist"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: {
            const beforeCount = playlist.count()
            playlist.add(selectedFile)
            const idx = currentIdx
            if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                player.setNextFile(selectedFile)
            }
        }
    }

    FileDialog {
        id: importDialog
        title: "Import Playlist (M3U/M3U8)"
        nameFilters: [
            "Playlists (*.m3u *.m3u8)",
            "All files (*)"
        ]
        onAccepted: playlist.importM3U8(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Export Playlist (M3U8)"
        nameFilters: [
            "Playlists (*.m3u8)",
            "All files (*)"
        ]
        onAccepted: playlist.exportM3U8(selectedFile)
    }

    // Output device selector (kept for functionality)
    ComboBox {
        id: outputBox
        visible: false
        model: player.audioOutputs
        onActivated: player.selectOutputByIndex(index)
        Component.onCompleted: currentIndex = Math.max(0, player.audioOutputs.indexOf(player.currentOutput))
    }

    Connections {
        target: player
        function onPlayingChanged() { /* Handled by player */ }
        function onPositionChanged() { /* Handled by player */ }
        function onDurationChanged() { /* Handled by player */ }
        function onCurrentSourceChanged() { /* Handled by player */ }
        function onCurrentMetadataChanged() { /* Metadata is now read on add, no update needed */ }
    }
}
