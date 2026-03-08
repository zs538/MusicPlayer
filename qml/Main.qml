import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MusicPlayer

ApplicationWindow {
    id: root
    visible: true
    width: 1000
    height: 700
    title: qsTr("MusicPlayer")

    color: Theme.background

    Connections {
        target: AppViewModel.browseActivation
        function onOpenCollectionPanelRequested(panelState) {
            // Open a new collection window via WindowManager
            WindowManager.openCollectionWindow(panelState)
        }
    }

    Connections {
        target: WindowManager
        function onWindowOpened(windowId, panelState) {
            collectionWindowComponent.createObject(root, {
                windowId: windowId,
                initFilter: panelState.filter || [],
                initGroupBy: panelState.groupBy || "none",
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
            id: mainSplitView
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Item {
                implicitWidth: 10
                implicitHeight: parent ? parent.height : 0

                HoverHandler {
                    cursorShape: Qt.SizeHorCursor
                }

                Rectangle {
                    id: splitHandleLine
                    anchors.centerIn: parent
                    width: 1
                    height: parent.height
                    color: SplitHandle.pressed ? Theme.textPrimary : Theme.border

                    containmentMask: Item {
                        x: (splitHandleLine.width - width) / 2
                        width: 12
                        height: mainSplitView.height
                    }
                }
            }

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
            onSettingsRequested: {
                settingsWindow.show()
                settingsWindow.raise()
                settingsWindow.requestActivate()
            }
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
        onActivated: {
            settingsWindow.show()
            settingsWindow.raise()
            settingsWindow.requestActivate()
        }
    }
}
