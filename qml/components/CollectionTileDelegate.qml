import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

Item {
    id: gridDel

    required property int index
    required property string entryType
    required property string groupType
    required property var groupValue
    required property string displayText
    required property string subtitle
    required property string representativeFilePath
    required property var coverFilePaths
    required property string filePath

    property bool selected: false
    property bool playButtonVisible: false

    signal clicked(int index, int button)
    signal doubleClicked(int index)
    signal contextMenuRequested(int index, string entryType, string groupType, var groupValue, string filePath)
    signal playButtonClicked(string groupType, var groupValue)

    Rectangle {
        anchors.fill: parent
        color: gridMouseArea.containsMouse ? "#ebebeb" : "transparent"

        MouseArea {
            id: gridMouseArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            cursorShape: Qt.PointingHandCursor

            onClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton) {
                    gridDel.clicked(gridDel.index, Qt.LeftButton)
                } else if (mouse.button === Qt.RightButton) {
                    gridDel.contextMenuRequested(gridDel.index, gridDel.entryType, gridDel.groupType, gridDel.groupValue, gridDel.filePath)
                } else if (mouse.button === Qt.MiddleButton) {
                    gridDel.clicked(gridDel.index, Qt.MiddleButton)
                }
            }

            onDoubleClicked: {
                gridDel.doubleClicked(gridDel.index)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            anchors.topMargin: 4
            anchors.bottomMargin: 12
            spacing: 2

            Rectangle {
                id: coverContainer
                Layout.fillWidth: true
                Layout.preferredHeight: width
                color: Theme.surfaceAlt
                border.color: gridDel.selected ? "#505050" : Theme.border
                border.width: 1

                Image {
                    id: coverImage
                    anchors.fill: parent
                    anchors.margins: 1
                    source: AppViewModel.coverImageSourceForFiles(gridDel.coverFilePaths)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    sourceSize.width: 512
                    sourceSize.height: 512
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)

                    Image {
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: gridDel.entryType === "group"
                            ? Qt.resolvedUrl("../icons/album.svg")
                            : Qt.resolvedUrl("../icons/music_note.svg")
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectFit
                        opacity: 0.3
                        visible: coverImage.status !== Image.Ready
                    }
                }
            }

            Label {
                id: titleLabel
                text: gridDel.displayText || ""
                color: Theme.textPrimary
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft

                MouseArea {
                    id: titleMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    property point toolTipAnchorPos: Qt.point(0, 0)

                    function updateToolTipAnchor(x, y) {
                        toolTipAnchorPos = Qt.point(x + 8, y + 12)
                    }

                    StyledHoverToolTip {
                        id: titleToolTip
                        parent: titleMouseArea
                        visible: titleMouseArea.containsMouse && titleLabel.text.length > 0
                        delay: 800
                        timeout: 5000
                        text: titleLabel.text
                        x: Math.max(0, titleMouseArea.toolTipAnchorPos.x)
                        y: Math.max(0, titleMouseArea.toolTipAnchorPos.y)
                        onVisibleChanged: {
                            if (visible)
                                titleMouseArea.updateToolTipAnchor(titleMouseArea.mouseX, titleMouseArea.mouseY)
                        }
                    }
                }
            }

            Label {
                id: subtitleLabel
                text: gridDel.subtitle || ""
                color: Theme.textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft

                MouseArea {
                    id: subtitleMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    property point toolTipAnchorPos: Qt.point(0, 0)

                    function updateToolTipAnchor(x, y) {
                        toolTipAnchorPos = Qt.point(x + 8, y + 12)
                    }

                    StyledHoverToolTip {
                        id: subtitleToolTip
                        parent: subtitleMouseArea
                        visible: subtitleMouseArea.containsMouse && subtitleLabel.text.length > 0
                        delay: 800
                        timeout: 5000
                        text: subtitleLabel.text
                        x: Math.max(0, subtitleMouseArea.toolTipAnchorPos.x)
                        y: Math.max(0, subtitleMouseArea.toolTipAnchorPos.y)
                        onVisibleChanged: {
                            if (visible)
                                subtitleMouseArea.updateToolTipAnchor(subtitleMouseArea.mouseX, subtitleMouseArea.mouseY)
                        }
                    }
                }
            }
        }

        // Play button overlay - only for groups, when selected and enabled in settings
        // Positioned over cover container, outside MouseArea to capture clicks
        Rectangle {
            id: playButton
            visible: gridDel.playButtonVisible
            x: parent.width - 24
            y: 4 + coverContainer.height - 20
            width: 20
            height: 20
            color: playButtonMouseArea.pressed ? "#d0d0d0" : (playButtonMouseArea.containsMouse ? "#f0f0f0" : "#ffffff")
            border.color: "#505050"
            border.width: 1
            z: 2

            Image {
                anchors.centerIn: parent
                width: 10
                height: 10
                source: Qt.resolvedUrl("../icons/play_arrow.svg")
                sourceSize: Qt.size(20, 20)
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: playButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: gridDel.playButtonClicked(gridDel.groupType, gridDel.groupValue)
            }
        }
    }
}
