import QtQuick
import QtQuick.Controls
import MusicPlayer

Rectangle {
    id: root

    component PointingCursor: HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    property var layout: TrackListColumnsSupport.defaultLayout()
    property var customTagKeys: []
    property int rowHeight: 22
    property int leftMargin: 0
    property int rightMargin: 0
    property int _columnContextIndex: -1
    property int _dragColumnIndex: -1
    property int _dragSeparatorIndex: -1
    property real _dragCurrentX: 0
    property int _resizeColumnIndex: -1
    property real _resizeStartX: 0
    property var _resizeBaseLayout: ({})

    readonly property var normalizedLayout: TrackListColumnsSupport.ensureLayout(layout)
    readonly property var columns: normalizedLayout.columns || []
    readonly property real contentWidth: Math.max(0, width - leftMargin - rightMargin)
    readonly property var resolvedWidths: TrackListColumnsSupport.resolveColumnWidths(columns, contentWidth)
    readonly property var columnStarts: TrackListColumnsSupport.columnStartPositions(resolvedWidths)
    readonly property var separatorPositions: TrackListColumnsSupport.separatorPositions(resolvedWidths)

    signal layoutEdited(var layout)
    signal columnClicked(string key)

    visible: !!normalizedLayout.headerVisible
    height: visible ? rowHeight : 0
    color: "transparent"
    border.width: 0

    function emitEdited(nextLayout) {
        root.layoutEdited(TrackListColumnsSupport.ensureLayout(nextLayout))
    }

    function setColumn(key) {
        root.emitEdited(TrackListColumnsSupport.setColumn(root.layout, root._columnContextIndex, key))
    }

    function currentColumnAlignment() {
        if (root._columnContextIndex < 0 || root._columnContextIndex >= root.columns.length)
            return "left"
        return root.columns[root._columnContextIndex].alignment || "left"
    }

    function beginResize(index, localX) {
        if (root.normalizedLayout.headerLocked)
            return
        root._resizeColumnIndex = index
        root._resizeStartX = localX
        root._resizeBaseLayout = TrackListColumnsSupport.ensureLayout(root.layout)
    }

    function updateResize(localX) {
        if (root._resizeColumnIndex < 0)
            return
        root.emitEdited(TrackListColumnsSupport.resizeBetween(
            root._resizeBaseLayout,
            root._resizeColumnIndex,
            localX - root._resizeStartX,
            root.contentWidth
        ))
    }

    function endResize() {
        root._resizeColumnIndex = -1
        root._resizeBaseLayout = ({})
    }

    function nearestSeparatorIndex(localX) {
        let clamped = Math.max(0, Math.min(root.contentWidth, localX))
        let bestIndex = 0
        let bestDistance = Number.MAX_VALUE
        for (let i = 0; i < root.separatorPositions.length; ++i) {
            let distance = Math.abs(root.separatorPositions[i] - clamped)
            if (distance < bestDistance) {
                bestDistance = distance
                bestIndex = i
            }
        }
        return bestIndex
    }

    function beginDrag(index, localX) {
        if (root.normalizedLayout.headerLocked)
            return
        root._dragColumnIndex = index
        root._dragCurrentX = localX
        root._dragSeparatorIndex = root.nearestSeparatorIndex(localX)
    }

    function updateDrag(localX) {
        if (root._dragColumnIndex < 0)
            return
        root._dragCurrentX = Math.max(0, Math.min(root.contentWidth, localX))
        root._dragSeparatorIndex = root.nearestSeparatorIndex(root._dragCurrentX)
    }

    function endDrag() {
        if (root._dragColumnIndex < 0)
            return
        let fromIndex = root._dragColumnIndex
        let separatorIndex = root._dragSeparatorIndex
        root._dragColumnIndex = -1
        root._dragSeparatorIndex = -1
        root._dragCurrentX = 0
        if (separatorIndex < 0)
            return
        let targetIndex = separatorIndex > fromIndex ? separatorIndex - 1 : separatorIndex
        targetIndex = Math.max(0, Math.min(targetIndex, root.columns.length - 1))
        if (targetIndex === fromIndex)
            return
        root.emitEdited(TrackListColumnsSupport.moveColumn(root.layout, fromIndex, targetIndex))
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
        z: 2
    }

    Repeater {
        model: root.columns.length

        delegate: Item {
            id: headerCell
            x: root.leftMargin + (root.columnStarts[index] || 0)
            width: root.resolvedWidths[index] || 0
            height: root.height
            opacity: root._dragColumnIndex === index ? 0.35 : 1.0

            Rectangle {
                anchors.fill: parent
                color: cellMouse.containsMouse && root._dragColumnIndex !== index ? Theme.hover : "transparent"
            }

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: index < root.columns.length - 1 ? 1 : 0
                height: index < root.columns.length - 1 ? Math.max(10, parent.height - 6) : 0
                color: Theme.border
                opacity: 0.7
            }

            readonly property var columnData: root.columns[index] || ({})

            Label {
                anchors {
                    fill: parent
                    leftMargin: 2
                    rightMargin: 2
                }
                text: headerCell.columnData.title || ""
                color: Theme.textSecondary
                font.pixelSize: 10
                font.bold: true
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }

            MouseArea {
                id: cellMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: root.normalizedLayout.headerLocked ? Qt.PointingHandCursor : (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)

                property real pressLocalX: 0
                property bool dragActive: false

                onPressed: (mouse) => {
                    if (mouse.button === Qt.LeftButton && !root.normalizedLayout.headerLocked) {
                        pressLocalX = headerCell.x + mouse.x - root.leftMargin
                        dragActive = false
                    }
                }
                onPositionChanged: (mouse) => {
                    if (!pressed || root.normalizedLayout.headerLocked || !(mouse.buttons & Qt.LeftButton))
                        return
                    let localX = headerCell.x + mouse.x - root.leftMargin
                    if (!dragActive) {
                        if (Math.abs(localX - pressLocalX) >= 4) {
                            dragActive = true
                            root.beginDrag(index, localX)
                        }
                    } else {
                        root.updateDrag(localX)
                    }
                }
                onReleased: (mouse) => {
                    if (mouse.button === Qt.LeftButton && dragActive)
                        root.endDrag()
                    dragActive = false
                }
                onCanceled: {
                    if (dragActive)
                        root.endDrag()
                    dragActive = false
                }
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        root._columnContextIndex = index
                        columnContextMenu.popup()
                    } else if (mouse.button === Qt.LeftButton && !dragActive && headerCell.columnData.key) {
                        root.columnClicked(String(headerCell.columnData.key))
                    }
                }
            }

        }
    }

    Repeater {
        model: Math.max(0, root.columns.length - 1)

        delegate: Item {
            x: root.leftMargin + (root.separatorPositions[index + 1] || 0) - width / 2
            width: 8
            height: root.height
            z: 3

            HoverHandler {
                enabled: !root.normalizedLayout.headerLocked
                cursorShape: Qt.SizeHorCursor
            }

            DragHandler {
                target: null
                enabled: !root.normalizedLayout.headerLocked
                acceptedButtons: Qt.LeftButton

                onActiveChanged: {
                    if (active)
                        root.beginResize(index, root.separatorPositions[index + 1] || 0)
                    else
                        root.endResize()
                }
                onActiveTranslationChanged: {
                    if (active)
                        root.updateResize(root._resizeStartX + activeTranslation.x)
                }
            }
        }
    }

    Rectangle {
        visible: root._dragColumnIndex >= 0 && root._dragSeparatorIndex >= 0
        x: root.leftMargin + (root.separatorPositions[root._dragSeparatorIndex] || 0) - width / 2
        width: 3
        height: parent.height
        color: Theme.accent
        z: 4
    }

    Menu {
        id: columnContextMenu

        MenuItem {
            text: "Hide Header"
            PointingCursor {}
            onTriggered: root.emitEdited(TrackListColumnsSupport.setHeaderVisible(root.layout, false))
        }
        MenuSeparator {}
        MenuItem {
            text: "Lock Header"
            checkable: true
            checked: root.normalizedLayout.headerLocked
            PointingCursor {}
            onTriggered: root.emitEdited(TrackListColumnsSupport.setHeaderLocked(root.layout, !root.normalizedLayout.headerLocked))
        }
        MenuSeparator {}
        MenuItem {
            text: "Add Column"
            enabled: !root.normalizedLayout.headerLocked
            PointingCursor {}
            onTriggered: root.emitEdited(TrackListColumnsSupport.splitColumn(root.layout, root._columnContextIndex))
        }
        Menu {
            title: "Set Column"

            MenuItem {
                text: "None"
                PointingCursor {}
                onTriggered: root.setColumn("")
            }
            MenuSeparator {}
            MenuItem {
                text: TrackListColumnsSupport.titleForKey("trackNumber")
                PointingCursor {}
                onTriggered: root.setColumn("trackNumber")
            }
            MenuItem {
                text: TrackListColumnsSupport.titleForKey("title")
                PointingCursor {}
                onTriggered: root.setColumn("title")
            }
            MenuItem {
                text: TrackListColumnsSupport.titleForKey("artist")
                PointingCursor {}
                onTriggered: root.setColumn("artist")
            }
            MenuItem {
                text: TrackListColumnsSupport.titleForKey("album")
                PointingCursor {}
                onTriggered: root.setColumn("album")
            }
            MenuItem {
                text: TrackListColumnsSupport.titleForKey("durationMs")
                PointingCursor {}
                onTriggered: root.setColumn("durationMs")
            }
            MenuSeparator {}
            Menu {
                id: otherColumnsMenu
                title: "Other"

                MenuItem {
                    text: "No other columns"
                    enabled: false
                    visible: otherColumnsInstantiator.count === 0
                }

                Instantiator {
                    id: otherColumnsInstantiator
                    model: TrackListColumnsSupport.otherBuiltinKeys

                    delegate: MenuItem {
                        required property string modelData

                        text: TrackListColumnsSupport.titleForKey(modelData)
                        PointingCursor {}
                        onTriggered: root.setColumn(modelData)
                    }

                    onObjectAdded: (index, object) => otherColumnsMenu.insertItem(index + 1, object)
                    onObjectRemoved: (index, object) => otherColumnsMenu.removeItem(object)
                }
            }
            Menu {
                id: customColumnsMenu
                title: "Custom"

                MenuItem {
                    text: "No custom tags found"
                    enabled: false
                    visible: customColumnsInstantiator.count === 0
                }

                Instantiator {
                    id: customColumnsInstantiator
                    model: root.customTagKeys

                    delegate: MenuItem {
                        required property string modelData

                        text: modelData
                        PointingCursor {}
                        onTriggered: root.setColumn("custom:" + String(modelData).toUpperCase())
                    }

                    onObjectAdded: (index, object) => customColumnsMenu.insertItem(index + 1, object)
                    onObjectRemoved: (index, object) => customColumnsMenu.removeItem(object)
                }
            }
        }

        Menu {
            title: "Text Align"

            MenuItem {
                text: "Left"
                checkable: true
                checked: root.currentColumnAlignment() === "left"
                PointingCursor {}
                onTriggered: root.emitEdited(TrackListColumnsSupport.setColumnAlignment(root.layout, root._columnContextIndex, "left"))
            }
            MenuItem {
                text: "Center"
                checkable: true
                checked: root.currentColumnAlignment() === "center"
                PointingCursor {}
                onTriggered: root.emitEdited(TrackListColumnsSupport.setColumnAlignment(root.layout, root._columnContextIndex, "center"))
            }
            MenuItem {
                text: "Right"
                checkable: true
                checked: root.currentColumnAlignment() === "right"
                PointingCursor {}
                onTriggered: root.emitEdited(TrackListColumnsSupport.setColumnAlignment(root.layout, root._columnContextIndex, "right"))
            }
        }
        MenuItem {
            text: "Remove Column"
            enabled: root.columns.length > 1 && !root.normalizedLayout.headerLocked
            PointingCursor {}
            onTriggered: root.emitEdited(TrackListColumnsSupport.removeColumn(root.layout, root._columnContextIndex))
        }
    }
}
