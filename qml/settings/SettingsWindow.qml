import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import MusicPlayer

Window {
    id: root
    width: 500
    height: 400
    minimumWidth: 525
    minimumHeight: 300
    title: qsTr("Settings")
    flags: Qt.Tool | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    color: Theme.background

    property int currentPage: 0
    readonly property var pages: ["Behavior", "Appearance", "Playback", "Library", "Session"]
    
    onVisibleChanged: {
        if (visible) {
            refreshWatchFolders()
        }
    }
    
    function refreshWatchFolders() {
        folderModel.clear()
        var folders = AppViewModel.watchFolders
        for (var i = 0; i < folders.length; i++) {
            folderModel.append({ "path": folders[i] })
        }
    }
    
    ListModel {
        id: folderModel
    }
    
    Connections {
        target: AppViewModel
        function onLibraryFoldersChanged() {
            root.refreshWatchFolders()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Navigation
        Rectangle {
            Layout.preferredWidth: 100
            Layout.fillHeight: true
            color: Theme.background
            
            Column {
                anchors.fill: parent
                anchors.topMargin: 8
                
                Repeater {
                    model: root.pages
                    delegate: Rectangle {
                        width: parent.width
                        height: 28
                        color: root.currentPage === index ? Theme.accent : "transparent"
                        required property string modelData
                        required property int index
                        
                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            text: modelData
                            verticalAlignment: Text.AlignVCenter
                            color: root.currentPage === index ? "white" : Theme.textPrimary
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.currentPage = index
                        }
                    }
                }
            }
            
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.border
            }
        }

        // Content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            currentIndex: root.currentPage

            // Page 0: Behavior
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 8
                    
                    // Adding tracks
                    GroupBox {
                        title: "Adding track(s) will"
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Never start playing", "Play only if the playback is stopped", "Always start playing"]
                                currentIndex: Settings.addTracksPolicy
                                onCurrentValueChanged: Settings.addTracksPolicy = currentIndex
                            }
                        }
                    }
                    
                    // Previous button
                    GroupBox {
                        title: "Pressing previous button will"
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["Jump to previous track right away", "Restart song, then jump to the previous one if pressed again"]
                                currentIndex: Settings.previousButtonAction
                                onCurrentValueChanged: Settings.previousButtonAction = currentIndex
                            }
                        }
                    }
                    
                    // Opening tracks
                    GroupBox {
                        title: "Opening track(s) will"
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 4
                            
                            RadioButton {
                                text: "Append to viewed playlist"
                                checked: Settings.openingTracksAction === 0
                                onClicked: Settings.openingTracksAction = 0
                            }
                            
                            RowLayout {
                                RadioButton {
                                    text: "Create a new playlist"
                                    checked: Settings.openingTracksAction === 1
                                    onClicked: Settings.openingTracksAction = 1
                                }
                                Label { 
                                    text: "Count:" 
                                    enabled: Settings.openingTracksAction === 1
                                }
                                SpinBox {
                                    from: 1
                                    to: 20
                                    value: Settings.generatedPlaylistCount
                                    onValueModified: Settings.generatedPlaylistCount = value
                                    enabled: Settings.openingTracksAction === 1
                                    Layout.preferredWidth: 80
                                }
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }

            // Page 1: Appearance
            ColumnLayout {
                spacing: 12
                
                Label { text: "Grid View"; font.bold: true }
                
                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 8
                    
                    Label { text: "Minimum cell width:" }
                    RowLayout {
                        SpinBox {
                            from: 60
                            to: 300
                            stepSize: 10
                            value: Settings.gridCellMinWidth
                            onValueModified: Settings.gridCellMinWidth = value
                        }
                        Label { text: "px"; color: Theme.textSecondary }
                    }
                    
                    Label { text: "Maximum cell width:" }
                    RowLayout {
                        SpinBox {
                            from: 80
                            to: 400
                            stepSize: 10
                            value: Settings.gridCellMaxWidth
                            onValueModified: Settings.gridCellMaxWidth = value
                        }
                        Label { text: "px"; color: Theme.textSecondary }
                    }
                }
                
                Label {
                    text: "Grid cells expand to fill width, clamped between min and max."
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                
                Item { Layout.fillHeight: true }
            }

            // Page 2: Playback
            ColumnLayout {
                spacing: 12
                
                Label { text: "Playback Mode"; font.bold: true }
                RadioButton { 
                    text: "Gapless Session (resamples to common format)"
                    checked: Settings.playbackMode === 0
                    onClicked: Settings.playbackMode = 0
                }
                RadioButton { 
                    text: "Bit-Perfect (not yet implemented)"
                    enabled: false
                    checked: Settings.playbackMode === 1
                    onClicked: Settings.playbackMode = 1
                }
                
                Label { text: "Buffer Size"; font.bold: true }
                RowLayout {
                    SpinBox {
                        id: bufferSpinBox
                        from: 50
                        to: 500
                        stepSize: 10
                        value: Settings.bufferSizeMs
                        onValueModified: Settings.bufferSizeMs = value
                    }
                    Label { text: "ms (lower = less latency, higher = more stable)" }
                }
                
                Item { Layout.fillHeight: true }
            }

            // Page 3: Library
            ColumnLayout {
                id: libraryPage
                spacing: 12
                
                property int selectedIdx: -1
                
                Label { text: "Watch Folders"; font.bold: true }
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    border.width: 1
                    border.color: Theme.border
                    color: Theme.background
                    clip: true
                    
                    ListView {
                        id: folderListView
                        anchors.fill: parent
                        anchors.margins: 2
                        model: folderModel
                        interactive: false
                        Behavior on contentY { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        WheelHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                            onWheel: (e) => folderListView.contentY = Math.max(0, Math.min(folderListView.contentHeight - folderListView.height, folderListView.contentY - e.angleDelta.y))
                        }
                        
                        delegate: Rectangle {
                            width: folderListView.width
                            height: 24
                            color: libraryPage.selectedIdx === index ? Theme.accent : (index % 2 === 0 ? "transparent" : Qt.rgba(0,0,0,0.03))
                            required property string path
                            required property int index
                            
                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                text: path
                                elide: Text.ElideMiddle
                                verticalAlignment: Text.AlignVCenter
                                color: libraryPage.selectedIdx === index ? "white" : Theme.textPrimary
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: libraryPage.selectedIdx = index
                            }
                        }
                        
                        Label {
                            anchors.centerIn: parent
                            text: "(no folders added)"
                            color: Theme.textSecondary
                            visible: folderListView.count === 0
                        }
                        
                    }
                }
                
                RowLayout {
                    spacing: 6
                    Button {
                        text: "Add Folder..."
                        onClicked: folderDialog.open()
                    }
                    Button {
                        text: "Remove"
                        enabled: libraryPage.selectedIdx >= 0 && libraryPage.selectedIdx < folderModel.count
                        onClicked: {
                            if (libraryPage.selectedIdx >= 0 && libraryPage.selectedIdx < folderModel.count) {
                                var path = folderModel.get(libraryPage.selectedIdx).path
                                libraryPage.selectedIdx = -1
                                AppViewModel.removeLibraryFolder(path)
                            }
                        }
                    }
                }
                
                RowLayout {
                    Button {
                        text: "Rescan Library"
                        enabled: !AppViewModel.libraryScanning
                        onClicked: AppViewModel.rescanLibrary()
                    }
                    Label {
                        text: AppViewModel.libraryScanning 
                            ? "Scanning... " + AppViewModel.libraryScanProgress + " files processed"
                            : AppViewModel.libraryTrackCount + " tracks in library"
                        color: Theme.textSecondary
                    }
                }
                
                Item { Layout.fillHeight: true }
                
                FolderDialog {
                    id: folderDialog
                    title: "Select Music Folder"
                    onAccepted: {
                        var path = selectedFolder.toString()
                        if (path.startsWith("file://")) {
                            path = path.substring(7)
                        }
                        AppViewModel.addLibraryFolder(path)
                    }
                }
            }

            // Page 4: Session
            ColumnLayout {
                spacing: 12
                
                Label { text: "Startup"; font.bold: true }
                
                CheckBox {
                    text: "Restore session on startup"
                    checked: Settings.restoreSession
                    onClicked: Settings.restoreSession = checked
                }
                
                Label {
                    text: "Restores: playlist tabs, window geometry, column layout"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    Layout.leftMargin: 24
                }
                
                Item { Layout.fillHeight: true }
            }
        }
    }
}