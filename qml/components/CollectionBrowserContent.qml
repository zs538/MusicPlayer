import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

// Shared collection browsing content used by both CollectionPanel and CollectionWindow.
// This component contains the top strip, grid/list views, delegates, and context menus.
// The parent component provides the panel state properties and navigation functions.

Item {
    id: root

    component PointingCursor: HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    component TextCursor: HoverHandler {
        cursorShape: Qt.IBeamCursor
    }

    component StyledHoverToolTip: ToolTip {
        id: toolTip
        leftPadding: 5
        rightPadding: 5
        topPadding: 2
        bottomPadding: 2
        background: Rectangle {
            color: Theme.surfaceAlt
            border.color: "#3a3a3a"
            border.width: 1
            radius: Theme.radiusNone
        }
        contentItem: Text {
            text: toolTip.text
            color: "#3a3a3a"
            font.pixelSize: 11
        }
    }

    component TopStripIconButton: Item {
        id: control

        property url iconSource: ""
        property real iconSize: 14
        property real idleOpacity: 0.9
        property bool interactive: true
        property real horizontalInset: 0
        property string toolTipText: ""

        signal clicked()

        implicitWidth: 24 - horizontalInset
        implicitHeight: 24
        width: implicitWidth
        height: implicitHeight

        Image {
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            source: control.iconSource
            sourceSize: Qt.size(control.iconSize * 2, control.iconSize * 2)
            opacity: buttonMouseArea.pressed ? Math.max(0.4, control.idleOpacity - 0.18) : control.idleOpacity
        }

        HoverHandler {
            enabled: control.interactive
            cursorShape: Qt.PointingHandCursor
        }

        MouseArea {
            id: buttonMouseArea
            anchors.fill: parent
            enabled: control.interactive
            hoverEnabled: true
            cursorShape: control.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
            StyledHoverToolTip {
                parent: buttonMouseArea
                visible: buttonMouseArea.containsMouse && control.toolTipText.length > 0
                delay: 800
                timeout: 5000
                text: control.toolTipText
            }
            onClicked: control.clicked()
        }
    }

    // Initial state (set once by parent)
    property var initialFilter: []
    property string initialGroupBy: "albumartist"

    property string windowTitle: ""
    property bool showBreadcrumbHomeButton: true
    property var _customGroupKeys: []
    property bool _loadingSortSettings: false
    property bool _loadingSubtitleSettings: false

    readonly property alias model: browserModel
    readonly property real _scrollY: gridView.contentY

    function doNavigate(filter, groupBy) {
        browserModel.navigate(filter, groupBy, _scrollY)
    }
    function doGoBack() {
        browserModel.goBack(_scrollY)
    }
    function doGoForward() {
        browserModel.goForward(_scrollY)
    }
    function jumpToBreadcrumb(index) {
        browserModel.jumpToBreadcrumb(index, _scrollY)
    }
    function groupLabel(groupBy) {
        return groupBy.indexOf("custom:") === 0 ? "Custom: " + groupBy.slice(7) :
               groupBy === "artist" ? "Artists" :
               groupBy === "albumartist" ? "Album Artists" :
               groupBy === "album" ? "Albums" :
               groupBy === "genre" ? "Genres" :
               groupBy === "year" ? "Years" :
               groupBy === "disc" ? "Discs" :
               groupBy === "performer" ? "Performers" :
               groupBy === "composer" ? "Composers" :
               groupBy === "originalyear" ? "Original Years" :
               groupBy === "bpm" ? "BPM" :
               groupBy === "initialkey" ? "Keys" :
               groupBy === "bitrate" ? "Bitrates" :
               groupBy === "filetype" ? "File Types" :
               groupBy === "none" ? "Tracks" : groupBy
    }
    function customGroupBy(key) {
        return "custom:" + key
    }
    function contextGroupTypeForFilter(filter) {
        return filter.length > 0 ? filter[filter.length - 1].field : "all"
    }
    function currentContextGroupType() {
        return root.contextGroupTypeForFilter(browserModel.filter)
    }
    function refreshCustomGroupKeys() {
        root._customGroupKeys = AppViewModel.libraryDatabase ? AppViewModel.libraryDatabase.customTagKeys() : []
    }
    function applyStoredSortSettings(groupType, groupBy) {
        root._loadingSortSettings = true
        browserModel.sortBy = Settings.groupTypeSortBy(groupType, groupBy)
        browserModel.sortAscending = Settings.groupTypeSortAscending(groupType, groupBy)
        root._loadingSortSettings = false
    }
    function applyStoredSubtitleSettings(groupType, groupBy) {
        root._loadingSubtitleSettings = true
        browserModel.subtitleKey = Settings.groupTypeSubtitle(groupType, groupBy)
        root._loadingSubtitleSettings = false
    }
    function setSortOption(sortBy) {
        browserModel.sortBy = sortBy
        Settings.setGroupTypeSortBy(root.currentContextGroupType(), browserModel.groupBy, sortBy)
    }
    function setSortAscending(ascending) {
        browserModel.sortAscending = ascending
        Settings.setGroupTypeSortAscending(root.currentContextGroupType(), browserModel.groupBy, ascending)
    }
    function applyCurrentContextSettings() {
        const groupType = root.currentContextGroupType()
        const groupBy = browserModel.groupBy
        root.applyStoredSortSettings(groupType, groupBy)
        root.applyStoredSubtitleSettings(groupType, groupBy)
    }
    function sortOptions() {
        if (browserModel.groupBy === "none") {
            return [
                { text: "Name", key: "name" },
                { text: "Track Number", key: "trackNumber" },
                { text: "Year", key: "year" },
                { text: "Duration", key: "duration" },
                { text: "Date Updated", key: "dateUpdated" }
            ]
        }

        return [
            { text: "Name", key: "name" },
            { text: "Year", key: "year" },
            { text: "Duration", key: "duration" },
            { text: "Track Count", key: "count" },
            { text: "Date Updated", key: "dateUpdated" }
        ]
    }
    function setSubtitleOption(subtitleKey) {
        browserModel.subtitleKey = subtitleKey
        Settings.setGroupTypeSubtitle(root.currentContextGroupType(), browserModel.groupBy, subtitleKey)
    }
    function subtitleOptions() {
        if (browserModel.groupBy === "none") {
            return [
                { text: "Track", key: "trackNumber" },
                { text: "Duration", key: "duration" },
                { text: "Track - Duration", key: "trackNumberDuration" },
                { text: "Year", key: "year" },
                { text: "Date Updated", key: "dateUpdated" },
                { text: "Artist", key: "artist" },
                { text: "Album Artist", key: "albumArtist" },
                { text: "Album", key: "album" },
                { text: "Track - Album", key: "trackNumberAlbum" },
                { text: "Artist - Album", key: "artistAlbum" },
                { text: "Album Artist - Album", key: "albumArtistAlbum" },
                { text: "File Type", key: "fileType" },
                { text: "Bitrate", key: "bitrate" }
            ]
        }

        return [
            { text: "Track Count", key: "count" },
            { text: "Duration", key: "duration" },
            { text: "Tracks - Duration", key: "countDuration" },
            { text: "Album Artist", key: "albumArtist" },
            { text: "Album Artist - Year", key: "albumArtistYear" },
            { text: "Album Artist - Tracks", key: "albumArtistCount" },
            { text: "Year - Album Artist", key: "yearAlbumArtist" },
            { text: "Year", key: "year" },
            { text: "Year - Tracks", key: "yearCount" },
            { text: "Date Updated", key: "dateUpdated" }
        ]
    }

    property bool _restoring: false
    Connections {
        target: browserModel
        function onPendingScrollYChanged() {
            root._restoring = true
            gridView.contentY = browserModel.pendingScrollY
            root._restoring = false
        }
    }

    Connections {
        target: AppViewModel.libraryDatabase
        function onDatabaseChanged() {
            root.refreshCustomGroupKeys()
        }
    }

    Component.onCompleted: refreshCustomGroupKeys()

    CollectionBrowseModel {
        id: browserModel
        database: AppViewModel.libraryDatabase
        filter: root.initialFilter
        groupBy: root.initialGroupBy
        sortBy: Settings.groupTypeSortBy(root.contextGroupTypeForFilter(root.initialFilter), root.initialGroupBy)
        sortAscending: Settings.groupTypeSortAscending(root.contextGroupTypeForFilter(root.initialFilter), root.initialGroupBy)
        subtitleKey: Settings.groupTypeSubtitle(root.contextGroupTypeForFilter(root.initialFilter), root.initialGroupBy)
        onFilterChanged: root.applyCurrentContextSettings()
        onGroupByChanged: {
            Settings.setGroupTypeNextGroupBy(root.currentContextGroupType(), browserModel.groupBy)
            root.applyCurrentContextSettings()
            root._currentOpenAction = Settings.groupTypeOpenAction(browserModel.groupBy)
        }
        onSortByChanged: {
            if (!root._loadingSortSettings)
                Settings.setGroupTypeSortBy(root.currentContextGroupType(), browserModel.groupBy, browserModel.sortBy)
        }
        onSortAscendingChanged: {
            if (!root._loadingSortSettings)
                Settings.setGroupTypeSortAscending(root.currentContextGroupType(), browserModel.groupBy, browserModel.sortAscending)
        }
        onSubtitleKeyChanged: {
            if (!root._loadingSubtitleSettings)
                Settings.setGroupTypeSubtitle(root.currentContextGroupType(), browserModel.groupBy, browserModel.subtitleKey)
        }
    }

    property string _currentOpenAction: Settings.groupTypeOpenAction(initialGroupBy)

    property string _ctxEntryType: ""
    property string _ctxGroupType: ""
    property var _ctxGroupValue: null
    property string _ctxFilePath: ""

    Menu {
        id: sharedContextMenu
        MenuItem {
            text: qsTr("Append to viewed playlist")
            PointingCursor {}
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.appendFilteredTracksToViewed(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + root._ctxFilePath)
            }
        }
        MenuItem {
            text: qsTr("Append after currently playing")
            PointingCursor {}
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.appendFilteredTracksAfterPlaying(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.appendCollectionEntryAfterPlaying("t:" + root._ctxFilePath)
            }
        }
        MenuItem {
            text: qsTr("Open in new playlist")
            PointingCursor {}
            onTriggered: {
                if (root._ctxEntryType === "group")
                    AppViewModel.browseActivation.openFilteredTracksInNewPlaylist(browserModel.filter, root._ctxGroupType, root._ctxGroupValue)
                else
                    AppViewModel.browseActivation.openCollectionEntryInNewPlaylist("t:" + root._ctxFilePath)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Rescan track(s)")
            PointingCursor {}
            onTriggered: AppViewModel.rescanCollectionEntry(browserModel.filter, root._ctxEntryType,
                                                            root._ctxGroupType, root._ctxGroupValue,
                                                            root._ctxFilePath)
        }
    }

    Menu {
        id: gridBackgroundContextMenu
        MenuItem {
            text: qsTr("Queue All")
            PointingCursor {}
            onTriggered: AppViewModel.browseActivation.appendFilePathsToViewed(browserModel.displayedFilePaths())
        }
    }

    function _openContextMenu(entryType, groupType, groupValue, filePath) {
        root._ctxEntryType = entryType
        root._ctxGroupType = groupType
        root._ctxGroupValue = groupValue
        root._ctxFilePath = filePath
        sharedContextMenu.popup()
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: function(eventPoint, button) {
            let searchFieldPosition = searchField.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
            if (searchField.activeFocus && !searchField.contains(searchFieldPosition))
                root.forceActiveFocus()
        }
    }

    TapHandler {
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        acceptedDevices: PointerDevice.Mouse
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: function(eventPoint, button) {
            if (button === Qt.BackButton)
                root.doGoBack()
            else if (button === Qt.ForwardButton)
                root.doGoForward()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: topStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Theme.background

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 4

                Item {
                    id: breadcrumbContainer
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    clip: true

                    Row {
                        anchors.fill: parent
                        spacing: 0

                        TopStripIconButton {
                            visible: root.showBreadcrumbHomeButton
                            iconSource: Qt.resolvedUrl("../icons/home.svg")
                            iconSize: 14
                            horizontalInset: 2
                            toolTipText: "Root"
                            onClicked: root.jumpToBreadcrumb(0)
                        }

                        Repeater {
                            model: root.showBreadcrumbHomeButton ? Math.max(0, browserModel.breadcrumbPath.length - 1) : browserModel.breadcrumbPath.length

                            delegate: Item {
                                required property int index

                                property int breadcrumbIndex: root.showBreadcrumbHomeButton ? index + 1 : index
                                property var crumb: browserModel.breadcrumbPath[breadcrumbIndex]
                                property bool isFuture: breadcrumbIndex > browserModel.currentBreadcrumbIndex
                                property bool showSeparator: breadcrumbIndex > 0
                                width: crumbLabel.width + (showSeparator ? separatorIcon.width + 8 : 0)
                                height: 24

                                Image {
                                    id: separatorIcon
                                    visible: parent.showSeparator
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 12
                                    height: 12
                                    source: Qt.resolvedUrl("../icons/chevron_right.svg")
                                    sourceSize: Qt.size(24, 24)
                                    opacity: parent.isFuture ? 0.45 : 0.75
                                }

                                Text {
                                    id: crumbLabel
                                    anchors.left: parent.showSeparator ? separatorIcon.right : parent.left
                                    anchors.leftMargin: parent.showSeparator ? 2 : 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: parent.crumb.label || ""
                                    color: parent.isFuture ? Theme.textDisabled : Theme.textPrimary
                                    font.pixelSize: 11
                                    font.underline: crumbMouseArea.containsMouse
                                    elide: Text.ElideRight
                                }

                                HoverHandler {
                                    cursorShape: Qt.PointingHandCursor
                                }

                                MouseArea {
                                    id: crumbMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.jumpToBreadcrumb(parent.breadcrumbIndex)
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.preferredWidth: searchField.width
                    Layout.preferredHeight: 24

                    TextField {
                        id: searchField
                        width: 108
                        height: 24
                        placeholderText: "Filter..."
                        placeholderTextColor: Theme.textSecondary
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        leftPadding: 2
                        rightPadding: 18
                        topPadding: 1
                        bottomPadding: 1
                        hoverEnabled: true
                        selectByMouse: true
                        background: Item {
                            implicitWidth: 108
                            implicitHeight: 24

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: searchField.activeFocus ? "black" : "#aaaaaa"
                            }
                        }
                        onTextChanged: browserModel.searchFilter = text

                        TopStripIconButton {
                            id: searchActionButton
                            z: 1
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            anchors.verticalCenter: parent.verticalCenter
                            iconSource: searchField.text.length > 0
                                ? Qt.resolvedUrl("../icons/close.svg")
                                : Qt.resolvedUrl("../icons/search.svg")
                            iconSize: searchField.text.length > 0 ? 11 : 14
                            idleOpacity: searchField.activeFocus ? 0.9 : 0.5
                            interactive: searchField.text.length > 0
                            onClicked: searchField.clear()
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            hoverEnabled: true
                            cursorShape: Qt.IBeamCursor
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 14
                    color: Theme.border
                }

                TopStripIconButton {
                    id: sortButton
                    iconSource: Qt.resolvedUrl("../icons/sort.svg")
                    iconSize: 16
                    toolTipText: "Sort"
                    onClicked: sortMenu.popup()

                    Menu {
                        id: sortMenu

                        Instantiator {
                            model: root.sortOptions()

                            delegate: MenuItem {
                                required property var modelData

                                text: modelData.text
                                checkable: true
                                checked: browserModel.sortBy === modelData.key
                                PointingCursor {}
                                onTriggered: root.setSortOption(modelData.key)
                            }

                            onObjectAdded: function(index, object) {
                                sortMenu.insertItem(index, object)
                            }

                            onObjectRemoved: function(index, object) {
                                sortMenu.removeItem(object)
                            }
                        }

                        MenuSeparator {}
                        MenuItem {
                            text: "Ascending"
                            checkable: true
                            checked: browserModel.sortAscending
                            PointingCursor {}
                            onTriggered: root.setSortAscending(!browserModel.sortAscending)
                        }
                    }
                }

                TopStripIconButton {
                    id: groupByButton
                    iconSource: Qt.resolvedUrl("../icons/filter_alt.svg")
                    iconSize: 16
                    toolTipText: "Group By"
                    onClicked: groupByMenu.popup()

                    Menu {
                        id: groupByMenu

                        MenuItem {
                            text: "Artist"
                            checkable: true
                            checked: browserModel.groupBy === "artist"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "artist"
                        }
                        MenuItem {
                            text: "Album Artist"
                            checkable: true
                            checked: browserModel.groupBy === "albumartist"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "albumartist"
                        }
                        MenuItem {
                            text: "Album"
                            checkable: true
                            checked: browserModel.groupBy === "album"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "album"
                        }
                        MenuItem {
                            text: "Genre"
                            checkable: true
                            checked: browserModel.groupBy === "genre"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "genre"
                        }
                        MenuItem {
                            text: "Year"
                            checkable: true
                            checked: browserModel.groupBy === "year"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "year"
                        }
                        MenuItem {
                            text: "None (Tracks)"
                            checkable: true
                            checked: browserModel.groupBy === "none"
                            PointingCursor {}
                            onTriggered: browserModel.groupBy = "none"
                        }
                        Menu {
                            title: "Other"

                            MenuItem {
                                text: "Disc"
                                checkable: true
                                checked: browserModel.groupBy === "disc"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "disc"
                            }
                            MenuItem {
                                text: "Performer"
                                checkable: true
                                checked: browserModel.groupBy === "performer"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "performer"
                            }
                            MenuItem {
                                text: "Composer"
                                checkable: true
                                checked: browserModel.groupBy === "composer"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "composer"
                            }
                            MenuItem {
                                text: "Original Year"
                                checkable: true
                                checked: browserModel.groupBy === "originalyear"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "originalyear"
                            }
                            MenuItem {
                                text: "BPM"
                                checkable: true
                                checked: browserModel.groupBy === "bpm"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "bpm"
                            }
                            MenuItem {
                                text: "Initial Key"
                                checkable: true
                                checked: browserModel.groupBy === "initialkey"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "initialkey"
                            }
                            MenuItem {
                                text: "Bitrate"
                                checkable: true
                                checked: browserModel.groupBy === "bitrate"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "bitrate"
                            }
                            MenuItem {
                                text: "File Type"
                                checkable: true
                                checked: browserModel.groupBy === "filetype"
                                PointingCursor {}
                                onTriggered: browserModel.groupBy = "filetype"
                            }
                        }
                        Menu {
                            id: customTagGroupMenuStandalone
                            title: "Custom Tag"

                            MenuItem {
                                text: "No custom tags found"
                                enabled: false
                                visible: root._customGroupKeys.length === 0
                            }

                            Instantiator {
                                model: root._customGroupKeys

                                delegate: MenuItem {
                                    required property string modelData
                                    text: modelData
                                    checkable: true
                                    checked: browserModel.groupBy === root.customGroupBy(modelData)
                                    PointingCursor {}
                                    onTriggered: browserModel.groupBy = root.customGroupBy(modelData)
                                }

                                onObjectAdded: function(index, object) {
                                    customTagGroupMenuStandalone.insertItem(index, object)
                                }

                                onObjectRemoved: function(index, object) {
                                    customTagGroupMenuStandalone.removeItem(object)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 14
                    color: Theme.border
                }

                TopStripIconButton {
                    id: optionsButton
                    iconSource: Qt.resolvedUrl("../icons/more_vert.svg")
                    iconSize: 16
                    toolTipText: "Menu"
                    onClicked: optionsMenu.popup()

                    Menu {
                        id: optionsMenu
                        title: "Options"

                        Menu {
                            id: subtitleMenu
                            title: "Subtitle"

                            Instantiator {
                                model: root.subtitleOptions()

                                delegate: MenuItem {
                                    required property var modelData

                                    text: modelData.text
                                    checkable: true
                                    checked: browserModel.subtitleKey === modelData.key
                                    PointingCursor {}
                                    onTriggered: root.setSubtitleOption(modelData.key)
                                }

                                onObjectAdded: function(index, object) {
                                    subtitleMenu.insertItem(index, object)
                                }

                                onObjectRemoved: function(index, object) {
                                    subtitleMenu.removeItem(object)
                                }
                            }
                        }

                        Menu {
                            title: "On double-click"

                            ButtonGroup { id: openActionGroup }

                            MenuItem {
                                text: "Queue tracks"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: root._currentOpenAction === "queueTracks"
                                PointingCursor {}
                                onTriggered: {
                                    Settings.setGroupTypeOpenAction(browserModel.groupBy, "queueTracks")
                                    root._currentOpenAction = "queueTracks"
                                }
                            }
                            MenuItem {
                                text: "Explore"
                                ButtonGroup.group: openActionGroup
                                checkable: true
                                checked: root._currentOpenAction !== "queueTracks"
                                PointingCursor {}
                                onTriggered: {
                                    Settings.setGroupTypeOpenAction(browserModel.groupBy, "openPanel")
                                    root._currentOpenAction = "openPanel"
                                }
                            }
                        }
                    }
                }
            }
        }

        GridView {
            id: gridView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 0
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            clip: true
            interactive: false
            reuseItems: true
            cacheBuffer: 300
            focus: true

            property int stableCellWidth: Settings.gridCellMinWidth
            property int stableCellHeight: stableCellWidth + 33

            function recalculateCellSize() {
                let cols = Math.max(1, Math.floor(width / Settings.gridCellMinWidth))
                let optimal = Math.floor(width / cols)
                stableCellWidth = Math.min(optimal, Settings.gridCellMaxWidth)
                stableCellHeight = stableCellWidth + 33
            }

            Timer {
                id: resizeDebounce
                interval: 150
                onTriggered: gridView.recalculateCellSize()
            }

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
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                acceptedDevices: PointerDevice.Mouse
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: function(eventPoint, button) {
                    const contentPosition = gridView.mapToItem(gridView.contentItem, eventPoint.position.x, eventPoint.position.y)
                    if (gridView.indexAt(contentPosition.x, contentPosition.y) !== -1)
                        return
                    gridView.forceActiveFocus()
                    gridView.clearSelection()
                    if (button === Qt.RightButton)
                        gridBackgroundContextMenu.popup()
                }
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (e) => {
                    let minY = gridView.originY
                    let maxY = gridView.originY + gridView.contentHeight - gridView.height
                    gridView.contentY = Math.max(minY, Math.min(maxY, gridView.contentY - e.angleDelta.y))
                }
            }
            onContentHeightChanged: {
                let maxY = Math.max(originY, originY + contentHeight - height)
                if (contentY > maxY)
                    contentY = maxY
                if (contentY < originY)
                    contentY = originY
            }
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
                required property var coverFilePaths
                required property string imagePath
                required property string filePath

                property bool selected: false

                function select() {
                    gridView.clearSelection()
                    selected = true
                }

                function deselect() {
                    selected = false
                }

                Rectangle {
                    anchors.fill: parent
                    color: gridMouseArea.containsMouse ? "#ebebeb" : "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        anchors.topMargin: 4
                        anchors.bottomMargin: 12
                        spacing: 2

                        Rectangle {
                            id: coverContainer
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            color: Theme.surfaceAlt
                            border.color: gridDel.selected ? "#505050" : Theme.border
                            border.width: 1

                            Image {
                                id: coverImage
                                anchors.fill: parent
                                anchors.margins: 1
                                source: gridDel.imagePath
                                    ? AppViewModel.localFileUrlForPath(gridDel.imagePath)
                                    : AppViewModel.coverImageSourceForFiles(gridDel.coverFilePaths)
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
                                    width: 32
                                    height: 32
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

                        Label {
                            id: titleLabel
                            text: gridDel.displayText || ""
                            color: Theme.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft

                        }

                        Label {
                            id: subtitleLabel
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
                        cursorShape: Qt.PointingHandCursor
                        property point toolTipAnchorPos: Qt.point(0, 0)

                        function updateToolTipAnchor(x, y) {
                            toolTipAnchorPos = Qt.point(x + 8, y + 12)
                        }

                        function toolTipTextAt(x, y) {
                            let titlePos = gridMouseArea.mapFromItem(titleLabel, 0, 0)
                            if (x >= titlePos.x && x <= titlePos.x + titleLabel.width
                                    && y >= titlePos.y && y <= titlePos.y + titleLabel.height)
                                return titleLabel.text

                            let subtitlePos = gridMouseArea.mapFromItem(subtitleLabel, 0, 0)
                            if (x >= subtitlePos.x && x <= subtitlePos.x + subtitleLabel.width
                                    && y >= subtitlePos.y && y <= subtitlePos.y + subtitleLabel.height)
                                return subtitleLabel.text

                            return ""
                        }

                        readonly property string hoveredToolTipText: containsMouse ? toolTipTextAt(mouseX, mouseY) : ""

                        ToolTip {
                            id: gridToolTip
                            parent: gridMouseArea
                            visible: gridMouseArea.containsMouse && gridMouseArea.hoveredToolTipText.length > 0
                            delay: 800
                            timeout: 5000
                            text: gridMouseArea.hoveredToolTipText
                            x: Math.max(0, gridMouseArea.toolTipAnchorPos.x)
                            y: Math.max(0, gridMouseArea.toolTipAnchorPos.y)
                            leftPadding: 5
                            rightPadding: 5
                            topPadding: 2
                            bottomPadding: 2
                            background: Rectangle {
                                color: Theme.surfaceAlt
                                border.color: "#3a3a3a"
                                border.width: 1
                                radius: Theme.radiusNone
                            }
                            contentItem: Text {
                                text: gridToolTip.text
                                color: "#3a3a3a"
                                font.pixelSize: 11
                            }
                            onVisibleChanged: {
                                if (visible)
                                    gridMouseArea.updateToolTipAnchor(gridMouseArea.mouseX, gridMouseArea.mouseY)
                            }
                        }

                        onHoveredToolTipTextChanged: {
                            if (hoveredToolTipText.length > 0 && gridToolTip.visible)
                                updateToolTipAnchor(mouseX, mouseY)
                        }

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                gridView.forceActiveFocus()
                                if (Settings.collectionSingleClickOpen && gridDel.entryType === "group") {
                                    // Single click opens group directly
                                    let openAction = Settings.groupTypeOpenAction(gridDel.groupType)
                                    if (openAction === "queueTracks") {
                                        AppViewModel.browseActivation.addFilteredTracksToViewed(
                                            browserModel.filter, gridDel.groupType, gridDel.groupValue)
                                    } else {
                                        let newFilter = browserModel.filter.concat([{field: gridDel.groupType, op: "=", value: gridDel.groupValue}])
                                        root.doNavigate(newFilter, Settings.groupTypeNextGroupBy(gridDel.groupType))
                                    }
                                } else {
                                    // Normal selection behavior
                                    if (gridDel.selected) {
                                        gridDel.deselect()
                                    } else {
                                        gridDel.select()
                                    }
                                }
                            } else if (mouse.button === Qt.RightButton) {
                                gridDel.select()
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
                                } else {
                                    let newFilter = browserModel.filter.concat([{field: gridDel.groupType, op: "=", value: gridDel.groupValue}])
                                    root.doNavigate(newFilter, Settings.groupTypeNextGroupBy(gridDel.groupType))
                                }
                            } else {
                                AppViewModel.browseActivation.activateCollectionEntry("t:" + gridDel.filePath)
                            }
                        }
                    }

                    // Play button overlay - only for groups, when selected and enabled in settings
                    // Positioned over cover container, outside MouseArea to capture clicks
                    Rectangle {
                        id: playButton
                        visible: gridDel.selected && gridDel.entryType === "group" && Settings.collectionPlayButtonEnabled && !Settings.collectionSingleClickOpen
                        x: parent.width - 24
                        y: 4 + coverContainer.height - 20
                        width: 20
                        height: 20
                        color: playButtonMouseArea.pressed ? "#d0d0d0" : (playButtonMouseArea.containsMouse ? "#f0f0f0" : "#ffffff")
                        border.color: "#505050"
                        border.width: 1
                        z: 2

                        Image {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            source: Qt.resolvedUrl("../icons/play_arrow.svg")
                            sourceSize: Qt.size(20, 20)
                            fillMode: Image.PreserveAspectFit
                        }

                        MouseArea {
                            id: playButtonMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                AppViewModel.browseActivation.playFilteredTracksInNewPlaylist(
                                    browserModel.filter, gridDel.groupType, gridDel.groupValue)
                            }
                        }
                    }
                }
            }

            function clearSelection() {
                for (let i = 0; i < count; i++) {
                    let item = itemAtIndex(i)
                    if (item && item.selected)
                        item.selected = false
                }
            }

            Keys.onEscapePressed: {
                clearSelection()
            }
        }
    }
}
