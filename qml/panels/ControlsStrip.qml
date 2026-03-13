import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

Rectangle {
    id: root
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    implicitHeight: 32
    property real lastNonZeroVolume: Settings.volume > 0 ? Settings.volume : 1.0

    component PointingCursor: HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    component SectionSeparator: Rectangle {
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        Layout.topMargin: 6
        Layout.bottomMargin: 6
        color: Theme.border
        opacity: 0.9
    }

    component StyledHoverToolTip: ToolTip {
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

    component FlatSlider: Slider {
        id: control
        implicitHeight: 20
        leftPadding: 0
        rightPadding: 0

        background: Item {
            implicitHeight: 16

            Rectangle {
                x: 0
                y: (parent.height - 5) / 2
                width: parent.width
                height: 5
                radius: Theme.radiusNone
                color: "#cfcfcf"
                border.width: 1
                border.color: "#a3a3a3"
            }

            Rectangle {
                x: 0
                y: (parent.height - 5) / 2
                width: parent.width * control.visualPosition
                height: 5
                radius: Theme.radiusNone
                color: "#313131"
                border.width: 1
                border.color: "#3d3d3d"
            }
        }

        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: (control.height - height) / 2
            width: 7
            height: 13
            radius: Theme.radiusNone
            color: "#f5f5f5"
            border.width: 1
            border.color: "#4a4a4a"
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 8

        // Left: Transport buttons
        RowLayout {
            spacing: 2

            Button {
                id: backButton
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                hoverEnabled: true
                icon.source: Qt.resolvedUrl("../icons/skip_previous.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                StyledHoverToolTip {
                    parent: backButton
                    visible: backButton.hovered
                    delay: 800
                    timeout: 5000
                    text: "Back"
                }
                PointingCursor {}
                onClicked: AppViewModel.previous()
            }

            Button {
                id: playPauseButton
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                hoverEnabled: true
                icon.source: AppViewModel.playbackState === AppViewModel.Playing
                    ? Qt.resolvedUrl("../icons/pause.svg")
                    : Qt.resolvedUrl("../icons/play_arrow.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                StyledHoverToolTip {
                    parent: playPauseButton
                    visible: playPauseButton.hovered
                    delay: 800
                    timeout: 5000
                    text: "Play/Pause"
                }
                PointingCursor {}
                onClicked: {
                    if (AppViewModel.playbackState === AppViewModel.Playing)
                        AppViewModel.pause()
                    else
                        AppViewModel.play()
                }
            }

            Button {
                id: stopButton
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                hoverEnabled: true
                icon.source: Qt.resolvedUrl("../icons/stop.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                StyledHoverToolTip {
                    parent: stopButton
                    visible: stopButton.hovered
                    delay: 800
                    timeout: 5000
                    text: "Stop"
                }
                PointingCursor {}
                onClicked: AppViewModel.stop()
            }

            Button {
                id: skipButton
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                hoverEnabled: true
                icon.source: Qt.resolvedUrl("../icons/skip_next.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                StyledHoverToolTip {
                    parent: skipButton
                    visible: skipButton.hovered
                    delay: 800
                    timeout: 5000
                    text: "Skip"
                }
                PointingCursor {}
                onClicked: AppViewModel.next()
            }
        }

        SectionSeparator {}

        // Track info
        Label {
            text: {
                let model = ViewedPlaylistRouter.viewedPlaylistModel
                let count = model ? model.count : 0
                let duration = model ? formatTime(model.totalDurationMs) : "0:00"
                return qsTr("Track(s): %1 [%2]").arg(count).arg(duration)
            }
            color: Theme.textSecondary
            font.pixelSize: 10
        }

        SectionSeparator {}

        // Center: Seek bar with times
        Label {
            text: formatTime(progressSlider.displayValue)
            color: Theme.textPrimary
            font.pixelSize: 10
        }

        FlatSlider {
            id: progressSlider
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            from: 0
            to: AppViewModel.durationMs > 0 ? AppViewModel.durationMs : 1
            value: displayValue
            hoverEnabled: true
            PointingCursor {}

            property bool seeking: false
            property real displayValue: AppViewModel.positionMs

            onPressedChanged: {
                if (pressed) {
                    displayValue = value
                    return
                }

                if (!pressed && seeking) {
                    displayValue = value
                    AppViewModel.seek(displayValue)
                    seeking = false
                }
            }

            onMoved: {
                seeking = true
                displayValue = value
            }

            Binding {
                target: progressSlider
                property: "displayValue"
                value: AppViewModel.positionMs
                when: !progressSlider.pressed && !progressSlider.seeking
            }
        }

        Label {
            text: formatTime(AppViewModel.durationMs)
            color: Theme.textPrimary
            font.pixelSize: 10
        }

        SectionSeparator {}

        // Right: Volume
        Item {
            id: volumeButton
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14

            Image {
                anchors.fill: parent
                source: Settings.volume > 0
                    ? Qt.resolvedUrl("../icons/volume_up.svg")
                    : Qt.resolvedUrl("../icons/volume_off.svg")
                sourceSize: Qt.size(28, 28)
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: muteMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                StyledHoverToolTip {
                    parent: muteMouseArea
                    visible: muteMouseArea.containsMouse
                    delay: 800
                    timeout: 5000
                    text: "Mute"
                }
                onClicked: {
                    if (Settings.volume > 0) {
                        root.lastNonZeroVolume = Settings.volume
                        Settings.volume = 0
                    } else {
                        Settings.volume = root.lastNonZeroVolume > 0 ? root.lastNonZeroVolume : 1.0
                    }
                }
            }
        }

        FlatSlider {
            id: volumeSlider
            Layout.preferredWidth: 80
            Layout.preferredHeight: 20
            from: 0
            to: 1
            value: Settings.volume
            hoverEnabled: true
            PointingCursor {}
            onMoved: {
                Settings.volume = value
                if (value > 0) {
                    root.lastNonZeroVolume = value
                }
            }
        }

        Label {
            text: Math.round(Settings.volume * 100) + "%"
            color: Theme.textSecondary
            font.pixelSize: 10
            Layout.preferredWidth: 30
            horizontalAlignment: Text.AlignHCenter
        }

        SectionSeparator {}

        Button {
            id: settingsButton
            flat: true
            implicitWidth: 24
            implicitHeight: 24
            hoverEnabled: true
            Layout.leftMargin: -4

            contentItem: Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../icons/settings.svg")
                sourceSize: Qt.size(32, 32)
                fillMode: Image.PreserveAspectFit
                opacity: settingsButton.pressed ? 0.75 : (settingsButton.hovered ? 0.9 : 1.0)
            }

            background: Rectangle {
                color: "transparent"
                border.width: 0
            }

            StyledHoverToolTip {
                parent: settingsButton
                visible: settingsButton.hovered
                delay: 800
                timeout: 5000
                text: "Settings"
            }
            PointingCursor {}
            onClicked: settingsRequested()
        }
    }

    signal settingsRequested()

    function formatTime(ms) {
        let secs = Math.floor(ms / 1000)
        let mins = Math.floor(secs / 60)
        let hours = Math.floor(mins / 60)
        mins = mins % 60
        secs = secs % 60
        if (hours > 0)
            return hours + ":" + String(mins).padStart(2, '0') + ":" + String(secs).padStart(2, '0')
        return mins + ":" + String(secs).padStart(2, '0')
    }
}
