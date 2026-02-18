import QtQuick
import QtQuick.Window

Window {
    id: root
    property string coverUrl: ""; property string artist: ""; property string trackTitle: ""
    property real _w: _maxH; property real _h: _maxH
    readonly property real _maxH: Screen.height * 0.8

    width: _w; height: _h
    minimumWidth: _w; maximumWidth: _w; minimumHeight: _h; maximumHeight: _h
    visible: true; color: "#000"
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: (artist && trackTitle) ? artist + " - " + trackTitle + " (" + img.sourceSize.width + "×" + img.sourceSize.height + ")" : img.sourceSize.width + "×" + img.sourceSize.height

    Shortcut { sequence: "Escape"; onActivated: root.close() }

    Image {
        id: img
        anchors.fill: parent; source: root.coverUrl; fillMode: Image.PreserveAspectFit
        onStatusChanged: if (status === Image.Ready && sourceSize.height > 0) { let r = sourceSize.width / sourceSize.height; root._w = root._maxH * r; root._h = root._maxH }
        DragHandler { target: null; onActiveChanged: if (active) root.startSystemMove(); cursorShape: Qt.SizeAllCursor }
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.MiddleButton; onClicked: root.close() }
    }
}
