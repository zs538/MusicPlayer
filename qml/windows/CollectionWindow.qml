import QtQuick
import QtQuick.Window
import MusicPlayer

// CollectionWindow - Floating window for collection browsing.

Window {
    id: root

    property string windowId: ""
    property var initFilter: []
    property string initGroupBy: "none"

    width: 350
    height: 400
    visible: true
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: root.title
    color: Theme.background

    onClosing: {
        if (root.windowId) WindowManager.closeWindow(root.windowId)
    }

    Connections {
        target: WindowManager
        function onWindowClosed(wid) { if (wid === root.windowId) root.destroy() }
    }

    CollectionBrowserContent {
        anchors.fill: parent
        initialFilter: root.initFilter
        initialGroupBy: root.initGroupBy
        windowTitle: root.title
        showBreadcrumbHomeButton: false
    }
}
