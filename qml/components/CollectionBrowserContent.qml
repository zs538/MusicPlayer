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
    readonly property bool focusWithinBrowser: root.activeFocus || topStrip.searchField.activeFocus || gridView.activeFocus

    function currentSelectedEntryId() {
        return gridView && gridView.selectedIndex >= 0 ? browserModel.entryIdAt(gridView.selectedIndex) : ""
    }
    function focusBrowser() {
        gridView.forceActiveFocus()
    }
    function focusFilterField() {
        topStrip.focusSearchField()
    }
    function clearSearchFilter() {
        if (topStrip.searchField.text.length > 0)
            topStrip.searchField.clear()
    }
    function acceptFilterField() {
        if (gridView.count > 0)
            gridView.selectIndex(0, false)
        root.focusBrowser()
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
        const scrollY = _scrollY
        const selectedEntryId = currentSelectedEntryId()
        gridView.pendingNavigateSelectionEntryId = rememberedEntryId
        gridView.selectFirstAfterNavigation = rememberedEntryId.length === 0
        clearSearchFilter()
        browserModel.navigate(filter, groupBy, scrollY, selectedEntryId)
    }
    function doGoBack() {
        const scrollY = _scrollY
        const selectedEntryId = currentSelectedEntryId()
        clearSearchFilter()
        browserModel.goBack(scrollY, selectedEntryId)
    }
    function doGoForward() {
        const scrollY = _scrollY
        const selectedEntryId = currentSelectedEntryId()
        clearSearchFilter()
        browserModel.goForward(scrollY, selectedEntryId)
    }
    function jumpToBreadcrumb(index) {
        const scrollY = _scrollY
        const selectedEntryId = currentSelectedEntryId()
        clearSearchFilter()
        browserModel.jumpToBreadcrumb(index, scrollY, selectedEntryId)
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
    function playCurrentSelection() {
        if (!gridView.selectionActive || gridView.selectedIndex < 0 || gridView.selectedIndex >= browserModel.count)
            return
        let item = gridView.currentItem
        if (!item || item.index !== gridView.selectedIndex)
            item = gridView.itemAtIndex(gridView.selectedIndex)
        if (!item || item.entryType !== "group")
            return
        AppViewModel.browseActivation.playFilteredTracksInNewPlaylist(
            browserModel.filter, item.groupType, item.groupValue)
    }
    function openSortMenu() {
        topStrip.openSortMenu()
    }
    function openGroupByMenu() {
        topStrip.openGroupByMenu()
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

        Shortcut {
            sequence: "Ctrl+F"
            enabled: root.focusWithinBrowser
            onActivated: root.focusFilterField()
        }

        Shortcut {
            sequence: "/"
            enabled: root.focusWithinBrowser && !topStrip.searchField.activeFocus
            onActivated: root.focusFilterField()
        }

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
            onSearchFieldEscapePressed: root.focusBrowser()
            onSearchFieldAccepted: root.acceptFilterField()
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
            property bool selectionActive: true
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

            function selectIndex(index, focusGrid = true) {
                if (index < 0 || index >= count) {
                    selectedIndex = -1
                    currentIndex = -1
                    selectionActive = false
                    root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, "")
                    return
                }
                selectedIndex = index
                currentIndex = index
                selectionActive = true
                if (focusGrid)
                    forceActiveFocus()
                positionViewAtIndex(index, GridView.Contain)
                root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, browserModel.entryIdAt(index))
            }

            function hideSelection() {
                if (selectedIndex < 0 || selectedIndex >= count)
                    return
                selectionActive = false
                forceActiveFocus()
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
                if (!selectionActive || selectedIndex < 0 || selectedIndex >= count)
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
            Component.onCompleted: {
                recalculateCellSize()
                if (count > 0 && selectedIndex < 0) {
                    selectIndex(0)
                    hideSelection()
                }
            }

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
                        if (gridView.count > 0)
                            gridView.selectIndex(0)
                        else
                            gridView.clearSelection()
                        return
                    }
                    gridView.finishSelectionPreservation()
                }
                function onSortByChanged() {
                    if (root._loadingSortSettings)
                        return
                    gridView.selectFirstAfterNavigation = true
                    gridView.finishSelectionPreservation()
                }
                function onSortAscendingChanged() {
                    if (root._loadingSortSettings)
                        return
                    gridView.selectFirstAfterNavigation = true
                    gridView.finishSelectionPreservation()
                }
                function onCountChanged() {
                    const preserveSearchFocus = topStrip.searchField.activeFocus
                    if (gridView.pendingNavigateSelectionEntryId.length > 0) {
                        const restoreIndex = browserModel.indexOfEntryId(gridView.pendingNavigateSelectionEntryId)
                        if (restoreIndex >= 0) {
                            gridView.selectIndex(restoreIndex, !preserveSearchFocus)
                            return
                        }
                        if (gridView.count > 0) {
                            gridView.selectIndex(0, !preserveSearchFocus)
                            return
                        }
                    }
                    if (gridView.selectFirstAfterNavigation && gridView.count > 0) {
                        gridView.selectIndex(0, !preserveSearchFocus)
                        return
                    }
                    if (gridView.selectedIndex < 0 && gridView.count > 0) {
                        gridView.selectIndex(0, !preserveSearchFocus)
                        if (!preserveSearchFocus)
                            gridView.hideSelection()
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
                        gridView.selectIndex(restoreIndex, !topStrip.searchField.activeFocus)
                }
            }

            cellWidth: stableCellWidth
            cellHeight: stableCellHeight
            model: browserModel
            ScrollBar.vertical: ScrollBar { active: true; policy: ScrollBar.AsNeeded }
            Behavior on contentY { enabled: !root._restoring; NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            TapHandler {
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                onTapped: function(eventPoint, button) {
                    const contentPosition = gridView.mapToItem(gridView.contentItem, eventPoint.position.x, eventPoint.position.y)
                    if (gridView.indexAt(contentPosition.x, contentPosition.y) !== -1)
                        return
                    gridView.forceActiveFocus()
                    gridView.hideSelection()
                    if (button === Qt.RightButton)
                        gridBackgroundContextMenu.popup()
                }
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (e) => {
                    const wheelBoost = 1.35
                    let minY = gridView.originY
                    let maxY = gridView.originY + gridView.contentHeight - gridView.height
                    gridView.contentY = Math.max(minY, Math.min(maxY, gridView.contentY - e.angleDelta.y * wheelBoost))
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
                if ((event.modifiers & Qt.AltModifier) !== 0) {
                    if (!browserModel.canGoBack)
                        return
                    root.doGoBack()
                    event.accepted = true
                    return
                }
                if (selectedIndex < 0)
                    return
                gridView.moveCurrentSelection(() => gridView.moveCurrentIndexLeft())
                event.accepted = true
            }
            Keys.onRightPressed: (event) => {
                if ((event.modifiers & Qt.AltModifier) !== 0) {
                    if (!browserModel.canGoForward)
                        return
                    root.doGoForward()
                    event.accepted = true
                    return
                }
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
                if ((event.modifiers & Qt.ShiftModifier) !== 0) {
                    root.playCurrentSelection()
                    event.accepted = true
                    return
                }
                gridView.activateCurrentSelection((event.modifiers & Qt.ControlModifier) !== 0)
                event.accepted = true
            }
            Keys.onEnterPressed: (event) => {
                if (selectedIndex < 0)
                    return
                if ((event.modifiers & Qt.ShiftModifier) !== 0) {
                    root.playCurrentSelection()
                    event.accepted = true
                    return
                }
                gridView.activateCurrentSelection((event.modifiers & Qt.ControlModifier) !== 0)
                event.accepted = true
            }
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Backspace) {
                    if (!browserModel.canGoBack)
                        return
                    root.doGoBack()
                    event.accepted = true
                    return
                }
                if (event.modifiers === Qt.AltModifier) {
                    if (event.key === Qt.Key_H) {
                        if (!browserModel.canGoBack)
                            return
                        root.doGoBack()
                        event.accepted = true
                    } else if (event.key === Qt.Key_L) {
                        if (!browserModel.canGoForward)
                            return
                        root.doGoForward()
                        event.accepted = true
                    }
                    return
                }
                if (event.modifiers !== Qt.NoModifier)
                    return
                if (event.key === Qt.Key_G) {
                    root.openGroupByMenu()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_S) {
                    root.openSortMenu()
                    event.accepted = true
                    return
                }
                if (selectedIndex < 0)
                    return
                if (event.key === Qt.Key_H) {
                    gridView.moveCurrentSelection(() => gridView.moveCurrentIndexLeft())
                    event.accepted = true
                } else if (event.key === Qt.Key_L) {
                    gridView.moveCurrentSelection(() => gridView.moveCurrentIndexRight())
                    event.accepted = true
                } else if (event.key === Qt.Key_K) {
                    gridView.moveCurrentSelection(() => gridView.moveCurrentIndexUp())
                    event.accepted = true
                } else if (event.key === Qt.Key_J) {
                    gridView.moveCurrentSelection(() => gridView.moveCurrentIndexDown())
                    event.accepted = true
                }
            }

            delegate: CollectionTileDelegate {
                width: gridView.stableCellWidth
                height: gridView.stableCellHeight
                selected: gridView.selectionActive && gridView.selectedIndex === index
                playButtonVisible: selected && entryType === "group" && Settings.generatedPlaylistsEnabled && Settings.collectionPlayButtonEnabled && !Settings.collectionSingleClickOpen

                onClicked: (idx, button) => {
                    if (button === Qt.LeftButton) {
                        gridView.forceActiveFocus()
                        if (Settings.collectionSingleClickOpen && entryType === "group") {
                            gridView.selectIndex(idx)
                            gridView.activateDelegate(gridView.itemAtIndex(idx), false, false)
                        } else if (!gridView.selectionActive) {
                            gridView.selectIndex(idx)
                        } else if (gridView.selectedIndex === idx) {
                            gridView.hideSelection()
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
                selectionActive = false
                root.rememberSelectionForContext(browserModel.filter, browserModel.groupBy, "")
            }

            Keys.onEscapePressed: {
                hideSelection()
            }
        }
    }
}
