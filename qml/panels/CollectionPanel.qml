import QtQuick
import MusicPlayer

// CollectionPanel - Main collection browser panel for the fixed layout.

Rectangle {
    id: root
    color: Theme.background

    signal addLibraryRequested()

    readonly property alias focusWithinBrowser: browserContent.focusWithinBrowser

    function focusBrowser() {
        browserContent.focusBrowser()
    }
    function openGroupByMenu() {
        browserContent.focusBrowser()
        browserContent.openGroupByMenu()
    }
    function openSortMenu() {
        browserContent.focusBrowser()
        browserContent.openSortMenu()
    }

    CollectionBrowserContent {
        id: browserContent
        anchors.fill: parent
        initialGroupBy: Settings.groupTypeNextGroupBy("all")
        onAddLibraryRequested: root.addLibraryRequested()
    }
}
