import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer 1.0

Item {
    id: root
    
    // Panel contract
    readonly property string panelType: "filebrowser"
    property string panelInstanceId: ""
    property bool isFocused: false
    
    // Signals
    signal openRequested(var request)
    signal settingsRequested(string settingsPath)
    
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSmall
        
        // Navigation bar
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            spacing: Theme.spacingTiny
            
            Button {
                text: "◀"
                enabled: AppViewModel.fileBrowserModel && AppViewModel.fileBrowserModel.canGoBack
                implicitWidth: 32
                onClicked: AppViewModel.fileBrowserModel.goBack()
            }
            
            Button {
                text: "▶"
                enabled: AppViewModel.fileBrowserModel && AppViewModel.fileBrowserModel.canGoForward
                implicitWidth: 32
                onClicked: AppViewModel.fileBrowserModel.goForward()
            }
            
            Button {
                text: "↑"
                implicitWidth: 32
                onClicked: if (AppViewModel.fileBrowserModel) AppViewModel.fileBrowserModel.goUp()
            }
            
            Button {
                text: "⌂"
                implicitWidth: 32
                onClicked: if (AppViewModel.fileBrowserModel) AppViewModel.fileBrowserModel.goHome()
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.controlHeight
                color: Theme.surface
                border.color: Theme.border
                border.width: Theme.borderWidth
                
                Label {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSmall
                    anchors.rightMargin: Theme.spacingSmall
                    verticalAlignment: Text.AlignVCenter
                    text: AppViewModel.fileBrowserModel ? AppViewModel.fileBrowserModel.displayPath : ""
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    elide: Text.ElideMiddle
                }
            }
        }
        
        // Three-column ranger-like view (simplified for now - single column)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            
            ListView {
                id: fileListView
                anchors.fill: parent
                anchors.margins: Theme.borderWidth
                clip: true
                model: AppViewModel.fileBrowserModel
                interactive: false
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Behavior on contentY { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (e) => fileListView.contentY = Math.max(0, Math.min(fileListView.contentHeight - fileListView.height, fileListView.contentY - e.angleDelta.y))
                }
                
                delegate: Rectangle {
                    width: fileListView.width
                    height: 24
                    color: fileMouse.containsMouse ? Theme.hover : "transparent"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSmall
                        anchors.rightMargin: Theme.spacingSmall
                        spacing: Theme.spacingSmall
                        
                        Label {
                            text: model.isDir ? "📁" : (model.entryType === "playlist" ? "📋" : "🎵")
                            font.pixelSize: 12
                            Layout.preferredWidth: 20
                        }
                        
                        Label {
                            text: model.fileName
                            font.pixelSize: Theme.fontSizeNormal
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    
                    MouseArea {
                        id: fileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        
                        onDoubleClicked: {
                            if (!AppViewModel.fileBrowserModel) return
                            if (model.isDir) {
                                AppViewModel.fileBrowserModel.goTo(model.filePath)
                            } else if (model.entryType === "audio") {
                                AppViewModel.addFilesToPlaylist([model.fileUrl])
                            } else if (model.entryType === "playlist") {
                                var uuid = AppViewModel.playlistStore.importPlaylist(model.filePath)
                                if (uuid) AppViewModel.playlistStore.displayedPlaylistId = uuid
                            }
                        }
                    }
                }
                
                Label {
                    anchors.centerIn: parent
                    text: "Empty folder"
                    font.pixelSize: Theme.fontSizeNormal
                    color: Theme.textMuted
                    visible: fileListView.count === 0
                }
            }
        }
    }
}
