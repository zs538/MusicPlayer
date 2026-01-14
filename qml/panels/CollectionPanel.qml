import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

// CollectionPanel - Main collection browser panel for the fixed layout.
// Uses shared CollectionBrowserContent component for the actual UI.

Rectangle {
    id: root
    color: Theme.background

    // Panel state properties (for filter-based browsing)
    property string panelContextType: "all"
    property var filter: []
    property string groupBy: Settings.groupTypeNextGroupBy(panelContextType)

    // Single-level back navigation for "replace current panel" mode
    property var previousState: null  // {filter, groupBy} or null

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

    // Computed panel state for passing to activation service
    readonly property var panelState: ({
        panelContextType: root.panelContextType,
        filter: root.filter,
        groupBy: root.groupBy
    })

    CollectionBrowserContent {
        id: content
        anchors.fill: parent

        panelContextType: root.panelContextType
        filter: root.filter
        groupBy: root.groupBy
        panelState: root.panelState
        canGoBack: root.previousState !== null

        onBackRequested: root.goBack()
        onNavigateRequested: (newFilter, newGroupBy) => root.navigateTo(newFilter, newGroupBy)
    }
}
