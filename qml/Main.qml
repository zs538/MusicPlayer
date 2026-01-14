import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MusicPlayer

ApplicationWindow {
    id: root
    visible: true
    width: SessionManager.windowGeometry.width || 1000
    height: SessionManager.windowGeometry.height || 700
    x: SessionManager.windowGeometry.x || 100
    y: SessionManager.windowGeometry.y || 100
    title: qsTr("MusicPlayer")

    color: Theme.background

    Component.onCompleted: {
        // Restore floating windows from session
        let windows = SessionManager.floatingWindows
        if (windows && windows.length > 0) {
            WindowManager.restoreWindowsFromVariant(windows)
        }
    }

    onXChanged: saveGeometryTimer.restart()
    onYChanged: saveGeometryTimer.restart()
    onWidthChanged: saveGeometryTimer.restart()
    onHeightChanged: saveGeometryTimer.restart()

    Timer {
        id: saveGeometryTimer
        interval: 500
        onTriggered: {
            SessionManager.setWindowGeometry(root.x, root.y, root.width, root.height)
        }
    }

    onClosing: {
        // Save floating windows to session
        SessionManager.floatingWindows = WindowManager.windowsToVariant()
    }

    Connections {
        target: AppViewModel.browseActivation
        function onOpenCollectionPanelRequested(panelState) {
            // Open a new collection window via WindowManager
            WindowManager.openCollectionWindow(panelState)
        }
        function onReplaceCollectionPanelRequested(panelState) {
            // Replace the main collection panel state
            collectionPanel.panelContextType = panelState.panelContextType || "all"
            collectionPanel.filter = panelState.filter || []
            collectionPanel.groupBy = panelState.groupBy || "none"
        }
    }

    Connections {
        target: WindowManager
        function onWindowOpened(windowId, panelState) {
            collectionWindowComponent.createObject(root, {
                windowId: windowId,
                panelContextType: panelState.panelContextType || "all",
                filter: panelState.filter || [],
                groupBy: panelState.groupBy || "none",
                title: panelState.title || "Collection"
            })
        }
    }

    Component {
        id: collectionWindowComponent
        CollectionWindow {}
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Main content area: 2-column layout
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // Left column: Cover (top, fixed 1:1) + Playlist (bottom)
            ColumnLayout {
                SplitView.preferredWidth: 260
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 340
                spacing: 0

                // Cover panel (fixed 1:1 aspect ratio)
                CoverPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width
                    Layout.minimumHeight: 200
                    Layout.maximumHeight: 340
                }

                // Playlist panel (fills remaining space)
                PlaylistPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            // Right column: Collection browser (grid view)
            CollectionPanel {
                id: collectionPanel
                SplitView.fillWidth: true
                SplitView.minimumWidth: 300
            }
        }

        // Bottom: Controls strip spanning full width
        ControlsStrip {
            id: controlsStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.minimumHeight: 28
            Layout.maximumHeight: 44
            onSettingsRequested: settingsWindow.show()
        }
    }

    // Settings window (modal)
    SettingsWindow {
        id: settingsWindow
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (AppViewModel.playbackState === AppViewModel.Playing)
                AppViewModel.pause()
            else
                AppViewModel.play()
        }
    }

    Shortcut {
        sequence: "Ctrl+,"
        onActivated: settingsWindow.show()
    }
}
