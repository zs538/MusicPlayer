import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MusicPlayer

Rectangle {
    id: root
    color: Theme.surface

    property string playlistId: ViewedPlaylistRouter.viewedPlaylistId

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
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Playlist header strip
        Rectangle {
            id: playlistHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            color: Theme.surfaceAlt

            function updateHeaderTitle() {
                let idx = AppViewModel.playlistStore.indexOfUuid(root.playlistId)
                headerLabel.text = idx >= 0 ? AppViewModel.playlistStore.tabName(idx) : "Playlist"
            }

            Label {
                id: headerLabel
                anchors.centerIn: parent
                text: "Playlist"
                font.bold: true
                font.pixelSize: 11
                color: Theme.textPrimary
                elide: Text.ElideMiddle
                width: Math.min(implicitWidth, parent.width - 16)
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
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton)
                        playlistSwitchMenu.popup()
                    else if (mouse.button === Qt.RightButton)
                        playlistActionsMenu.popup()
                }
                
                onWheel: function(wheel) {
                    let currentIndex = AppViewModel.playlistStore.indexOfUuid(root.playlistId)
                    let newIndex = currentIndex
                    
                    if (wheel.angleDelta.y > 0) {
                        // Scroll up - previous playlist
                        if (currentIndex > 0)
                            newIndex = currentIndex - 1
                    } else {
                        // Scroll down - next playlist
                        if (currentIndex < AppViewModel.playlistStore.tabCount - 1)
                            newIndex = currentIndex + 1
                    }
                    
                    if (newIndex !== currentIndex) {
                        // Get UUID from the model like the menu does
                        let idx = AppViewModel.playlistTabsModel.index(newIndex, 0)
                        let newUuid = AppViewModel.playlistTabsModel.data(idx, 257) // UuidRole
                        if (newUuid) {
                            ViewedPlaylistRouter.viewedPlaylistId = newUuid
                        }
                    }
                    
                    wheel.accepted = true
                }
            }

            // Left-click: switch playlist menu
            Menu {
                id: playlistSwitchMenu
                MenuItem {
                    text: "New playlist"
                    onTriggered: {
                        let newId = AppViewModel.playlistStore.createNewTab()
                        ViewedPlaylistRouter.viewedPlaylistId = newId
                    }
                }
                MenuSeparator {}
                Repeater {
                    model: AppViewModel.playlistTabsModel
                    MenuItem {
                        required property string uuid
                        required property string name
                        required property bool isUserCreated
                        visible: isUserCreated
                        height: isUserCreated ? implicitHeight : 0
                        
                        contentItem: Row {
                            spacing: 8
                            Image {
                                source: "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='8' height='8' viewBox='0 0 8 8'%3E%3Cpath d='M1 1l6 3-6 3z' fill='%23currentColor'/%3E%3C/svg%3E"
                                visible: uuid === ViewedPlaylistRouter.activePlaylistId && AppViewModel.playbackState !== AppViewModel.Stopped
                                sourceSize.width: 8
                                sourceSize.height: 8
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: name
                                font.bold: uuid === root.playlistId
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        
                        onTriggered: ViewedPlaylistRouter.viewedPlaylistId = uuid
                    }
                }
                MenuSeparator {
                    visible: {
                        let hasUser = false, hasGen = false
                        for (let i = 0; i < AppViewModel.playlistTabsModel.rowCount(); i++) {
                            let idx = AppViewModel.playlistTabsModel.index(i, 0)
                            if (AppViewModel.playlistTabsModel.data(idx, 262)) hasUser = true
                            else hasGen = true
                        }
                        return hasUser && hasGen
                    }
                }
                Repeater {
                    model: AppViewModel.playlistTabsModel
                    MenuItem {
                        required property string uuid
                        required property string name
                        required property bool isUserCreated
                        visible: !isUserCreated
                        height: !isUserCreated ? implicitHeight : 0
                        
                        contentItem: Row {
                            spacing: 8
                            Image {
                                source: "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='8' height='8' viewBox='0 0 8 8'%3E%3Cpath d='M1 1l6 3-6 3z' fill='%23currentColor'/%3E%3C/svg%3E"
                                visible: uuid === ViewedPlaylistRouter.activePlaylistId && AppViewModel.playbackState !== AppViewModel.Stopped
                                sourceSize.width: 8
                                sourceSize.height: 8
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: name
                                font.bold: uuid === root.playlistId
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        
                        onTriggered: ViewedPlaylistRouter.viewedPlaylistId = uuid
                    }
                }
            }

            // Right-click: playlist actions menu
            Menu {
                id: playlistActionsMenu
                MenuItem {
                    text: "Rename"
                    onTriggered: {
                        let dialog = renameDialog.createObject(root, {playlistId: root.playlistId})
                        dialog.open()
                    }
                }
                MenuItem {
                    text: "Clear"
                    onTriggered: controller.model?.clear()
                }
                MenuSeparator {}
                MenuItem {
                    text: "Import..."
                    onTriggered: {
                        let dialog = importDialog.createObject(root)
                        dialog.open()
                    }
                }
                MenuItem {
                    text: "Export..."
                    onTriggered: {
                        let dialog = exportDialog.createObject(root, {playlistId: root.playlistId})
                        dialog.open()
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: "Close"
                    enabled: AppViewModel.playlistTabsModel.rowCount() > 1
                    onTriggered: AppViewModel.playlistStore.closeTab(root.playlistId)
                }
            }
        }

    ListView {
        id: listView
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        interactive: false
        model: controller.model
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        // Drop indicator
        Rectangle {
            id: dropIndicator
            width: parent.width
            height: 2
            color: Theme.accent
            visible: DragManager.isDragging && DragManager.dropTargetId === root.playlistId
            y: Math.max(0, DragManager.dropTargetIndex * 18 - listView.contentY)
            z: 100
        }

        delegate: Rectangle {
            id: del
            width: listView.width
            height: 18

            required property int index
            required property string filePath
            required property string title
            required property var durationMs

            property bool selected: controller.isRowSelected(index)
            property bool playing: AppViewModel.currentIndex === index &&
                ViewedPlaylistRouter.viewedPlaylistId === ViewedPlaylistRouter.activePlaylistId

            Connections {
                target: controller
                function onSelectionChanged() { del.selected = controller.isRowSelected(del.index) }
            }

            color: selected ? Theme.selected : playing ? Theme.accentLight : ma.containsMouse ? Theme.hover : "transparent"
            opacity: DragManager.isDragging && DragManager.sourceId === root.playlistId && DragManager.draggedIndices.indexOf(index) >= 0 ? 0.4 : 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4

                Label {
                    text: del.title || del.filePath.split('/').pop()
                    color: del.playing ? Theme.accent : Theme.textPrimary
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: {
                        let ms = del.durationMs || 0
                        if (!ms) return ""
                        let s = Math.floor(ms / 1000), m = Math.floor(s / 60)
                        return m + ":" + String(s % 60).padStart(2, '0')
                    }
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    Layout.preferredWidth: 35
                    horizontalAlignment: Text.AlignRight
                }
            }

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                property point pressPos

                onPressed: function(mouse) {
                    if (mouse.button === Qt.LeftButton) pressPos = Qt.point(mouse.x, mouse.y)
                }

                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        if (!del.selected) controller.clickRow(del.index, false, false)
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
                        DragManager.setDropTarget(root.playlistId, Math.max(0, Math.min(Math.round(y / 18), listView.count)))
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

        WheelHandler {
            onWheel: (e) => listView.contentY = Math.max(0, Math.min(listView.contentHeight - listView.height, listView.contentY - e.angleDelta.y))
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

    // Background right-click
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        z: -1
        onClicked: contextMenu.popup()
    }

    // Simple context menu
    Menu {
        id: contextMenu

        MenuItem { text: "Play"; enabled: controller.selectedCount > 0; onTriggered: AppViewModel.browseActivation.activatePlaylistRow(controller.selectedRows()[0]) }
        MenuItem { text: "Remove"; enabled: controller.selectedCount > 0; onTriggered: controller.removeSelected() }
        MenuSeparator {}
        MenuItem { text: "Select all"; onTriggered: controller.selectAll() }
        MenuItem { text: "Clear playlist"; onTriggered: controller.model?.clear() }
        MenuSeparator {}
        MenuItem { 
            text: "Rename playlist"
            onTriggered: {
                let dialog = renameDialog.createObject(root, {playlistId: root.playlistId})
                dialog.open()
            }
        }
        MenuItem { 
            text: "Close playlist"
            enabled: AppViewModel.playlistTabsModel.rowCount() > 1
            onTriggered: AppViewModel.playlistStore.closeTab(root.playlistId)
        }
        MenuSeparator {}
        MenuItem { 
            text: "Import playlist..."
            onTriggered: {
                let dialog = importDialog.createObject(root)
                dialog.open()
            }
        }
        MenuItem { 
            text: "Export playlist..."
            onTriggered: {
                let dialog = exportDialog.createObject(root, {playlistId: root.playlistId})
                dialog.open()
            }
        }
        MenuSeparator {}
        MenuItem { 
            text: "New playlist"
            onTriggered: {
                let newId = AppViewModel.playlistStore.createNewTab()
                ViewedPlaylistRouter.viewedPlaylistId = newId
            }
        }

        Menu {
            title: "Switch playlist"
            Repeater {
                model: AppViewModel.playlistTabsModel
                MenuItem {
                    required property string uuid
                    required property string name
                    text: name
                    font.bold: uuid === root.playlistId
                    onTriggered: ViewedPlaylistRouter.viewedPlaylistId = uuid
                }
            }
        }
    }

    // Rename dialog
    Component {
        id: renameDialog
        Dialog {
            property string playlistId
            title: "Rename Playlist"
            modal: true
            anchors.centerIn: parent
            standardButtons: Dialog.Ok | Dialog.Cancel

            ColumnLayout {
                anchors.fill: parent
                Label { text: "New name:" }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    selectByMouse: true
                    Component.onCompleted: {
                        let idx = AppViewModel.playlistStore.indexOfUuid(playlistId)
                        if (idx >= 0) {
                            text = AppViewModel.playlistStore.tabName(idx)
                            selectAll()
                            forceActiveFocus()
                        }
                    }
                }
            }

            onAccepted: {
                if (nameField.text.trim().length > 0) {
                    AppViewModel.playlistStore.renameTab(playlistId, nameField.text.trim())
                    playlistHeader.updateHeaderTitle()
                }
            }
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
                let path = selectedFile.toString().replace("file://", "")
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
                let path = selectedFile.toString().replace("file://", "")
                AppViewModel.playlistStore.exportPlaylist(playlistId, path)
            }
        }
    }
}
