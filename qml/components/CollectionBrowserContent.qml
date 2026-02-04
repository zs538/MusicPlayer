import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

// Shared collection browsing content used by both CollectionPanel and CollectionWindow.
// This component contains the top strip, grid/list views, delegates, and context menus.
// The parent component provides the panel state properties and navigation functions.

Item {
    id: root

    // Required properties from parent (panel state)
    required property string panelContextType
    required property var filter
    required property string groupBy
    required property var panelState  // {panelContextType, filter, groupBy}

    // Optional properties with defaults
    property bool showHeader: false
    property string viewMode: Settings.groupTypeViewMode(groupBy)  // "grid", "list", "tracks"
    property bool canGoBack: false
    property string windowTitle: ""  // For window mode, prepended to title
    
    // Persist view mode changes to Settings
    onViewModeChanged: Settings.setGroupTypeViewMode(groupBy, viewMode)
    
    // Update view mode when groupBy changes (load from settings)
    onGroupByChanged: viewMode = Settings.groupTypeViewMode(groupBy)

    // Expandable groups (window mode feature)
    property bool expandableGroups: false
    property var expandedGroups: ({})

    // Signals for navigation (parent handles these)
    signal backRequested()
    signal navigateRequested(var newFilter, string newGroupBy)

    // Expose model for parent access
    readonly property alias model: browserModel

    CollectionBrowseModel {
        id: browserModel
        database: AppViewModel.libraryDatabase
        filter: root.filter
        groupBy: root.groupBy
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top strip (title, options, sort, search)
        Rectangle {
            id: topStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Theme.surfaceAlt

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 4

                // Back button (visible when we have navigation history)
                Button {
                    visible: root.canGoBack
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    text: "◀"
                    font.pixelSize: 10
                    flat: true
                    onClicked: root.backRequested()
                }

                // Title on the left
                Label {
                    id: titleLabel
                    Layout.fillWidth: true
                    text: {
                        let groupLabel = root.groupBy === "albumartist" ? "Artists" :
                                        root.groupBy === "artist" ? "Artists" :
                                        root.groupBy === "album" ? "Albums" :
                                        root.groupBy === "genre" ? "Genres" :
                                        root.groupBy === "year" ? "Years" :
                                        root.groupBy === "none" ? "Tracks" : root.groupBy
                        let prefix = root.windowTitle ? (root.windowTitle + " - ") : ""
                        return prefix + groupLabel + " (" + browserModel.count + ")"
                    }
                    font.bold: true
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                // Search filter
                TextField {
                    id: searchField
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 20
                    placeholderText: "Filter..."
                    font.pixelSize: 10
                    leftPadding: 4
                    rightPadding: 4
                    topPadding: 2
                    bottomPadding: 2
                    onTextChanged: browserModel.searchFilter = text
                }

                // Sort button
                Button {
                    id: sortButton
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 20
                    text: browserModel.sortBy === "name" ? "A-Z" :
                          browserModel.sortBy === "year" ? "Year" :
                          browserModel.sortBy === "count" ? "Count" : "Sort"
                    font.pixelSize: 10
                    flat: true
                    onClicked: sortMenu.popup()

                    Menu {
                        id: sortMenu
                        MenuItem {
                            text: "Name (A-Z)"
                            checkable: true
                            checked: browserModel.sortBy === "name" && browserModel.sortAscending
                            onTriggered: { browserModel.sortBy = "name"; browserModel.sortAscending = true }
                        }
                        MenuItem {
                            text: "Name (Z-A)"
                            checkable: true
                            checked: browserModel.sortBy === "name" && !browserModel.sortAscending
                            onTriggered: { browserModel.sortBy = "name"; browserModel.sortAscending = false }
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: "Year (Oldest)"
                            checkable: true
                            checked: browserModel.sortBy === "year" && browserModel.sortAscending
                            onTriggered: { browserModel.sortBy = "year"; browserModel.sortAscending = true }
                        }
                        MenuItem {
                            text: "Year (Newest)"
                            checkable: true
                            checked: browserModel.sortBy === "year" && !browserModel.sortAscending
                            onTriggered: { browserModel.sortBy = "year"; browserModel.sortAscending = false }
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: "Track Count"
                            checkable: true
                            checked: browserModel.sortBy === "count"
                            onTriggered: { browserModel.sortBy = "count"; browserModel.sortAscending = false }
                        }
                    }
                }

                // Options button
                Button {
                    id: optionsButton
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 20
                    text: "..."
                    font.pixelSize: 10
                    flat: true
                    onClicked: optionsMenu.popup()

                    Menu {
                        id: optionsMenu
                        title: "Options"

                        MenuItem {
                            text: "Show header"
                            checkable: true
                            checked: root.showHeader
                            onTriggered: root.showHeader = !root.showHeader
                        }

                        MenuItem {
                            visible: root.viewMode === "list"
                            text: "Expandable groups"
                            checkable: true
                            checked: root.expandableGroups
                            onTriggered: root.expandableGroups = !root.expandableGroups
                        }

                        MenuSeparator {}

                        Menu {
                            title: "View"

                            ButtonGroup { id: viewModeGroup }

                            MenuItem {
                                text: "Grid"
                                ButtonGroup.group: viewModeGroup
                                checkable: true
                                checked: root.viewMode === "grid"
                                onTriggered: root.viewMode = "grid"
                            }
                            MenuItem {
                                text: "List"
                                ButtonGroup.group: viewModeGroup
                                checkable: true
                                checked: root.viewMode === "list"
                                onTriggered: root.viewMode = "list"
                            }
                            MenuItem {
                                text: "Tracks"
                                ButtonGroup.group: viewModeGroup
                                checkable: true
                                checked: root.viewMode === "tracks"
                                onTriggered: root.viewMode = "tracks"
                            }
                        }

                        Menu {
                            title: "Group by"
                            MenuItem {
                                text: "Album Artist"
                                checkable: true
                                checked: root.groupBy === "albumartist"
                                onTriggered: root.navigateRequested(root.filter, "albumartist")
                            }
                            MenuItem {
                                text: "Artist"
                                checkable: true
                                checked: root.groupBy === "artist"
                                onTriggered: root.navigateRequested(root.filter, "artist")
                            }
                            MenuItem {
                                text: "Album"
                                checkable: true
                                checked: root.groupBy === "album"
                                onTriggered: root.navigateRequested(root.filter, "album")
                            }
                            MenuItem {
                                text: "Genre"
                                checkable: true
                                checked: root.groupBy === "genre"
                                onTriggered: root.navigateRequested(root.filter, "genre")
                            }
                            MenuItem {
                                text: "Year"
                                checkable: true
                                checked: root.groupBy === "year"
                                onTriggered: root.navigateRequested(root.filter, "year")
                            }
                            MenuItem {
                                text: "None (Tracks)"
                                checkable: true
                                checked: root.groupBy === "none"
                                onTriggered: root.navigateRequested(root.filter, "none")
                            }
                        }

                        Menu {
                            title: "On double-click"

                            ButtonGroup { id: openActionGroup }

                            MenuItem {
                                text: "Further explore"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: Settings.groupTypeOpenAction(root.groupBy) !== "queueTracks"
                                onTriggered: Settings.setGroupTypeOpenAction(root.groupBy, "openPanel")
                            }
                            MenuItem {
                                text: "Queue tracks"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: Settings.groupTypeOpenAction(root.groupBy) === "queueTracks"
                                onTriggered: Settings.setGroupTypeOpenAction(root.groupBy, "queueTracks")
                            }
                        }
                    }
                }
            }
        }

        // Header (optional, toggleable)
        Rectangle {
            visible: root.showHeader
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Theme.surface

            Label {
                anchors.fill: parent
                anchors.leftMargin: 8
                verticalAlignment: Text.AlignVCenter
                text: root.windowTitle || browserModel.title
                font.bold: true
                font.pixelSize: 12
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }

        // Collection grid view (album covers)
        GridView {
            id: gridView
            visible: root.viewMode === "grid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 4
            clip: true
            interactive: false
            cacheBuffer: 500
            
            // Debounced cell sizing - recalculates after resize stops to avoid per-frame re-layout
            property int stableCellWidth: Settings.gridCellMinWidth
            property int stableCellHeight: Math.round(stableCellWidth * 1.25)
            
            function recalculateCellSize() {
                let cols = Math.max(1, Math.floor(width / Settings.gridCellMinWidth))
                let optimal = Math.floor(width / cols)
                stableCellWidth = Math.min(optimal, Settings.gridCellMaxWidth)
                stableCellHeight = Math.round(stableCellWidth * 1.25)
            }
            
            Timer { id: resizeDebounce; interval: 150; onTriggered: gridView.recalculateCellSize() }
            onWidthChanged: resizeDebounce.restart()
            Component.onCompleted: recalculateCellSize()
            Connections {
                target: Settings
                function onGridCellMinWidthChanged() { gridView.recalculateCellSize() }
                function onGridCellMaxWidthChanged() { gridView.recalculateCellSize() }
            }
            
            cellWidth: stableCellWidth
            cellHeight: stableCellHeight

            model: browserModel

            ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
            Behavior on contentY { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (e) => {
                    let minY = gridView.originY
                    let maxY = gridView.originY + gridView.contentHeight - gridView.height
                    gridView.contentY = Math.max(minY, Math.min(maxY, gridView.contentY - e.angleDelta.y))
                }
            }
            // Clamp contentY to valid bounds [originY, max(originY, originY + contentHeight - height)]
            // When content fits in view (contentHeight <= height), maxY = originY (no scrolling needed)
            onContentHeightChanged: {
                let maxY = Math.max(originY, originY + contentHeight - height)
                if (contentY > maxY) contentY = maxY
                if (contentY < originY) contentY = originY
            }
            
            // Reset scroll position to top when view becomes visible
            onVisibleChanged: if (visible) contentY = originY

            delegate: Item {
                id: gridDel
                width: gridView.stableCellWidth
                height: gridView.stableCellHeight

                required property int index
                required property string entryType
                required property string groupType
                required property var groupValue
                required property string displayText
                required property string subtitle
                required property string representativeFilePath
                required property string imagePath
                required property string filePath

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    color: gridMouseArea.containsMouse ? Theme.hover : "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 2

                        // Cover art
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            color: Theme.surfaceAlt
                            border.color: Theme.border
                            border.width: 1

                            Image {
                                id: coverImage
                                anchors.fill: parent
                                anchors.margins: 1
                                source: gridDel.imagePath ? ("file://" + gridDel.imagePath) : ("image://cover/" + (gridDel.representativeFilePath || gridDel.filePath || ""))
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                                sourceSize.width: 512
                                sourceSize.height: 512
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)

                                Label {
                                    anchors.centerIn: parent
                                    text: gridDel.entryType === "group" ? "💿" : "🎵"
                                    font.pixelSize: 32
                                    color: Theme.textMuted
                                    visible: parent.status !== Image.Ready
                                }
                            }
                        }

                        // Primary label (album/artist name)
                        Label {
                            text: gridDel.displayText || ""
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }

                        // Secondary label (artist or item count)
                        Label {
                            text: gridDel.subtitle || ""
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }
                    }

                    MouseArea {
                        id: gridMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                gridContextMenu.popup()
                            }
                        }

                        onDoubleClicked: {
                            if (gridDel.entryType === "group") {
                                let openAction = Settings.groupTypeOpenAction(gridDel.groupType)
                                if (openAction === "queueTracks") {
                                    // Queue tracks from this group
                                    AppViewModel.browseActivation.addFilteredTracksToViewed(
                                        root.filter, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    // Further explore - open in new panel
                                    AppViewModel.browseActivation.openCollectionGroup(
                                        root.panelState, gridDel.groupType, gridDel.groupValue)
                                }
                            } else {
                                // Track - always queue it
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + gridDel.filePath)
                            }
                        }
                    }

                    Menu {
                        id: gridContextMenu

                        MenuItem {
                            text: qsTr("Append to viewed playlist")
                            onTriggered: {
                                if (gridDel.entryType === "group") {
                                    AppViewModel.browseActivation.appendFilteredTracksToViewed(
                                        root.filter, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + gridDel.filePath)
                                }
                            }
                        }

                        MenuItem {
                            text: qsTr("Append after currently playing")
                            onTriggered: {
                                if (gridDel.entryType === "group") {
                                    AppViewModel.browseActivation.appendFilteredTracksAfterPlaying(
                                        root.filter, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryAfterPlaying("t:" + gridDel.filePath)
                                }
                            }
                        }

                        MenuItem {
                            text: qsTr("Open in new playlist")
                            onTriggered: {
                                if (gridDel.entryType === "group") {
                                    AppViewModel.browseActivation.openFilteredTracksInNewPlaylist(
                                        root.filter, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.openCollectionEntryInNewPlaylist("t:" + gridDel.filePath)
                                }
                            }
                        }
                    }
                }
            }
            
        }

        // List view (for list/tracks mode)
        ListView {
            id: listView
            visible: root.viewMode === "list" || root.viewMode === "tracks"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            interactive: false
            model: browserModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 300
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Behavior on contentY { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (e) => listView.contentY = Math.max(0, Math.min(listView.contentHeight - listView.height, listView.contentY - e.angleDelta.y))
            }

            delegate: Column {
                id: listDel
                width: listView.width

                required property int index
                required property string entryType
                required property string groupType
                required property var groupValue
                required property string displayText
                required property string representativeFilePath
                required property string imagePath
                required property string filePath
                required property var durationMs
                required property int trackNumber
                required property int childCount

                property bool isGroup: entryType === "group"
                property string groupKey: String(listDel.groupValue)
                property bool isExpanded: root.expandableGroups && root.expandedGroups[groupKey] === true

                // Group/Track row
                Rectangle {
                    width: listDel.width
                    height: listDel.isGroup ? 40 : 20
                    color: listMouseArea.containsMouse ? Theme.hover : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: listDel.isGroup ? 4 : 24
                        anchors.rightMargin: 8
                        spacing: 6

                        // Expand indicator for groups (when expandable mode is on)
                        Label {
                            visible: listDel.isGroup && root.expandableGroups
                            text: listDel.isExpanded ? "▼" : "▶"
                            color: Theme.textSecondary
                            font.pixelSize: 10
                            Layout.preferredWidth: 12
                        }

                        // Cover art for groups
                        Rectangle {
                            visible: listDel.isGroup
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            color: Theme.surfaceAlt
                            border.color: Theme.border

                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: listDel.imagePath ? ("file://" + listDel.imagePath) : ((listDel.representativeFilePath || listDel.filePath) ? "image://cover/" + (listDel.representativeFilePath || listDel.filePath) : "")
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                                sourceSize.width: 128
                                sourceSize.height: 128
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                            }
                        }

                        // Track number
                        Label {
                            visible: !listDel.isGroup && listDel.trackNumber > 0
                            text: String(listDel.trackNumber).padStart(2, '0')
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            Layout.preferredWidth: 20
                        }

                        // Title
                        Label {
                            text: listDel.displayText
                            color: Theme.textPrimary
                            font.pixelSize: listDel.isGroup ? 12 : 11
                            font.bold: listDel.isGroup
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Track count or duration
                        Label {
                            text: listDel.isGroup ? (listDel.childCount + " tracks") : formatDuration(listDel.durationMs)
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            Layout.preferredWidth: 50
                            horizontalAlignment: Text.AlignRight

                            function formatDuration(ms) {
                                if (!ms) return ""
                                let s = Math.floor(ms / 1000), m = Math.floor(s / 60)
                                return m + ":" + String(s % 60).padStart(2, '0')
                            }
                        }
                    }

                    MouseArea {
                        id: listMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                listContextMenu.popup()
                            } else if (listDel.isGroup && root.expandableGroups) {
                                // Toggle expansion
                                let newExpanded = Object.assign({}, root.expandedGroups)
                                newExpanded[listDel.groupKey] = !listDel.isExpanded
                                root.expandedGroups = newExpanded
                            }
                        }
                        onDoubleClicked: {
                            if (listDel.isGroup && !root.expandableGroups) {
                                let openAction = Settings.groupTypeOpenAction(listDel.groupType)
                                if (openAction === "queueTracks") {
                                    // Queue tracks from this group
                                    AppViewModel.browseActivation.addFilteredTracksToViewed(
                                        root.filter, listDel.groupType, listDel.groupValue)
                                } else {
                                    // Further explore - open in new panel
                                    AppViewModel.browseActivation.openCollectionGroup(
                                        root.panelState, listDel.groupType, listDel.groupValue)
                                }
                            } else if (!listDel.isGroup) {
                                // Track - always queue it
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + listDel.filePath)
                            }
                        }
                    }

                    Menu {
                        id: listContextMenu
                        MenuItem {
                            text: "Append to viewed playlist"
                            onTriggered: {
                                if (listDel.isGroup) {
                                    AppViewModel.browseActivation.appendFilteredTracksToViewed(
                                        root.filter, listDel.groupType, listDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + listDel.filePath)
                                }
                            }
                        }
                        MenuItem {
                            text: "Append after currently playing"
                            onTriggered: {
                                if (listDel.isGroup) {
                                    AppViewModel.browseActivation.appendFilteredTracksAfterPlaying(
                                        root.filter, listDel.groupType, listDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryAfterPlaying("t:" + listDel.filePath)
                                }
                            }
                        }
                        MenuItem {
                            text: "Open in new playlist"
                            onTriggered: {
                                if (listDel.isGroup) {
                                    AppViewModel.browseActivation.openFilteredTracksInNewPlaylist(
                                        root.filter, listDel.groupType, listDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.openCollectionEntryInNewPlaylist("t:" + listDel.filePath)
                                }
                            }
                        }
                    }
                }

                // Expanded tracks (only for groups when expanded)
                Repeater {
                    model: listDel.isGroup && listDel.isExpanded ? browserModel.tracksForGroup(listDel.groupType, listDel.groupValue) : []

                    Rectangle {
                        width: listDel.width
                        height: 18
                        color: expandedTrackMa.containsMouse ? Theme.hover : "transparent"

                        required property var modelData
                        required property int index

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 48
                            anchors.rightMargin: 8
                            spacing: 4

                            Label {
                                text: modelData.trackNumber > 0 ? String(modelData.trackNumber).padStart(2, '0') : ""
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                Layout.preferredWidth: 18
                            }

                            Label {
                                text: modelData.title || modelData.filePath.split('/').pop()
                                color: Theme.textPrimary
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: {
                                    let ms = modelData.durationMs
                                    if (!ms) return ""
                                    let s = Math.floor(ms / 1000), m = Math.floor(s / 60)
                                    return m + ":" + String(s % 60).padStart(2, '0')
                                }
                                color: Theme.textSecondary
                                font.pixelSize: 10
                                Layout.preferredWidth: 40
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        MouseArea {
                            id: expandedTrackMa
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton

                            onClicked: (mouse) => { if (mouse.button === Qt.RightButton) expandedTrackMenu.popup() }
                            onDoubleClicked: {
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + modelData.filePath)
                            }
                        }

                        Menu {
                            id: expandedTrackMenu
                            MenuItem {
                                text: "Append to viewed playlist"
                                onTriggered: AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + modelData.filePath)
                            }
                            MenuItem {
                                text: "Append after currently playing"
                                onTriggered: AppViewModel.browseActivation.appendCollectionEntryAfterPlaying("t:" + modelData.filePath)
                            }
                            MenuItem {
                                text: "Open in new playlist"
                                onTriggered: AppViewModel.browseActivation.openCollectionEntryInNewPlaylist("t:" + modelData.filePath)
                            }
                        }
                    }
                }
            }

        }
    }
}
