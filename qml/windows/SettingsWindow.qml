import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MusicPlayer

Window {
    id: root
    
    width: 500
    height: 400
    title: qsTr("Settings")
    modality: Qt.ApplicationModal
    flags: Qt.Dialog

    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Label {
            text: qsTr("Settings")
            font.bold: true
            font.pixelSize: 18
            color: Theme.textPrimary
        }

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton { text: qsTr("Playback") }
            TabButton { text: qsTr("Browse") }
            TabButton { text: qsTr("Library") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // Playback settings
            ColumnLayout {
                spacing: 12

                GroupBox {
                    title: qsTr("Playback Mode")
                    Layout.fillWidth: true

                    ColumnLayout {
                        RadioButton {
                            text: qsTr("Gapless Session")
                            checked: Settings.playbackMode === 0
                            onClicked: Settings.playbackMode = 0
                        }
                        RadioButton {
                            text: qsTr("Bit-Perfect (Same Rate)")
                            checked: Settings.playbackMode === 1
                            onClicked: Settings.playbackMode = 1
                        }
                    }
                }

                GroupBox {
                    title: qsTr("Audio")
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 8

                        RowLayout {
                            Label { text: qsTr("Buffer size:"); Layout.preferredWidth: 100 }
                            SpinBox {
                                from: 10
                                to: 1000
                                stepSize: 10
                                value: Settings.bufferSizeMs
                                onValueModified: Settings.bufferSizeMs = value
                            }
                            Label { text: qsTr("ms") }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Browse settings
            ColumnLayout {
                spacing: 12

                GroupBox {
                    title: qsTr("Browse Target Policy")
                    Layout.fillWidth: true

                    ColumnLayout {
                        RadioButton {
                            text: qsTr("Append to viewed playlist")
                            checked: Settings.browseTargetPolicy === 0
                            onClicked: Settings.browseTargetPolicy = 0
                        }
                        RadioButton {
                            text: qsTr("Replace generated playlist (prefer viewed)")
                            checked: Settings.browseTargetPolicy === 1
                            onClicked: Settings.browseTargetPolicy = 1
                        }
                        RadioButton {
                            text: qsTr("Create new playlist")
                            checked: Settings.browseTargetPolicy === 2
                            onClicked: Settings.browseTargetPolicy = 2
                        }
                    }
                }

                GroupBox {
                    title: qsTr("Browse Autoplay Policy")
                    Layout.fillWidth: true

                    ColumnLayout {
                        RadioButton {
                            text: qsTr("Never start playback")
                            checked: Settings.browseAutoplayPolicy === 0
                            onClicked: Settings.browseAutoplayPolicy = 0
                        }
                        RadioButton {
                            text: qsTr("Start if stopped")
                            checked: Settings.browseAutoplayPolicy === 1
                            onClicked: Settings.browseAutoplayPolicy = 1
                        }
                        RadioButton {
                            text: qsTr("Always start playback")
                            checked: Settings.browseAutoplayPolicy === 2
                            onClicked: Settings.browseAutoplayPolicy = 2
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Library settings
            ColumnLayout {
                spacing: 12

                GroupBox {
                    title: qsTr("Watch Folders")
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            interactive: false

                            model: AppViewModel.watchFolders

                            delegate: ItemDelegate {
                                width: parent.width
                                text: modelData

                                contentItem: RowLayout {
                                    Label {
                                        text: modelData
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        text: "✕"
                                        flat: true
                                        onClicked: AppViewModel.removeLibraryFolder(modelData)
                                    }
                                }
                            }
                        }

                        Button {
                            text: qsTr("Add folder...")
                            onClicked: folderDialog.open()
                        }
                    }
                }

                RowLayout {
                    Button {
                        text: qsTr("Rescan Library")
                        enabled: !AppViewModel.libraryScanning
                        onClicked: AppViewModel.rescanLibrary()
                    }

                    Label {
                        text: AppViewModel.libraryScanning 
                            ? qsTr("Scanning... %1%").arg(AppViewModel.libraryScanProgress)
                            : qsTr("%1 tracks").arg(AppViewModel.libraryTrackCount)
                        color: Theme.textSecondary
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Footer
        RowLayout {
            Layout.fillWidth: true

            CheckBox {
                text: qsTr("Restore session on startup")
                checked: Settings.restoreSession
                onClicked: Settings.restoreSession = checked
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }

    // Folder dialog would need platform integration
    // For now, just a placeholder
    Dialog {
        id: folderDialog
        title: qsTr("Add Watch Folder")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: folderPathField
            width: 300
            placeholderText: qsTr("Enter folder path...")
        }

        onAccepted: {
            if (folderPathField.text.trim().length > 0) {
                AppViewModel.addLibraryFolder(folderPathField.text.trim())
                folderPathField.text = ""
            }
        }
    }
}
