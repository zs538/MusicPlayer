import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MusicPlayer

// CollectionWindow - Floating window for collection browsing.
// Uses shared CollectionBrowserContent component for the actual UI.

Window {
    id: root

    // Window identity (for WindowManager tracking)
    property string windowId: ""

    // Filter-based state properties
    property string panelContextType: "all"
    property var filter: []
    property string groupBy: "none"

    // Single-level back navigation
    property var previousState: null

    function navigateTo(newFilter, newGroupBy) {
        root.previousState = {filter: root.filter, groupBy: root.groupBy}
        root.filter = newFilter
        root.groupBy = newGroupBy
    }

    function goBack() {
        if (!root.previousState) return
        root.filter = root.previousState.filter
        root.groupBy = root.previousState.groupBy
        root.previousState = null
    }

    readonly property var panelState: ({
        panelContextType: root.panelContextType,
        filter: root.filter,
        groupBy: root.groupBy
    })

    width: 350
    height: 400
    visible: true
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: root.title
    color: Theme.background

    // Geometry persistence
    onXChanged: geometrySaveTimer.restart()
    onYChanged: geometrySaveTimer.restart()
    onWidthChanged: geometrySaveTimer.restart()
    onHeightChanged: geometrySaveTimer.restart()

    Timer {
        id: geometrySaveTimer
        interval: 300
        onTriggered: {
            if (root.windowId)
                WindowManager.updateWindowGeometry(root.windowId, root.x, root.y, root.width, root.height)
        }
    }

    onClosing: {
        if (root.windowId) WindowManager.closeWindow(root.windowId)
    }

    Connections {
        target: WindowManager
        function onWindowClosed(wid) { if (wid === root.windowId) root.destroy() }
    }

    CollectionBrowserContent {
        id: content
        anchors.fill: parent

        panelContextType: root.panelContextType
        filter: root.filter
        groupBy: root.groupBy
        panelState: root.panelState
        canGoBack: root.previousState !== null
        windowTitle: root.title
        viewMode: "list"  // Default to list view for windows

        onBackRequested: root.goBack()
        onNavigateRequested: (newFilter, newGroupBy) => root.navigateTo(newFilter, newGroupBy)
    }
}
