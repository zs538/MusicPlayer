import QtQuick
import MusicPlayer

// CollectionPanel - Main collection browser panel for the fixed layout.

Rectangle {
    id: root
    color: Theme.background

    CollectionBrowserContent {
        anchors.fill: parent
        initialGroupBy: Settings.groupTypeNextGroupBy("all")
    }
}
