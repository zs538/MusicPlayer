import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer 1.0

Item {
    id: root
    
    // Input: entries to display
    property var entries: []
    
    // Output signals
    signal entryClicked(int index, var entry, bool ctrl, bool shift)
    signal entryDoubleClicked(int index, var entry)
    signal entryRightClicked(int index, var entry, real x, real y)
    
    // Selection state
    property var selectedIndices: new Set()
    property int selectionVersion: 0
    
    // Expanded state for entries (index -> bool)
    property var expandedIndices: new Set()
    property int expandedVersion: 0
    
    function isSelected(idx) {
        var v = selectionVersion
        return selectedIndices.has(idx)
    }
    
    function isExpanded(idx) {
        var v = expandedVersion
        return expandedIndices.has(idx)
    }
    
    function toggleExpanded(idx) {
        if (expandedIndices.has(idx)) {
            expandedIndices.delete(idx)
        } else {
            expandedIndices.add(idx)
        }
        expandedVersion++
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
        
        delegate: Column {
            id: delegateColumn
            width: listView.width
            
            property var entryData: modelData
            property int entryIndex: index
            property bool expanded: {
                var v = root.expandedVersion  // Force re-evaluation
                return root.expandedIndices.has(index)
            }
            
            // Entry row
            Rectangle {
                id: entryRow
                width: parent.width
                height: 40
                color: entryMouse.containsMouse ? Theme.hover : 
                       root.isSelected(index) ? Theme.selected : "transparent"
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingSmall
                    anchors.rightMargin: Theme.spacingSmall
                    spacing: Theme.spacingSmall
                    
                    // Expand arrow (only if has children)
                    Label {
                        id: expandArrow
                        text: delegateColumn.entryData.childCount > 0 ? (delegateColumn.expanded ? "▾" : "▸") : ""
                        font.pixelSize: 12
                        color: Theme.textMuted
                        Layout.preferredWidth: 20
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    // Cover art
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        color: Theme.surfaceAlt
                        
                        Image {
                            id: coverImage
                            anchors.fill: parent
                            source: delegateColumn.entryData.filePath ? 
                                CoverArtProvider.coverUrlForFile(delegateColumn.entryData.filePath, delegateColumn.entryData.album || "", delegateColumn.entryData.artist || "") : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: status === Image.Ready
                            cache: true
                            sourceSize.width: 256
                            sourceSize.height: 256
                            layer.enabled: true
                            layer.smooth: true
                            layer.textureSize: Qt.size(width * 2, height * 2)
                        }
                        
                        Label {
                            anchors.centerIn: parent
                            text: "♪"
                            font.pixelSize: 14
                            color: Theme.textMuted
                            visible: parent.children[0].status !== Image.Ready
                        }
                    }
                    
                    // Title
                    Label {
                        text: delegateColumn.entryData.displayText || ""
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    
                    // Info
                    Label {
                        text: (delegateColumn.entryData.childCount || 0) + " tracks"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }
                
                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            root.entryRightClicked(delegateColumn.entryIndex, delegateColumn.entryData, mouse.x, mouse.y)
                        } else {
                            var ctrl = mouse.modifiers & Qt.ControlModifier
                            var shift = mouse.modifiers & Qt.ShiftModifier
                            root.entryClicked(delegateColumn.entryIndex, delegateColumn.entryData, ctrl, shift)
                        }
                    }
                    
                    onDoubleClicked: {
                        root.entryDoubleClicked(delegateColumn.entryIndex, delegateColumn.entryData)
                    }
                }
                
                // Expand arrow click area (on top of entryMouse)
                MouseArea {
                    x: Theme.spacingSmall
                    y: 0
                    width: 24
                    height: parent.height
                    visible: delegateColumn.entryData.childCount > 0
                    z: 1
                    
                    onClicked: root.toggleExpanded(delegateColumn.entryIndex)
                }
            }
            
            // Expanded tracks (compact list)
            Column {
                width: parent.width
                visible: delegateColumn.expanded && delegateColumn.entryData.tracks
                
                Repeater {
                    model: delegateColumn.expanded && delegateColumn.entryData.tracks ? delegateColumn.entryData.tracks : []
                    
                    Rectangle {
                        width: listView.width
                        height: 24
                        color: trackMouse.containsMouse ? Theme.hover : "transparent"
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 56
                            anchors.rightMargin: Theme.spacingSmall
                            spacing: Theme.spacingSmall
                            
                            Label {
                                text: (modelData.trackNumber || (index + 1)) + "."
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.textMuted
                                Layout.preferredWidth: 24
                                horizontalAlignment: Text.AlignRight
                            }
                            
                            Label {
                                text: modelData.title || ""
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            
                            Label {
                                text: formatTime(modelData.durationMs || 0)
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.textSecondary
                            }
                        }
                        
                        MouseArea {
                            id: trackMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            
                            onDoubleClicked: {
                                AppViewModel.addLibraryTracksToPlaylist([modelData])
                            }
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
    
    function formatTime(ms) {
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }
}
