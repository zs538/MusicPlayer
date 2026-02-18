import QtQuick
import QtQuick.Controls
import MusicPlayer

Rectangle {
    id: root
    color: Theme.surfaceAlt
    readonly property bool hasCover: AppViewModel.nowPlayingCoverUrl !== ""

    onHasCoverChanged: if (!hasCover) backImg.source = ""

    // Back image holds current, front loads new then swaps
    Image {
        id: backImg
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true; cache: true
        sourceSize: Qt.size(1024, 1024)
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(width * 2, height * 2)
    }
    Image {
        id: frontImg
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        asynchronous: true; cache: true
        sourceSize: Qt.size(1024, 1024)
        source: AppViewModel.nowPlayingCoverUrl
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(width * 2, height * 2)
        onStatusChanged: if (status === Image.Ready) backImg.source = source
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.hasCover) {
                let component = Qt.createComponent("../windows/CoverWindow.qml")
                if (component.status === Component.Ready) {
                    component.createObject(null, {
                        coverUrl: AppViewModel.nowPlayingCoverUrl,
                        artist: AppViewModel.nowPlayingArtist,
                        trackTitle: AppViewModel.nowPlayingTitle
                    })
                } else if (component.status === Component.Error) {
                    console.error("CoverWindow error:", component.errorString())
                }
            }
        }

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton && root.hasCover) {
                contextMenu.popup()
            }
        }
    }

    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Show in playlist")
            onTriggered: {
                ViewedPlaylistRouter.viewedPlaylistId = ViewedPlaylistRouter.activePlaylistId
                ViewedPlaylistRouter.requestScrollToIndex(AppViewModel.currentIndex)
            }
        }
    }
}
