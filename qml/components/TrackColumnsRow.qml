import QtQuick
import QtQuick.Controls
import MusicPlayer

Item {
    id: root

    property var columns: []
    property var trackData: ({})
    property color primaryTextColor: Theme.textPrimary
    property color secondaryTextColor: Theme.textSecondary
    property real fontPixelSize: 11
    property bool boldPrimary: false
    property real leftMargin: 0
    property real rightMargin: 0

    readonly property real availableWidth: Math.max(0, width - leftMargin - rightMargin)
    readonly property var resolvedWidths: TrackListColumnsSupport.resolveColumnWidths(columns, availableWidth)
    readonly property var columnStarts: TrackListColumnsSupport.columnStartPositions(resolvedWidths)

    implicitHeight: fontPixelSize + 8

    function textAtX(xPos) {
        let localX = xPos - leftMargin
        if (localX < 0 || localX > availableWidth)
            return ""
        for (let i = 0; i < columns.length; ++i) {
            let start = columnStarts[i] || 0
            let width = resolvedWidths[i] || 0
            if (localX >= start && localX < start + width)
                return TrackListColumnsSupport.textForColumn(trackData, columns[i] || ({}))
        }
        return ""
    }

    Repeater {
        model: root.columns

        delegate: Item {
            readonly property var columnData: root.columns[index] || ({})

            x: root.leftMargin + (root.columnStarts[index] || 0)
            width: root.resolvedWidths[index] || 0
            height: root.height

            Label {
                id: columnLabel
                anchors {
                    fill: parent
                    leftMargin: 2
                    rightMargin: 2
                }
                text: TrackListColumnsSupport.textForColumn(root.trackData, columnData)
                color: (columnData.tone || "primary") === "secondary" ? root.secondaryTextColor : root.primaryTextColor
                font.pixelSize: root.fontPixelSize
                font.bold: root.boldPrimary && String(columnData.key) === "title"
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: (columnData.alignment || "left") === "right"
                    ? Text.AlignRight
                    : ((columnData.alignment || "left") === "center" ? Text.AlignHCenter : Text.AlignLeft)

                HoverHandler {
                    id: columnHover
                }

                ToolTip.visible: columnHover.hovered && columnLabel.text.length > 0
                ToolTip.delay: 800
                ToolTip.timeout: 5000
                ToolTip.text: columnLabel.text
            }
        }
    }
}
