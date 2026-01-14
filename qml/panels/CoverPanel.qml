import QtQuick
import QtQuick.Controls
import MusicPlayer

Rectangle {
    id: root
    color: Theme.surface

    // 1:1 aspect ratio cover art only, no text, no padding
    Image {
        id: coverImage
        anchors.fill: parent
        source: AppViewModel.nowPlayingCoverUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        sourceSize.width: 1024
        sourceSize.height: 1024
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(width * 2, height * 2)

        Rectangle {
            anchors.fill: parent
            color: Theme.surfaceAlt
            visible: coverImage.status !== Image.Ready
        }
    }
}
