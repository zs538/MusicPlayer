import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

ApplicationWindow {
    id: root
    visible: true
    width: 1000
    height: 700
    title: qsTr("MusicPlayer")

    color: Theme.background

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
                id: leftColumn
                SplitView.preferredWidth: 260
                SplitView.minimumWidth: 120
                SplitView.maximumWidth: Math.max(120, mainSplitView.width - 120)
                spacing: 0

                // Cover panel (fixed 1:1 aspect ratio)
                CoverPanel {
                    id: coverPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: width
                    Layout.minimumHeight: 200
                    Layout.maximumHeight: root.height * 0.70
                }

                // Playlist panel (fills remaining space)
                PlaylistPanel {
                    id: playlistPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            // Right column: Collection browser (grid view)
            CollectionPanel {
                id: collectionPanel
                SplitView.fillWidth: true
                SplitView.minimumWidth: 120
            }
        }

        // Bottom: Controls strip spanning full width
        ControlsStrip {
            id: controlsStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.minimumHeight: 28
            Layout.maximumHeight: 44
            onFocusReturnRequested: {
                if (playlistPanel.focusWithinPlaylist)
                    playlistPanel.focusPlaylist()
                else
                    collectionPanel.focusBrowser()
            }
            onSettingsRequested: {
                if (settingsWindow.visible) {
                    settingsWindow.close()
                } else {
                    openSettingsWindow()
                }
            }
        }
    }


    // Settings window (modal)
    SettingsWindow {
        id: settingsWindow
    }

    function openSettingsWindow() {
        settingsWindow.resetGeometry(root)
        settingsWindow.show()
        settingsWindow.raise()
        settingsWindow.requestActivate()
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
        onActivated: openSettingsWindow()
    }

    Shortcut {
        sequence: "Tab"
        onActivated: {
            if (playlistPanel.focusWithinPlaylist) {
                collectionPanel.focusBrowser()
            } else if (collectionPanel.focusWithinBrowser) {
                playlistPanel.focusPlaylist()
            } else {
                playlistPanel.focusPlaylist()
            }
        }
    }

    Shortcut {
        sequence: "G"
        onActivated: collectionPanel.openGroupByMenu()
    }

    Shortcut {
        sequence: "S"
        onActivated: collectionPanel.openSortMenu()
    }

    Shortcut {
        sequence: "M"
        onActivated: {
            playlistPanel.focusPlaylist()
            playlistPanel.openPlaylistMenu()
        }
    }

    Shortcut {
        sequence: "N"
        onActivated: playlistPanel.createNewPlaylistAndFocus()
    }

    Shortcut {
        sequence: "R"
        onActivated: playlistPanel.startRenameViewedPlaylist()
    }

    Shortcut {
        sequence: "Ctrl+Shift+R"
        onActivated: AppViewModel.rescanLibrary()
    }

    Shortcut {
        sequence: "Q"
        onActivated: AppViewModel.stop()
    }

    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: AppViewModel.seek(Math.max(0, AppViewModel.positionMs - 5000))
    }

    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: AppViewModel.seek(Math.min(AppViewModel.durationMs, AppViewModel.positionMs + 5000))
    }

    Shortcut {
        sequence: "V"
        onActivated: coverPanel.showCurrentTrackInPlaylist()
    }
}
