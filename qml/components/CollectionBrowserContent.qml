import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

// Shared collection browsing content used by both CollectionPanel and CollectionWindow.
// This component contains the top strip, grid/list views, delegates, and context menus.
// The parent component provides the panel state properties and navigation functions.

Item {
    id: root

    // Initial state (set once by parent)
    property var initialFilter: []
    property string initialGroupBy: "albumartist"

    // View options
    property bool showHeader: false
    property string viewMode: Settings.groupTypeViewMode(initialGroupBy)
    property string windowTitle: ""
    property bool expandableGroups: false
    property var expandedGroups: ({})

    property bool _loadingViewMode: false
    onViewModeChanged: { if (!_loadingViewMode) Settings.setGroupTypeViewMode(browserModel.groupBy, viewMode) }

    // Expose model for parent access
    readonly property alias model: browserModel

    // Current scroll position (whichever view is active)
    readonly property real _scrollY: viewMode === "grid" ? gridView.contentY : listView.contentY

    // Navigation helpers — pass current scroll position to model
    function doNavigate(filter, groupBy) { browserModel.navigate(filter, groupBy, _scrollY) }
    function doGoBack() { browserModel.goBack(_scrollY) }
    function doGoForward() { browserModel.goForward(_scrollY) }

    // Scroll restore after back/forward navigation (direct — no Timer needed
    // because swapEntries avoids model reset, so the view is already laid out)
    property bool _restoring: false
    Connections {
        target: browserModel
        function onPendingScrollYChanged() {
            root._restoring = true
            if (root.viewMode === "grid") gridView.contentY = browserModel.pendingScrollY
            else listView.contentY = browserModel.pendingScrollY
            root._restoring = false
        }
    }

    CollectionBrowseModel {
        id: browserModel
        database: AppViewModel.libraryDatabase
        filter: root.initialFilter
        groupBy: root.initialGroupBy
        onGroupByChanged: {
            root._loadingViewMode = true
            root.viewMode = Settings.groupTypeViewMode(browserModel.groupBy)
            root._loadingViewMode = false
            root._currentOpenAction = Settings.groupTypeOpenAction(browserModel.groupBy)
        }
    }

    // Reactive local mirror of the per-groupType open action so menu items update within the same session
    property string _currentOpenAction: Settings.groupTypeOpenAction(initialGroupBy)

    // Shared context menu state (one Menu instance instead of one per delegate)
    property string _ctxEntryType: ""
    property string _ctxGroupType: ""
    property var _ctxGroupValue: null
    property string _ctxFilePath: ""

    Menu {
        id: sharedContextMenu
        MenuItem {
            text: qsTr("Append to viewed playlist")
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.appendFilteredTracksToViewed(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + root._ctxFilePath)
            }
        }
        MenuItem {
            text: qsTr("Append after currently playing")
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.appendFilteredTracksAfterPlaying(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.appendCollectionEntryAfterPlaying("t:" + root._ctxFilePath)
            }
        }
        MenuItem {
            text: qsTr("Open in new playlist")
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.openFilteredTracksInNewPlaylist(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.openCollectionEntryInNewPlaylist("t:" + root._ctxFilePath)
            }
        }
    }

    function _openContextMenu(entryType, groupType, groupValue, filePath) {
        root._ctxEntryType = entryType
        root._ctxGroupType = groupType
        root._ctxGroupValue = groupValue
        root._ctxFilePath = filePath
        sharedContextMenu.popup()
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

                // Back button
                Button {
                    visible: browserModel.canGoBack || browserModel.canGoForward
                    enabled: browserModel.canGoBack
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    flat: true
                    opacity: enabled ? 1.0 : 0.3
                    icon.source: Qt.resolvedUrl("../icons/arrow_back.svg")
                    icon.color: Theme.textPrimary
                    icon.width: 12; icon.height: 12
                    onClicked: root.doGoBack()
                }

                // Forward button
                Button {
                    visible: browserModel.canGoBack || browserModel.canGoForward
                    enabled: browserModel.canGoForward
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    flat: true
                    opacity: enabled ? 1.0 : 0.3
                    icon.source: Qt.resolvedUrl("../icons/arrow_forward.svg")
                    icon.color: Theme.textPrimary
                    icon.width: 12; icon.height: 12
                    onClicked: root.doGoForward()
                }

                // Title on the left
                Label {
                    id: titleLabel
                    Layout.fillWidth: true
                    text: {
                        let groupLabel = browserModel.groupBy === "albumartist" ? "Artists" :
                                        browserModel.groupBy === "artist" ? "Artists" :
                                        browserModel.groupBy === "album" ? "Albums" :
                                        browserModel.groupBy === "genre" ? "Genres" :
                                        browserModel.groupBy === "year" ? "Years" :
                                        browserModel.groupBy === "none" ? "Tracks" : browserModel.groupBy
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
                    flat: true
                    icon.source: Qt.resolvedUrl("../icons/more_vert.svg")
                    icon.color: Theme.textPrimary
                    icon.width: 12; icon.height: 12
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
                                checked: browserModel.groupBy === "albumartist"
                                onTriggered: browserModel.groupBy = "albumartist"
                            }
                            MenuItem {
                                text: "Artist"
                                checkable: true
                                checked: browserModel.groupBy === "artist"
                                onTriggered: browserModel.groupBy = "artist"
                            }
                            MenuItem {
                                text: "Album"
                                checkable: true
                                checked: browserModel.groupBy === "album"
                                onTriggered: browserModel.groupBy = "album"
                            }
                            MenuItem {
                                text: "Genre"
                                checkable: true
                                checked: browserModel.groupBy === "genre"
                                onTriggered: browserModel.groupBy = "genre"
                            }
                            MenuItem {
                                text: "Year"
                                checkable: true
                                checked: browserModel.groupBy === "year"
                                onTriggered: browserModel.groupBy = "year"
                            }
                            MenuItem {
                                text: "None (Tracks)"
                                checkable: true
                                checked: browserModel.groupBy === "none"
                                onTriggered: browserModel.groupBy = "none"
                            }
                        }

                        Menu {
                            title: "On double-click"

                            ButtonGroup { id: openActionGroup }

                            MenuItem {
                                text: "Explore"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: root._currentOpenAction !== "queueTracks"
                                onTriggered: { Settings.setGroupTypeOpenAction(browserModel.groupBy, "openPanel"); root._currentOpenAction = "openPanel" }
                            }
                            MenuItem {
                                text: "Queue tracks"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: root._currentOpenAction === "queueTracks"
                                onTriggered: { Settings.setGroupTypeOpenAction(browserModel.groupBy, "queueTracks"); root._currentOpenAction = "queueTracks" }
                            }

                            MenuSeparator {}

                            MenuItem {
                                text: "Explore in new window"
                                checkable: true
                                enabled: root._currentOpenAction !== "queueTracks"
                                checked: Settings.groupTypeExploreInWindow(browserModel.groupBy)
                                onTriggered: Settings.setGroupTypeExploreInWindow(browserModel.groupBy, !Settings.groupTypeExploreInWindow(browserModel.groupBy))
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
            reuseItems: true
            cacheBuffer: 300
            
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
            Behavior on contentY { enabled: !root._restoring; NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
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

                                Image {
                                    anchors.centerIn: parent
                                    width: 32; height: 32
                                    source: gridDel.entryType === "group"
                                        ? Qt.resolvedUrl("../icons/album.svg")
                                        : Qt.resolvedUrl("../icons/music_note.svg")
                                    sourceSize: Qt.size(64, 64)
                                    fillMode: Image.PreserveAspectFit
                                    opacity: 0.3
                                    visible: coverImage.status !== Image.Ready
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
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                root._openContextMenu(gridDel.entryType, gridDel.groupType, gridDel.groupValue, gridDel.filePath)
                            } else if (mouse.button === Qt.MiddleButton) {
                                if (gridDel.entryType === "group") {
                                    AppViewModel.browseActivation.appendFilteredTracksToViewed(
                                        browserModel.filter, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + gridDel.filePath)
                                }
                            }
                        }

                        onDoubleClicked: {
                            if (gridDel.entryType === "group") {
                                let openAction = Settings.groupTypeOpenAction(gridDel.groupType)
                                if (openAction === "queueTracks") {
                                    AppViewModel.browseActivation.addFilteredTracksToViewed(
                                        browserModel.filter, gridDel.groupType, gridDel.groupValue)
                                } else if (Settings.groupTypeExploreInWindow(gridDel.groupType)) {
                                    AppViewModel.browseActivation.openCollectionGroup(
                                        {filter: browserModel.filter, groupBy: browserModel.groupBy}, gridDel.groupType, gridDel.groupValue)
                                } else {
                                    let newFilter = browserModel.filter.concat([{field: gridDel.groupType, op: "=", value: gridDel.groupValue}])
                                    root.doNavigate(newFilter, Settings.groupTypeNextGroupBy(gridDel.groupType))
                                }
                            } else {
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + gridDel.filePath)
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
            reuseItems: true
            model: browserModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 300
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Behavior on contentY { enabled: !root._restoring; NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
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
                        Image {
                            visible: listDel.isGroup && root.expandableGroups
                            source: listDel.isExpanded
                                ? Qt.resolvedUrl("../icons/expand_more.svg")
                                : Qt.resolvedUrl("../icons/chevron_right.svg")
                            sourceSize: Qt.size(24, 24)
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.6
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
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.RightButton) {
                                root._openContextMenu(listDel.entryType, listDel.groupType, listDel.groupValue, listDel.filePath)
                            } else if (mouse.button === Qt.MiddleButton) {
                                if (listDel.isGroup) {
                                    AppViewModel.browseActivation.appendFilteredTracksToViewed(
                                        browserModel.filter, listDel.groupType, listDel.groupValue)
                                } else {
                                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + listDel.filePath)
                                }
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
                                    AppViewModel.browseActivation.addFilteredTracksToViewed(
                                        browserModel.filter, listDel.groupType, listDel.groupValue)
                                } else if (Settings.groupTypeExploreInWindow(listDel.groupType)) {
                                    AppViewModel.browseActivation.openCollectionGroup(
                                        {filter: browserModel.filter, groupBy: browserModel.groupBy}, listDel.groupType, listDel.groupValue)
                                } else {
                                    let newFilter = browserModel.filter.concat([{field: listDel.groupType, op: "=", value: listDel.groupValue}])
                                    root.doNavigate(newFilter, Settings.groupTypeNextGroupBy(listDel.groupType))
                                }
                            } else if (!listDel.isGroup) {
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + listDel.filePath)
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
                            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) expandedTrackMenu.popup()
                                else if (mouse.button === Qt.MiddleButton) AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + modelData.filePath)
                            }
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

    // Mouse back/forward buttons overlay (on top of content, only catches Back/Forward)
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.BackButton) root.doGoBack()
            else if (mouse.button === Qt.ForwardButton) root.doGoForward()
        }
    }
}
