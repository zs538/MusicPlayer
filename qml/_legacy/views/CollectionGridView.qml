import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer 1.0

Item {
    id: root
    
    // Input: entries to display (array of {nodeKey, displayText, childCount, filePath, album, artist})
    property var entries: []
    
    // Output signals
    signal entryClicked(int index, var entry, bool ctrl, bool shift)
    signal entryDoubleClicked(int index, var entry)
    signal entryRightClicked(int index, var entry, real x, real y)
    
    // Selection state (managed by parent)
    property var selectedIndices: new Set()
    property int selectionVersion: 0
    
    function isSelected(idx) {
        var v = selectionVersion
        return selectedIndices.has(idx)
    }
    
    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: width
        contentHeight: grid.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        
        WheelHandler {
            onWheel: function(event) {
                flickable.contentY = Math.max(0, Math.min(
                    flickable.contentY - event.angleDelta.y,
                    flickable.contentHeight - flickable.height
                ))
            }
        }
        
        Flow {
            id: grid
            width: parent.width
            spacing: Theme.spacingSmall
            
            Repeater {
                model: root.entries
                
                Rectangle {
                    id: cell
                    width: 120
                    height: 140
                    color: cellMouse.containsMouse ? Theme.hover : 
                           root.isSelected(index) ? Theme.selected : "transparent"
                    
                    property var entryData: modelData
                    property int entryIndex: index
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 2
                        
                        // Cover art
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width
                            color: Theme.surfaceAlt
                            
                            Image {
                                id: coverImage
                                anchors.fill: parent
                                source: cell.entryData.filePath ? 
                                    CoverArtProvider.coverUrlForFile(cell.entryData.filePath, cell.entryData.album || "", cell.entryData.artist || "") : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                visible: status === Image.Ready
                                cache: true
                                sourceSize.width: 512
                                sourceSize.height: 512
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                            }
                            
                            Label {
                                anchors.centerIn: parent
                                text: "♪"
                                font.pixelSize: 24
                                color: Theme.textMuted
                                visible: parent.children[0].status !== Image.Ready
                            }
                        }
                        
                        // Title
                        Label {
                            text: cell.entryData.displayText || ""
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        
                        // Track count
                        Label {
                            text: (cell.entryData.childCount || 0) + " tracks"
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.textSecondary
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    
                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                root.entryRightClicked(cell.entryIndex, cell.entryData, mouse.x, mouse.y)
                            } else {
                                var ctrl = mouse.modifiers & Qt.ControlModifier
                                var shift = mouse.modifiers & Qt.ShiftModifier
                                root.entryClicked(cell.entryIndex, cell.entryData, ctrl, shift)
                            }
                        }
                        
                        onDoubleClicked: {
                            root.entryDoubleClicked(cell.entryIndex, cell.entryData)
                        }
                    }
                }
            }
        }
    }
    
    Label {
        anchors.centerIn: parent
        text: "No items"
        color: Theme.textMuted
        visible: root.entries.length === 0
    }
}
