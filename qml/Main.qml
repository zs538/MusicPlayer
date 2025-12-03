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

    property int browserMode: 0 // 0 = Collection, 1 = Files
    
    // Convenience alias for current playing index (from player controller)
    readonly property int currentIdx: player.currentIndex

    function nextFromQueue() {
        player.next()
    }

    function prevFromQueue() {
        player.previous()
    }

    function handlePlayToggle() {
        if (player.playing) {
            player.pause()
            return
        }
        const idx = player.currentIndex
        const finished = player.duration > 0 && player.position >= (player.duration - 5)
        if (idx === -1) {
            if (playlist.count() > 0) {
                player.playIndex(0)
            } else {
                player.play()
            }
        } else if (idx === playlist.count() - 1 && finished) {
            player.playIndex(0)
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

                    // Playlist tabs area - fixed height, horizontally scrollable
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        Layout.maximumHeight: 36
                        spacing: 4
                        
                        // Scrollable tab list
                        ListView {
                            id: tabListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            orientation: ListView.Horizontal
                            spacing: 2
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            
                            model: playlistManager
                            
                            // Drag-and-drop properties
                            property int dragIndex: -1
                            property int dropIndex: -1
                            
                            delegate: Item {
                                id: tabDelegate
                                width: tabContent.width
                                height: tabListView.height
                                
                                property int visualIndex: index
                                
                                Rectangle {
                                    id: tabContent
                                    width: Math.max(tabLabel.implicitWidth + (closeBtn.visible ? 36 : 16), 80)
                                    height: 30
                                    anchors.verticalCenter: parent.verticalCenter
                                    radius: 4
                                    color: playlistManager.displayedIndex === index ? "#3a3a3a" : 
                                           (dragArea.containsMouse ? "#333" : "#252525")
                                    border.color: model.isActive ? "#3b82f6" : "#444"
                                    border.width: model.isActive ? 2 : 1
                                    
                                    // Drag state
                                    Drag.active: dragArea.drag.active
                                    Drag.source: tabDelegate
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2
                                    
                                    states: State {
                                        when: tabContent.Drag.active
                                        ParentChange { target: tabContent; parent: tabListView }
                                        AnchorChanges {
                                            target: tabContent
                                            anchors.verticalCenter: undefined
                                        }
                                    }
                                    
                                    Row {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 4
                                        
                                        Text {
                                            text: "●"
                                            color: "#f59e0b"
                                            font.pixelSize: 8
                                            visible: model.isDirty
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        
                                        Text {
                                            id: tabLabel
                                            text: model.name
                                            color: playlistManager.displayedIndex === index ? "#fff" : "#aaa"
                                            font.pixelSize: 12
                                        }
                                    }
                                    
                                    // Close button
                                    Rectangle {
                                        id: closeBtn
                                        anchors.right: parent.right
                                        anchors.rightMargin: 4
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 16
                                        height: 16
                                        radius: 8
                                        color: closeMouse.containsMouse ? "#555" : "transparent"
                                        visible: playlistManager.tabCount > 1
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: closeMouse.containsMouse ? "#fff" : "#888"
                                            font.pixelSize: 14
                                        }
                                        
                                        MouseArea {
                                            id: closeMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: playlistManager.closeTab(model.uuid)
                                        }
                                    }
                                    
                                    MouseArea {
                                        id: dragArea
                                        anchors.fill: parent
                                        anchors.rightMargin: closeBtn.visible ? 20 : 0
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                        drag.target: tabContent
                                        drag.axis: Drag.XAxis
                                        
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.MiddleButton) {
                                                if (playlistManager.tabCount > 1)
                                                    playlistManager.closeTab(model.uuid)
                                            } else {
                                                playlistManager.displayedPlaylistId = model.uuid
                                            }
                                        }
                                        
                                        onDoubleClicked: {
                                            renameDialog.tabUuid = model.uuid
                                            renameDialog.currentName = model.name
                                            renameDialog.open()
                                        }
                                        
                                        onPressed: tabListView.dragIndex = index
                                        
                                        onReleased: {
                                            if (tabListView.dragIndex >= 0 && tabListView.dropIndex >= 0 &&
                                                tabListView.dragIndex !== tabListView.dropIndex) {
                                                playlistManager.moveTab(tabListView.dragIndex, tabListView.dropIndex)
                                            }
                                            tabListView.dragIndex = -1
                                            tabListView.dropIndex = -1
                                        }
                                    }
                                    
                                    // Right-click area (separate to not interfere with drag)
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: {
                                            tabContextMenu.tabUuid = model.uuid
                                            tabContextMenu.tabName = model.name
                                            tabContextMenu.x = mouse.x
                                            tabContextMenu.y = mouse.y + tabContent.height
                                            tabContextMenu.open()
                                        }
                                    }
                                }
                                
                                DropArea {
                                    anchors.fill: parent
                                    onEntered: tabListView.dropIndex = index
                                }
                            }
                            
                            // Scroll with mouse wheel
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                onWheel: function(wheel) {
                                    tabListView.contentX = Math.max(0,
                                        Math.min(tabListView.contentWidth - tabListView.width,
                                            tabListView.contentX - wheel.angleDelta.y * 0.5))
                                }
                            }
                        }
                        
                        // Add tab button
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: 4
                            color: addMouse.containsMouse ? "#3a3a3a" : "#252525"
                            border.color: "#444"
                            
                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: "#aaa"
                                font.pixelSize: 16
                            }
                            
                            MouseArea {
                                id: addMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    var newUuid = playlistManager.createNewTab()
                                    playlistManager.displayedPlaylistId = newUuid
                                }
                            }
                        }
                    }

                    // Playlist content area (single view, switches based on displayed playlist)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                                    
                                    // Note: currentIdx is now managed by PlayerController via PlaybackQueue
                                    // The queue automatically tracks row moves, so we just log for debugging
                                    if (playingRowMoved) {
                                        const newCurrentIdx = finalOrder.indexOf(originalCurrentIdx)
                                        console.log("Playing row moved from", originalCurrentIdx, "to", newCurrentIdx)
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
                                            // Set displayed playlist as active and play track
                                            playlistManager.setActivePlaylist(playlistManager.displayedPlaylistId)
                                            player.playIndex(index)
                                        }
                                    }
                                }
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
                                // Note: PlaybackQueue automatically tracks index changes
                                const sortedRows = listView.selectedRows.sort((a, b) => b - a)
                                for (let i = 0; i < sortedRows.length; i++) {
                                    playlist.removeAt(sortedRows[i])
                                }
                                
                                // Clear selection after removal
                                listView.clearSelection()
                            }
                        }
                        
                        Button {
                            text: "Import"
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 30
                            onClicked: importDialog.open()
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
                                // Note: PlaybackQueue handles index reset automatically
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
                                                playlist.add(urlRole)
                                                // Note: PlayerController automatically arms next track when queue changes
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
                                                playlist.add(urlRole)
                                                // Note: PlayerController automatically arms next track when queue changes
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
                        text: player.currentMetadata ? (player.currentMetadata.artist || "Unknown Artist") + " • " + (player.currentMetadata.album || "Unknown Album") : ""
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
                        
                        // Use tentative position while dragging, actual position otherwise
                        value: pressed ? tentativePosition : player.position
                        
                        // Store tentative position while dragging
                        property real tentativePosition: player.position
                        
                        onPressedChanged: {
                            if (!pressed) {
                                // User released - seek to final position
                                player.seek(value)
                                tentativePosition = player.position
                            } else {
                                // User started dragging - initialize tentative position
                                tentativePosition = value
                            }
                        }
                        
                        onMoved: {
                            // Only update tentative position while dragging
                            if (pressed) {
                                tentativePosition = value
                            }
                        }
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
        var seconds = Math.floor(milliseconds / 1000)
        var minutes = Math.floor(seconds / 60)
        var secs = seconds % 60
        return minutes + ":" + (secs < 10 ? "0" : "") + secs
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
        onAccepted: {
            // Add to playlist and it will be armed automatically
            playlist.add(selectedFile)
        }
    }

    FileDialog {
        id: addDialog
        title: "Add to Playlist"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: {
            playlist.add(selectedFile)
            // Note: PlayerController automatically arms next track when queue changes
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
        function onCurrentSourceChanged() { /* currentIndex is now managed by PlayerController */ }
        function onCurrentMetadataChanged() { /* Metadata is now read on add, no update needed */ }
        function onCurrentIndexChanged() { /* Index updates are automatic */ }
    }

    // Tab context menu (using Popup instead of Menu to avoid Qt.labs.platform conflict)
    Popup {
        id: tabContextMenu
        property string tabUuid: ""
        property string tabName: ""
        property int tabIndex: -1
        
        width: 160
        padding: 4
        
        background: Rectangle {
            color: "#2a2a2a"
            border.color: "#555"
            border.width: 1
            radius: 4
        }
        
        contentItem: Column {
            spacing: 2
            
            // Set as Active
            Rectangle {
                width: parent.width
                height: 28
                color: setActiveArea.containsMouse ? "#3a3a3a" : "transparent"
                radius: 2
                
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Set as Active"
                    color: "#ddd"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    id: setActiveArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        playlistManager.setActivePlaylist(tabContextMenu.tabUuid)
                        tabContextMenu.close()
                    }
                }
            }
            
            // Rename
            Rectangle {
                width: parent.width
                height: 28
                color: renameArea.containsMouse ? "#3a3a3a" : "transparent"
                radius: 2
                
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Rename..."
                    color: "#ddd"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    id: renameArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        renameDialog.tabUuid = tabContextMenu.tabUuid
                        renameDialog.currentName = tabContextMenu.tabName
                        tabContextMenu.close()
                        renameDialog.open()
                    }
                }
            }
            
            // Duplicate
            Rectangle {
                width: parent.width
                height: 28
                color: duplicateArea.containsMouse ? "#3a3a3a" : "transparent"
                radius: 2
                
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Duplicate"
                    color: "#ddd"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    id: duplicateArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        var newUuid = playlistManager.duplicateTab(tabContextMenu.tabUuid)
                        playlistManager.displayedPlaylistId = newUuid
                        tabContextMenu.close()
                    }
                }
            }
            
            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#444"
            }
            
            // Close
            Rectangle {
                width: parent.width
                height: 28
                color: closeArea.containsMouse && playlistManager.tabCount > 1 ? "#3a3a3a" : "transparent"
                radius: 2
                opacity: playlistManager.tabCount > 1 ? 1.0 : 0.5
                
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Close"
                    color: playlistManager.tabCount > 1 ? "#ddd" : "#666"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: playlistManager.tabCount > 1
                    onClicked: {
                        playlistManager.closeTab(tabContextMenu.tabUuid)
                        tabContextMenu.close()
                    }
                }
            }
        }
    }

    // Rename dialog
    Popup {
        id: renameDialog
        modal: true
        anchors.centerIn: parent
        width: 300
        height: 150
        padding: 20
        
        property string tabUuid: ""
        property string currentName: ""
        
        background: Rectangle {
            color: "#2a2a2a"
            border.color: "#555"
            border.width: 1
            radius: 8
        }
        
        contentItem: ColumnLayout {
            spacing: 12
            
            Text {
                text: "Rename Playlist"
                color: "#fff"
                font.pixelSize: 16
                font.bold: true
            }
            
            TextField {
                id: renameField
                Layout.fillWidth: true
                placeholderText: "Playlist name"
                text: renameDialog.currentName
                onAccepted: {
                    if (text.trim() !== "") {
                        playlistManager.renameTab(renameDialog.tabUuid, text.trim())
                        renameDialog.close()
                    }
                }
            }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: "Cancel"
                    onClicked: renameDialog.close()
                }
                
                Button {
                    text: "OK"
                    onClicked: {
                        if (renameField.text.trim() !== "") {
                            playlistManager.renameTab(renameDialog.tabUuid, renameField.text.trim())
                            renameDialog.close()
                        }
                    }
                }
            }
        }
        
        onOpened: {
            renameField.text = currentName
            renameField.selectAll()
            renameField.forceActiveFocus()
        }
    }
}
