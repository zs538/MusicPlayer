import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MusicPlayer

Rectangle {
    id: root
    color: Theme.surface

    readonly property int playlistRowHeight: 22

    property string playlistId: ViewedPlaylistRouter.viewedPlaylistId
    property var playlistTrackListLayout: TrackListColumnsSupport.ensureLayout(SessionManager.playlistTrackListLayout)
    property var customTagKeys: AppViewModel.libraryDatabase ? AppViewModel.libraryDatabase.customTagKeys() : []
    property string playlistSortKey: ""
    property bool playlistSortAscending: true
    readonly property bool focusWithinPlaylist: root.activeFocus || renameField.activeFocus || listView.activeFocus
    property bool isUserCreated: {
        let idx = AppViewModel.playlistStore.indexOfUuid(playlistId)
        return idx >= 0 ? AppViewModel.playlistStore.tabIsUserCreated(idx) : true
    }

    function focusPlaylist() {
        if (playlistHeader.renaming)
            renameField.forceActiveFocus()
        else
            listView.forceActiveFocus()
    }
    function orderedPlaylists() {
        let userPlaylists = []
        let generatedPlaylists = []
        for (let i = 0; i < AppViewModel.playlistTabsModel.rowCount(); i++) {
            let idx = AppViewModel.playlistTabsModel.index(i, 0)
            let item = {
                uuid: AppViewModel.playlistTabsModel.data(idx, 257),
                isUserCreated: AppViewModel.playlistTabsModel.data(idx, 262)
            }
            if (item.isUserCreated)
                userPlaylists.push(item.uuid)
            else
                generatedPlaylists.push(item.uuid)
        }
        return userPlaylists.concat(generatedPlaylists)
    }
    function stepViewedPlaylist(delta) {
        let ordered = orderedPlaylists()
        let currentIdx = ordered.indexOf(root.playlistId)
        if (currentIdx < 0)
            currentIdx = 0
        let nextIdx = Math.max(0, Math.min(ordered.length - 1, currentIdx + delta))
        if (nextIdx !== currentIdx && ordered[nextIdx]) {
            ViewedPlaylistRouter.viewedPlaylistId = ordered[nextIdx]
            Qt.callLater(function() {
                root.focusPlaylist()
            })
        }
    }
    function openPlaylistMenu() {
        playlistSwitchMenu.popup()
        if (playlistSwitchMenu.count > 0)
            playlistSwitchMenu.currentIndex = 0
    }
    function createNewPlaylistAndFocus() {
        let newId = AppViewModel.playlistStore.createNewTab()
        if (newId.length === 0)
            return
        ViewedPlaylistRouter.viewedPlaylistId = newId
        focusPlaylist()
    }
    function startRenameViewedPlaylist() {
        focusPlaylist()
        playlistHeader.startRename()
    }
    function selectAllTracks() {
        controller.selectAll()
        focusPlaylist()
    }
    function closeViewedPlaylist() {
        if (AppViewModel.playlistTabsModel.rowCount() <= 1)
            return
        AppViewModel.playlistStore.closeTab(root.playlistId)
    }
    function setPlaylistTrackListLayout(layout) {
        let normalized = TrackListColumnsSupport.ensureLayout(layout)
        playlistTrackListLayout = normalized
        SessionManager.playlistTrackListLayout = normalized
        SessionManager.playlistColumns = normalized.columns
    }
    function resetPlaylistSortState() {
        playlistSortKey = ""
        playlistSortAscending = true
    }
    function togglePlaylistSort(key) {
        let normalizedKey = String(key || "")
        if (!normalizedKey.length)
            return
        if (playlistSortKey === normalizedKey)
            playlistSortAscending = !playlistSortAscending
        else {
            playlistSortKey = normalizedKey
            playlistSortAscending = true
        }
        controller.sortByColumn(normalizedKey, playlistSortAscending)
    }
    onPlaylistIdChanged: resetPlaylistSortState()

    PlaylistPanelController {
        id: controller
        model: ViewedPlaylistRouter.viewedPlaylistModel || AppViewModel.displayedPlaylistModel
    }

    // Explicitly update controller model when viewed playlist changes
    Connections {
        target: ViewedPlaylistRouter
        function onViewedPlaylistModelChanged() {
            controller.model = ViewedPlaylistRouter.viewedPlaylistModel || AppViewModel.displayedPlaylistModel
        }
        function onScrollToIndexRequested(index) {
            if (index >= 0 && index < listView.count) {
                listView.positionViewAtIndex(index, ListView.Center)
            }
        }
    }

    Connections {
        target: AppViewModel.libraryDatabase
        function onDatabaseChanged() {
            root.customTagKeys = AppViewModel.libraryDatabase ? AppViewModel.libraryDatabase.customTagKeys() : []
        }
    }

    Connections {
        target: SessionManager
        function onPlaylistTrackListLayoutChanged() {
            root.playlistTrackListLayout = TrackListColumnsSupport.ensureLayout(SessionManager.playlistTrackListLayout)
        }
    }

    Component.onCompleted: {
        if (!SessionManager.playlistTrackListLayout.columns || SessionManager.playlistTrackListLayout.columns.length === 0)
            root.setPlaylistTrackListLayout(root.playlistTrackListLayout)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Playlist header strip
        Rectangle {
            id: playlistHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            z: 2
            color: Theme.surfaceAlt

            property bool renaming: false
            property string renameOriginalText: ""

            function updateHeaderTitle() {
                let idx = AppViewModel.playlistStore.indexOfUuid(root.playlistId)
                headerLabel.text = idx >= 0 ? AppViewModel.playlistStore.tabName(idx) : "Playlist"
            }
            function startRename() {
                updateHeaderTitle()
                renameOriginalText = headerLabel.text
                renameField.text = headerLabel.text
                renaming = true
                renameField.forceActiveFocus()
                renameField.selectAll()
            }
            function cancelRename() {
                renaming = false
                renameField.text = renameOriginalText
                root.focusPlaylist()
            }
            function commitRename() {
                let trimmed = renameField.text.trim()
                renaming = false
                root.focusPlaylist()
                if (trimmed.length > 0 && trimmed !== renameOriginalText)
                    AppViewModel.playlistStore.renameTab(root.playlistId, trimmed)
                updateHeaderTitle()
            }

            Label {
                id: headerLabel
                anchors.centerIn: parent
                visible: !playlistHeader.renaming
                text: "Playlist"
                font.bold: true
                font.pixelSize: 11
                color: Theme.textPrimary
                elide: Text.ElideMiddle
                width: Math.min(implicitWidth, parent.width - 16)
            }

            TextField {
                id: renameField
                anchors.centerIn: parent
                visible: playlistHeader.renaming
                width: parent.width - 16
                height: headerLabel.implicitHeight
                text: ""
                color: Theme.textPrimary
                font.bold: true
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                selectByMouse: true
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: Item {}
                TextCursor {}
                Keys.onEscapePressed: (event) => {
                    playlistHeader.cancelRename()
                    event.accepted = true
                }
                Keys.onReturnPressed: (event) => {
                    playlistHeader.commitRename()
                    event.accepted = true
                }
                Keys.onEnterPressed: (event) => {
                    playlistHeader.commitRename()
                    event.accepted = true
                }
                onActiveFocusChanged: {
                    if (!activeFocus && playlistHeader.renaming)
                        playlistHeader.cancelRename()
                }
                onEditingFinished: {
                    if (playlistHeader.renaming)
                        playlistHeader.cancelRename()
                }
            }

            Component.onCompleted: playlistHeader.updateHeaderTitle()

            Connections {
                target: root
                function onPlaylistIdChanged() { playlistHeader.updateHeaderTitle() }
            }

            Connections {
                target: AppViewModel.playlistStore
                function onTabDataChanged(index) {
                    if (index === AppViewModel.playlistStore.indexOfUuid(root.playlistId))
                        playlistHeader.updateHeaderTitle()
                }
            }

            Connections {
                target: AppViewModel.playlistTabsModel
                function onDataChanged(topLeft, bottomRight, roles) {
                    let idx = AppViewModel.playlistStore.indexOfUuid(root.playlistId)
                    if (idx < 0) return
                    if (topLeft.row <= idx && idx <= bottomRight.row)
                        playlistHeader.updateHeaderTitle()
                }
            }

            MouseArea {
                id: headerMouseArea
                anchors.fill: parent
                visible: !playlistHeader.renaming
                enabled: visible
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                property point toolTipAnchorPos: Qt.point(0, 0)

                function updateToolTipAnchor(x, y) {
                    toolTipAnchorPos = Qt.point(x + 8, y + 12)
                }

                StyledHoverToolTip {
                    id: headerToolTip
                    parent: headerMouseArea
                    visible: headerMouseArea.containsMouse && headerLabel.text.length > 0
                    delay: 800
                    timeout: 5000
                    text: headerLabel.text
                    x: Math.max(0, headerMouseArea.toolTipAnchorPos.x)
                    y: Math.max(0, headerMouseArea.toolTipAnchorPos.y)
                    onVisibleChanged: {
                        if (visible)
                            headerMouseArea.updateToolTipAnchor(headerMouseArea.mouseX, headerMouseArea.mouseY)
                    }
                }
                
                onClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        root.openPlaylistMenu()
                    else if (mouse.button === Qt.RightButton)
                        playlistActionsMenu.popup()
                }
                
                onWheel: (wheel) => {
                    root.stepViewedPlaylist(wheel.angleDelta.y > 0 ? -1 : 1)
                    wheel.accepted = true
                }
            }

            // Left-click: switch playlist menu
            Menu {
                id: playlistSwitchMenu
                
                // Build menu items dynamically when opened
                onAboutToShow: {
                    // Clear existing dynamic items
                    while (playlistSwitchMenu.count > 2) {
                        let item = playlistSwitchMenu.itemAt(2)
                        playlistSwitchMenu.removeItem(item)
                        item.destroy()
                    }
                    
                    let userPlaylists = []
                    let genPlaylists = []
                    
                    // Separate playlists by type
                    for (let i = 0; i < AppViewModel.playlistTabsModel.rowCount(); i++) {
                        let idx = AppViewModel.playlistTabsModel.index(i, 0)
                        let item = {
                            uuid: AppViewModel.playlistTabsModel.data(idx, 257),
                            name: AppViewModel.playlistTabsModel.data(idx, 258),
                            isActive: AppViewModel.playlistTabsModel.data(idx, 259),
                            isUserCreated: AppViewModel.playlistTabsModel.data(idx, 262)
                        }
                        if (item.isUserCreated) userPlaylists.push(item)
                        else genPlaylists.push(item)
                    }
                    
                    // Add user playlists
                    for (let pl of userPlaylists) {
                        let menuItem = playlistItemComponent.createObject(playlistSwitchMenu.contentItem, {
                            plUuid: pl.uuid, plName: pl.name, isActive: pl.isActive
                        })
                        playlistSwitchMenu.addItem(menuItem)
                    }
                    
                    // Add separator if both types exist
                    if (userPlaylists.length > 0 && genPlaylists.length > 0) {
                        let separator = separatorComponent.createObject(playlistSwitchMenu.contentItem)
                        playlistSwitchMenu.addItem(separator)
                    }
                    
                    // Add generated playlists
                    for (let pl of genPlaylists) {
                        let menuItem = playlistItemComponent.createObject(playlistSwitchMenu.contentItem, {
                            plUuid: pl.uuid, plName: pl.name, isActive: pl.isActive
                        })
                        playlistSwitchMenu.addItem(menuItem)
                    }
                }
                
                MenuItem {
                    text: "New playlist"
                    PointingCursor {}
                    onTriggered: {
                        let newId = AppViewModel.playlistStore.createNewTab()
                        ViewedPlaylistRouter.viewedPlaylistId = newId
                        Qt.callLater(function() {
                            root.focusPlaylist()
                        })
                    }
                }
                MenuSeparator {}
            }
            
            Component {
                id: playlistItemComponent
                MenuItem {
                    property string plUuid
                    property string plName
                    property bool isActive: false
                    
                    text: String(plName).replace(/&/g, "&&")
                    icon.source: isActive && AppViewModel.playbackState === AppViewModel.Playing
                        ? Qt.resolvedUrl("../icons/play_arrow.svg")
                        : ""
                    icon.color: Theme.textPrimary
                    icon.width: 12
                    icon.height: 12
                    PointingCursor {}
                    onTriggered: {
                        ViewedPlaylistRouter.viewedPlaylistId = plUuid
                        Qt.callLater(function() {
                            root.focusPlaylist()
                        })
                    }
                }
            }
            
            Component {
                id: separatorComponent
                MenuSeparator {}
            }

            // Right-click: playlist actions menu
            Menu {
                id: playlistActionsMenu
                MenuItem {
                    text: "New playlist"
                    PointingCursor {}
                    onTriggered: {
                        let newId = AppViewModel.playlistStore.createNewTab()
                        ViewedPlaylistRouter.viewedPlaylistId = newId
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: "Clear"
                    PointingCursor {}
                    onTriggered: controller.model?.clear()
                }
                MenuItem {
                    text: "Rename"
                    PointingCursor {}
                    onTriggered: playlistHeader.startRename()
                }
                MenuItem {
                    text: "Make permanent"
                    visible: !root.isUserCreated
                    height: visible ? implicitHeight : 0
                    PointingCursor {}
                    onTriggered: AppViewModel.playlistStore.setPlaylistUserCreated(root.playlistId, true)
                }
                MenuSeparator {}
                MenuItem {
                    text: "Close playlist"
                    enabled: AppViewModel.playlistTabsModel.rowCount() > 1
                    PointingCursor {}
                    onTriggered: AppViewModel.playlistStore.closeTab(root.playlistId)
                }
                MenuSeparator {}
                MenuItem {
                    text: "Import..."
                    PointingCursor {}
                    onTriggered: {
                        let dialog = importDialog.createObject(root)
                        dialog.open()
                    }
                }
                MenuItem {
                    text: "Export..."
                    PointingCursor {}
                    onTriggered: {
                        let dialog = exportDialog.createObject(root, {playlistId: root.playlistId})
                        dialog.open()
                    }
                }
            }
        }

        TrackColumnsHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: height
            layout: root.playlistTrackListLayout
            customTagKeys: root.customTagKeys
            rowHeight: root.playlistRowHeight
            leftMargin: 2
            rightMargin: 2
            onLayoutEdited: (layout) => root.setPlaylistTrackListLayout(layout)
            onColumnClicked: (key) => root.togglePlaylistSort(key)
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            interactive: false
            activeFocusOnTab: false
            model: controller.model
            boundsBehavior: Flickable.StopAtBounds

            function ensureRowVisible(row) {
                if (row >= 0 && row < count)
                    positionViewAtIndex(row, ListView.Contain)
            }
            
            footer: Item { width: 1; height: 20 }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Behavior on contentY { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (e) => listView.contentY = Math.max(0, Math.min(listView.contentHeight - listView.height, listView.contentY - e.angleDelta.y))
            }
            Keys.onDeletePressed: (event) => {
                if (controller.selectedCount <= 0)
                    return
                controller.removeSelected()
                event.accepted = true
            }
            Keys.onEscapePressed: (event) => {
                if (controller.selectedCount <= 0)
                    return
                controller.clearSelection()
                event.accepted = true
            }
            Keys.onReturnPressed: (event) => {
                if (controller.selectedCount <= 0)
                    return
                AppViewModel.browseActivation.activatePlaylistRow(controller.selectedRows()[0])
                event.accepted = true
            }
            Keys.onEnterPressed: (event) => {
                if (controller.selectedCount <= 0)
                    return
                AppViewModel.browseActivation.activatePlaylistRow(controller.selectedRows()[0])
                event.accepted = true
            }
            Keys.onUpPressed: (event) => {
                const row = controller.keyboardMoveSelection(-1, (event.modifiers & Qt.ShiftModifier) !== 0)
                if (row < 0)
                    return
                listView.ensureRowVisible(row)
                event.accepted = true
            }
            Keys.onDownPressed: (event) => {
                const row = controller.keyboardMoveSelection(1, (event.modifiers & Qt.ShiftModifier) !== 0)
                if (row < 0)
                    return
                listView.ensureRowVisible(row)
                event.accepted = true
            }
            Keys.onLeftPressed: (event) => {
                if ((event.modifiers & Qt.AltModifier) === 0)
                    return
                root.stepViewedPlaylist(-1)
                event.accepted = true
            }
            Keys.onRightPressed: (event) => {
                if ((event.modifiers & Qt.AltModifier) === 0)
                    return
                root.stepViewedPlaylist(1)
                event.accepted = true
            }
            Keys.onPressed: (event) => {
                if (event.modifiers === Qt.AltModifier) {
                    if (event.key === Qt.Key_H) {
                        root.stepViewedPlaylist(-1)
                        event.accepted = true
                    } else if (event.key === Qt.Key_L) {
                        root.stepViewedPlaylist(1)
                        event.accepted = true
                    }
                    return
                }
                if (event.modifiers === Qt.ControlModifier) {
                    if (event.key === Qt.Key_A) {
                        root.selectAllTracks()
                        event.accepted = true
                    }
                    return
                }
                if (event.modifiers === (Qt.ControlModifier | Qt.ShiftModifier)) {
                    if (event.key === Qt.Key_C) {
                        root.closeViewedPlaylist()
                        event.accepted = true
                    }
                    return
                }
                if (event.modifiers !== Qt.NoModifier)
                    return
                if (event.key === Qt.Key_M) {
                    root.openPlaylistMenu()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_J) {
                    const row = controller.keyboardMoveSelection(1, false)
                    if (row < 0)
                        return
                    listView.ensureRowVisible(row)
                    event.accepted = true
                } else if (event.key === Qt.Key_K) {
                    const row = controller.keyboardMoveSelection(-1, false)
                    if (row < 0)
                        return
                    listView.ensureRowVisible(row)
                    event.accepted = true
                }
            }
            
            MouseArea {
                anchors.fill: parent
                z: -1
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                property bool isDragSelecting: false
                property bool didDragSelect: false
                
                onPressed: {
                    if (mouse.button !== Qt.LeftButton)
                        return
                    let clickY = mouseY + listView.contentY
                    let lastEntryBottom = listView.count * root.playlistRowHeight
                    if (clickY >= lastEntryBottom) {
                        isDragSelecting = true
                        didDragSelect = false
                        controller.clearSelection()
                    }
                }
                
                onPositionChanged: (mouse) => {
                    if (pressed && isDragSelecting && listView.count > 0) {
                        let globalY = mouse.y + listView.contentY
                        let lastEntryBottom = listView.count * root.playlistRowHeight
                        if (globalY < lastEntryBottom) {
                            let hoverIndex = Math.floor(globalY / root.playlistRowHeight)
                            hoverIndex = Math.max(0, Math.min(listView.count - 1, hoverIndex))
                            controller.selectRange(hoverIndex, listView.count - 1)
                            didDragSelect = true
                        } else {
                            controller.clearSelection()
                        }
                    }
                }
                
                onReleased: isDragSelecting = false
                onCanceled: {
                    isDragSelecting = false
                    didDragSelect = false
                }
                
                onClicked: {
                    let clickY = mouseY + listView.contentY
                    let lastEntryBottom = listView.count * root.playlistRowHeight
                    listView.forceActiveFocus()
                    if (mouse.button === Qt.RightButton) {
                        if (clickY >= lastEntryBottom) {
                            controller.clearSelection()
                            contextMenu.hasTrackContext = false
                            contextMenu.popup()
                        }
                        return
                    }
                    if (!didDragSelect) controller.clearSelection()
                    didDragSelect = false
                }
            }

            Rectangle {
                id: dropIndicator
                width: parent.width
                height: 2
                color: Theme.accent
                visible: DragManager.isDragging && DragManager.dropTargetId === root.playlistId
                y: Math.max(0, DragManager.dropTargetIndex * root.playlistRowHeight - listView.contentY)
                z: 100
            }

            delegate: Rectangle {
                id: del
                width: listView.width
                height: root.playlistRowHeight

                required property int index
                required property string filePath
                required property var trackData

                property bool selected: controller.isRowSelected(index)
                property bool playing: AppViewModel.currentIndex === index &&
                    ViewedPlaylistRouter.viewedPlaylistId === ViewedPlaylistRouter.activePlaylistId

                Connections {
                    target: controller
                    function onSelectionChanged() { del.selected = controller.isRowSelected(del.index) }
                }

                color: selected ? Theme.pressed : ma.containsMouse ? Theme.hover : "transparent"
                opacity: DragManager.isDragging && DragManager.sourceId === root.playlistId && DragManager.draggedIndices.indexOf(index) >= 0 ? 0.4 : 1

                TrackColumnsRow {
                    id: trackColumnsRow
                    anchors.fill: parent
                    columns: root.playlistTrackListLayout.columns
                    trackData: del.trackData
                    leftMargin: 2
                    rightMargin: 2
                    primaryTextColor: del.playing ? "#000000" : Theme.textPrimary
                    secondaryTextColor: del.playing ? "#000000" : Theme.textSecondary
                    fontPixelSize: 11
                    boldPrimary: del.playing
                }

                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: DragManager.isDragging && DragManager.sourceId === root.playlistId ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    property point pressPos
                    property point toolTipAnchorPos: Qt.point(0, 0)
                    readonly property string hoveredColumnText: containsMouse ? trackColumnsRow.textAtX(mouseX) : ""

                    function updateToolTipAnchor(x, y) {
                        toolTipAnchorPos = Qt.point(x + 8, y + 12)
                    }

                    StyledHoverToolTip {
                        id: rowToolTip
                        parent: ma
                        visible: ma.containsMouse && !DragManager.isDragging && ma.hoveredColumnText.length > 0
                        delay: 800
                        timeout: 5000
                        text: ma.hoveredColumnText
                        x: Math.max(0, ma.toolTipAnchorPos.x)
                        y: Math.max(0, ma.toolTipAnchorPos.y)
                        onVisibleChanged: {
                            if (visible)
                                ma.updateToolTipAnchor(ma.mouseX, ma.mouseY)
                        }
                    }

                    onHoveredColumnTextChanged: {
                        if (hoveredColumnText.length > 0 && rowToolTip.visible)
                            updateToolTipAnchor(mouseX, mouseY)
                    }

                    onPressed: function(mouse) {
                        if (mouse.button === Qt.LeftButton) pressPos = Qt.point(mouse.x, mouse.y)
                    }

                    onClicked: function(mouse) {
                        listView.forceActiveFocus()
                        if (mouse.button === Qt.RightButton) {
                            if (!del.selected) controller.clickRow(del.index, false, false)
                            contextMenu.hasTrackContext = true
                            contextMenu.popup()
                        } else if (!DragManager.isDragging) {
                            controller.clickRow(del.index, mouse.modifiers & Qt.ControlModifier, mouse.modifiers & Qt.ShiftModifier)
                        }
                    }

                    onDoubleClicked: AppViewModel.browseActivation.activatePlaylistRow(del.index)

                    onPositionChanged: function(mouse) {
                        if (pressed && !DragManager.isDragging) {
                            let dx = mouse.x - pressPos.x, dy = mouse.y - pressPos.y
                            if (dx*dx + dy*dy > 25) {
                                if (!del.selected) controller.clickRow(del.index, false, false)
                                let indices = controller.selectedRows()
                                let items = []
                                for (let i = 0; i < indices.length; i++) {
                                    let idx = controller.model.index(indices[i], 0)
                                    items.push({ filePath: controller.model.data(idx, 257) })
                                }
                                DragManager.startDrag(indices, items, root.playlistId)
                            }
                        }
                        if (DragManager.isDragging) {
                            let y = mapToItem(listView, mouse.x, mouse.y).y + listView.contentY
                            DragManager.setDropTarget(root.playlistId, Math.max(0, Math.min(Math.round(y / del.height), listView.count)))
                        }
                    }

                    onReleased: {
                        if (DragManager.isDragging) {
                            let drop = DragManager.endDrag()
                            if (drop.valid && drop.sourceId === drop.targetId && drop.sourceId === root.playlistId) {
                                controller.moveSelectedTo(drop.targetIndex)
                            }
                        }
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                keys: ["text/uri-list"]
                onDropped: (drop) => { if (drop.hasUrls) AppViewModel.browseActivation.dropUrlsToViewed(drop.urls) }
                Rectangle { anchors.fill: parent; color: Theme.accent; opacity: parent.containsDrag ? 0.2 : 0 }
            }

            Label {
                anchors.centerIn: parent
                text: "Empty playlist"
                color: Theme.textMuted
                visible: listView.count === 0
            }
        }

    } // end ColumnLayout

    MouseArea {
        anchors.fill: parent
        anchors.topMargin: playlistHeader.height
        visible: playlistHeader.renaming
        enabled: visible
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 1
        onClicked: playlistHeader.cancelRename()
    }

    // Background right-click
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        z: -1
        onClicked: {
            controller.clearSelection()
            contextMenu.hasTrackContext = false
            contextMenu.popup()
        }
    }

    // Simple context menu
    Menu {
        id: contextMenu
        property bool hasTrackContext: false

        MenuItem { 
            text: "New playlist"
            PointingCursor {}
            onTriggered: {
                let newId = AppViewModel.playlistStore.createNewTab()
                ViewedPlaylistRouter.viewedPlaylistId = newId
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Play"
            visible: contextMenu.hasTrackContext
            height: visible ? implicitHeight : 0
            enabled: controller.selectedCount > 0
            PointingCursor {}
            onTriggered: AppViewModel.browseActivation.activatePlaylistRow(controller.selectedRows()[0])
        }
        MenuItem {
            text: "Remove"
            visible: contextMenu.hasTrackContext
            height: visible ? implicitHeight : 0
            enabled: controller.selectedCount > 0
            PointingCursor {}
            onTriggered: controller.removeSelected()
        }
        MenuItem {
            text: "Rescan track(s)"
            visible: contextMenu.hasTrackContext
            height: visible ? implicitHeight : 0
            enabled: controller.selectedCount > 0
            PointingCursor {}
            onTriggered: AppViewModel.rescanPlaylistSelection(root.playlistId, controller.selectedRows())
        }
        MenuSeparator {
            visible: contextMenu.hasTrackContext
            height: visible ? implicitHeight : 0
        }
        MenuItem {
            text: "Select all"
            PointingCursor {}
            onTriggered: controller.selectAll()
        }
        MenuItem {
            text: "Clear playlist"
            PointingCursor {}
            onTriggered: controller.model?.clear()
        }
        MenuItem { 
            text: "Rename playlist"
            PointingCursor {}
            onTriggered: playlistHeader.startRename()
        }
        MenuSeparator {}
        MenuItem { 
            text: "Close playlist"
            enabled: AppViewModel.playlistTabsModel.rowCount() > 1
            PointingCursor {}
            onTriggered: AppViewModel.playlistStore.closeTab(root.playlistId)
        }
        MenuSeparator {}
        MenuItem { 
            text: "Import playlist..."
            PointingCursor {}
            onTriggered: {
                let dialog = importDialog.createObject(root)
                dialog.open()
            }
        }
        MenuItem { 
            text: "Export playlist..."
            PointingCursor {}
            onTriggered: {
                let dialog = exportDialog.createObject(root, {playlistId: root.playlistId})
                dialog.open()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: root.playlistTrackListLayout.headerVisible ? "Hide column header" : "Show column header"
            PointingCursor {}
            onTriggered: root.setPlaylistTrackListLayout(TrackListColumnsSupport.setHeaderVisible(root.playlistTrackListLayout, !root.playlistTrackListLayout.headerVisible))
        }

    }

    // Import dialog
    Component {
        id: importDialog
        FileDialog {
            title: "Import Playlist"
            fileMode: FileDialog.OpenFile
            nameFilters: ["Playlist files (*.m3u *.m3u8)", "All files (*)"]
            onAccepted: {
                let path = String(selectedFile)
                let newId = AppViewModel.playlistStore.importPlaylist(path)
                if (newId) {
                    ViewedPlaylistRouter.viewedPlaylistId = newId
                }
            }
        }
    }

    // Export dialog
    Component {
        id: exportDialog
        FileDialog {
            property string playlistId
            title: "Export Playlist"
            fileMode: FileDialog.SaveFile
            nameFilters: ["M3U8 Playlist (*.m3u8)", "M3U Playlist (*.m3u)"]
            defaultSuffix: "m3u8"
            onAccepted: {
                let path = String(selectedFile)
                AppViewModel.playlistStore.exportPlaylist(playlistId, path)
            }
        }
    }
}
