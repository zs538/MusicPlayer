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

    property string windowTitle: ""
    property bool showBreadcrumbHomeButton: true
    property var _customGroupKeys: []
    property bool _loadingSortSettings: false
    property bool _loadingSubtitleSettings: false
    property var _selectionByContext: ({})

    readonly property alias model: browserModel
    readonly property real _scrollY: gridView.contentY

    function currentSelectedEntryId() {
        return gridView && gridView.selectedIndex >= 0 ? browserModel.entryIdAt(gridView.selectedIndex) : ""
    }
    function contextKey(filter, groupBy) {
        return JSON.stringify({ filter: filter, groupBy: groupBy })
    }
    function rememberSelectionForContext(filter, groupBy, entryId) {
        const key = contextKey(filter, groupBy)
        let next = Object.assign({}, _selectionByContext)
        if (entryId && entryId.length > 0)
            next[key] = entryId
        else
            delete next[key]
        _selectionByContext = next
    }
    function rememberedSelectionForContext(filter, groupBy) {
        const key = contextKey(filter, groupBy)
        return _selectionByContext[key] || ""
    }
    function doNavigate(filter, groupBy) {
        const rememberedEntryId = rememberedSelectionForContext(filter, groupBy)
        gridView.pendingNavigateSelectionEntryId = rememberedEntryId
        gridView.selectFirstAfterNavigation = rememberedEntryId.length === 0
        browserModel.navigate(filter, groupBy, _scrollY, currentSelectedEntryId())
    }
    function doGoBack() {
        browserModel.goBack(_scrollY, currentSelectedEntryId())
    }
    function doGoForward() {
        browserModel.goForward(_scrollY, currentSelectedEntryId())
    }
    function jumpToBreadcrumb(index) {
        browserModel.jumpToBreadcrumb(index, _scrollY, currentSelectedEntryId())
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
    function setSubtitleOption(subtitleKey) {
        browserModel.subtitleKey = subtitleKey
        Settings.setGroupTypeSubtitle(root.currentContextGroupType(), browserModel.groupBy, subtitleKey)
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
            let sf = topStrip.searchField
            let searchFieldPosition = sf.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
            if (sf.activeFocus && !sf.contains(searchFieldPosition))
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

        CollectionBrowserToolbar {
            id: topStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            browserModel: root.model
            showBreadcrumbHomeButton: root.showBreadcrumbHomeButton
            customGroupKeys: root._customGroupKeys
            currentOpenAction: root._currentOpenAction
            onJumpToBreadcrumb: (index) => root.jumpToBreadcrumb(index)
            onSortOptionSelected: (key) => root.setSortOption(key)
            onSortAscendingToggled: (ascending) => root.setSortAscending(ascending)
            onSubtitleOptionSelected: (key) => root.setSubtitleOption(key)
            onSearchFilterChanged: (text) => browserModel.searchFilter = text
            onOpenActionChanged: (action) => {
                Settings.setGroupTypeOpenAction(browserModel.groupBy, action)
                root._currentOpenAction = action
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
            currentIndex: -1
            property int selectedIndex: -1
            property bool selectFirstAfterNavigation: false
            property string pendingNavigateSelectionEntryId: ""

            property int stableCellWidth: Settings.gridCellMinWidth
            property int stableCellHeight: stableCellWidth + 33

            function recalculateCellSize() {
                let cols = Math.max(1, Math.floor(width / Settings.gridCellMinWidth))
                let optimal = Math.floor(width / cols)
                stableCellWidth = Math.min(optimal, Settings.gridCellMaxWidth)
                stableCellHeight = stableCellWidth + 33
            }

            function selectIndex(index) {
                if (index < 0 || index >= count) {
                    selectedIndex = -1
                    currentIndex = -1
                    root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, "")
                    return
                }
                selectedIndex = index
                currentIndex = index
                forceActiveFocus()
                positionViewAtIndex(index, GridView.Contain)
                root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, browserModel.entryIdAt(index))
            }

            function activateDelegate(delegateItem, invertOpenAction, selectFirstOnNavigate) {
                if (!delegateItem)
                    return

                if (delegateItem.entryType === "group") {
                    let openAction = Settings.groupTypeOpenAction(delegateItem.groupType)
                    if (invertOpenAction)
                        openAction = openAction === "queueTracks" ? "openPanel" : "queueTracks"

                    if (openAction === "queueTracks") {
                        AppViewModel.browseActivation.addFilteredTracksToViewed(
                            browserModel.filter, delegateItem.groupType, delegateItem.groupValue)
                    } else {
                        selectFirstAfterNavigation = !!selectFirstOnNavigate
                        let newFilter = browserModel.filter.concat([{field: delegateItem.groupType, op: "=", value: delegateItem.groupValue}])
                        root.doNavigate(newFilter, Settings.groupTypeNextGroupBy(delegateItem.groupType))
                    }
                } else {
                    AppViewModel.browseActivation.activateCollectionEntry("t:" + delegateItem.filePath)
                }
            }

            function activateCurrentSelection(invertOpenAction) {
                if (selectedIndex < 0 || selectedIndex >= count)
                    return
                let item = currentItem
                if (!item || item.index !== selectedIndex)
                    item = itemAtIndex(selectedIndex)
                if (!item)
                    return
                activateDelegate(item, invertOpenAction, true)
            }

            function moveCurrentSelection(moveFn) {
                if (selectedIndex < 0 || selectedIndex >= count)
                    return
                moveFn()
                if (currentIndex < 0 || currentIndex >= count)
                    return
                selectIndex(currentIndex)
            }

            function shouldPreserveSelectionOnModelChange() {
                return selectFirstAfterNavigation
                    || pendingNavigateSelectionEntryId.length > 0
                    || browserModel.pendingSelectedEntryId.length > 0
            }

            function finishSelectionPreservation() {
                Qt.callLater(function() {
                    if (gridView.selectFirstAfterNavigation)
                        gridView.selectFirstAfterNavigation = false
                    if (gridView.pendingNavigateSelectionEntryId.length > 0)
                        gridView.pendingNavigateSelectionEntryId = ""
                })
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
            Connections {
                target: browserModel
                function onFilterChanged() {
                    if (!gridView.shouldPreserveSelectionOnModelChange()) {
                        gridView.clearSelection()
                        return
                    }
                    gridView.finishSelectionPreservation()
                }
                function onGroupByChanged() {
                    if (!gridView.shouldPreserveSelectionOnModelChange()) {
                        gridView.clearSelection()
                        return
                    }
                    gridView.finishSelectionPreservation()
                }
                function onCountChanged() {
                    if (gridView.pendingNavigateSelectionEntryId.length > 0) {
                        const restoreIndex = browserModel.indexOfEntryId(gridView.pendingNavigateSelectionEntryId)
                        if (restoreIndex >= 0) {
                            gridView.selectIndex(restoreIndex)
                            return
                        }
                        if (gridView.count > 0) {
                            gridView.selectIndex(0)
                            return
                        }
                    }
                    if (gridView.selectFirstAfterNavigation && gridView.count > 0) {
                        gridView.selectIndex(0)
                        return
                    }
                    if (gridView.selectFirstAfterNavigation)
                        gridView.selectFirstAfterNavigation = false
                    if (gridView.selectedIndex >= gridView.count)
                        gridView.clearSelection()
                }
                function onPendingSelectedEntryIdChanged() {
                    if (!browserModel.pendingSelectedEntryId.length)
                        return
                    const restoreIndex = browserModel.indexOfEntryId(browserModel.pendingSelectedEntryId)
                    if (restoreIndex >= 0)
                        gridView.selectIndex(restoreIndex)
                }
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
            Keys.onLeftPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.moveCurrentSelection(() => gridView.moveCurrentIndexLeft())
                event.accepted = true
            }
            Keys.onRightPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.moveCurrentSelection(() => gridView.moveCurrentIndexRight())
                event.accepted = true
            }
            Keys.onUpPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.moveCurrentSelection(() => gridView.moveCurrentIndexUp())
                event.accepted = true
            }
            Keys.onDownPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.moveCurrentSelection(() => gridView.moveCurrentIndexDown())
                event.accepted = true
            }
            Keys.onReturnPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.activateCurrentSelection((event.modifiers & Qt.ControlModifier) !== 0)
                event.accepted = true
            }
            Keys.onEnterPressed: (event) => {
                if (selectedIndex < 0)
                    return
                gridView.activateCurrentSelection((event.modifiers & Qt.ControlModifier) !== 0)
                event.accepted = true
            }
            Keys.onPressed: (event) => {
                if (event.key !== Qt.Key_Backspace || !browserModel.canGoBack)
                    return
                root.doGoBack()
                event.accepted = true
            }

            delegate: CollectionTileDelegate {
                width: gridView.stableCellWidth
                height: gridView.stableCellHeight
                selected: gridView.selectedIndex === index
                playButtonVisible: selected && entryType === "group" && Settings.generatedPlaylistsEnabled && Settings.collectionPlayButtonEnabled && !Settings.collectionSingleClickOpen

                onClicked: (idx, button) => {
                    if (button === Qt.LeftButton) {
                        gridView.forceActiveFocus()
                        if (Settings.collectionSingleClickOpen && entryType === "group") {
                            gridView.selectIndex(idx)
                            gridView.activateDelegate(gridView.itemAtIndex(idx), false, false)
                        } else {
                            gridView.selectIndex(idx)
                        }
                    } else if (button === Qt.MiddleButton) {
                        if (entryType === "group") {
                            AppViewModel.browseActivation.appendFilteredTracksToViewed(
                                browserModel.filter, groupType, groupValue)
                        } else {
                            AppViewModel.browseActivation.appendCollectionEntryToViewed("t:" + filePath)
                        }
                    }
                }
                onContextMenuRequested: (idx, eType, gType, gValue, fPath) => {
                    gridView.selectIndex(idx)
                    root._openContextMenu(eType, gType, gValue, fPath)
                }
                onDoubleClicked: (idx) => {
                    gridView.selectIndex(idx)
                    gridView.activateDelegate(gridView.itemAtIndex(idx), false, false)
                }
                onPlayButtonClicked: (gType, gValue) => {
                    AppViewModel.browseActivation.playFilteredTracksInNewPlaylist(
                        browserModel.filter, gType, gValue)
                }
            }

            function clearSelection() {
                selectedIndex = -1
                currentIndex = -1
                root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, "")
            }

            Keys.onEscapePressed: {
                clearSelection()
            }
        }
    }
}
