import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer 1.0

Item {
    id: root
    
    // Input: songs to display (array of track objects)
    property var entries: []
    
    // Output signals
    signal entryClicked(int index, var entry, bool ctrl, bool shift)
    signal entryDoubleClicked(int index, var entry)
    signal entryRightClicked(int index, var entry, real x, real y)
    
    // Selection state
    property var selectedIndices: new Set()
    property int selectionVersion: 0
    
    function isSelected(idx) {
        var v = selectionVersion
        return selectedIndices.has(idx)
    }
    
    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        model: root.entries
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        
        WheelHandler {
            onWheel: function(event) {
                listView.contentY = Math.max(0, Math.min(
                    listView.contentY - event.angleDelta.y,
                    listView.contentHeight - listView.height
                ))
            }
        }
        
        delegate: Rectangle {
            id: songRow
            width: listView.width
            height: 28
            color: songMouse.containsMouse ? Theme.hover : 
                   root.isSelected(index) ? Theme.selected : "transparent"
            
            property var entryData: modelData
            property int entryIndex: index
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSmall
                anchors.rightMargin: Theme.spacingSmall
                spacing: Theme.spacingSmall
                
                // Track number
                Label {
                    text: (songRow.entryData.trackNumber || (songRow.entryIndex + 1)) + "."
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMuted
                    Layout.preferredWidth: 30
                    horizontalAlignment: Text.AlignRight
                }
                
                // Title
                Label {
                    text: songRow.entryData.title || songRow.entryData.displayText || ""
                    font.pixelSize: Theme.fontSizeNormal
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                
                // Artist
                Label {
                    text: songRow.entryData.artist || ""
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                    Layout.preferredWidth: 120
                    visible: text !== ""
                }
                
                // Duration
                Label {
                    text: formatTime(songRow.entryData.durationMs || 0)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    Layout.preferredWidth: 40
                    horizontalAlignment: Text.AlignRight
                }
            }
            
            MouseArea {
                id: songMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        root.entryRightClicked(songRow.entryIndex, songRow.entryData, mouse.x, mouse.y)
                    } else {
                        var ctrl = mouse.modifiers & Qt.ControlModifier
                        var shift = mouse.modifiers & Qt.ShiftModifier
                        root.entryClicked(songRow.entryIndex, songRow.entryData, ctrl, shift)
                    }
                }
                
                onDoubleClicked: {
                    root.entryDoubleClicked(songRow.entryIndex, songRow.entryData)
                }
            }
        }
    }
    
    Label {
        anchors.centerIn: parent
        text: "No songs"
        color: Theme.textMuted
        visible: root.entries.length === 0
    }
    
    function formatTime(ms) {
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }
}
