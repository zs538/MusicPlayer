import QtQuick
import QtQuick.Controls
import MusicPlayer

ToolTip {
    id: toolTip
    leftPadding: 5
    rightPadding: 5
    topPadding: 2
    bottomPadding: 2
    background: Rectangle {
        color: Theme.surfaceAlt
        border.color: "#3a3a3a"
        border.width: 1
        radius: Theme.radiusNone
    }
    contentItem: Text {
        text: toolTip.text
        color: "#3a3a3a"
        font.pixelSize: 11
    }
}
