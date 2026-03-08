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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 8

        // Left: Transport buttons
        RowLayout {
            spacing: 2

            Button {
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                icon.source: Qt.resolvedUrl("../icons/skip_previous.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                PointingCursor {}
                onClicked: AppViewModel.previous()
            }

            Button {
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                icon.source: AppViewModel.playbackState === AppViewModel.Playing
                    ? Qt.resolvedUrl("../icons/pause.svg")
                    : Qt.resolvedUrl("../icons/play_arrow.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                PointingCursor {}
                onClicked: {
                    if (AppViewModel.playbackState === AppViewModel.Playing)
                        AppViewModel.pause()
                    else
                        AppViewModel.play()
                }
            }

            Button {
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                icon.source: Qt.resolvedUrl("../icons/stop.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
                PointingCursor {}
                onClicked: AppViewModel.stop()
            }

            Button {
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                icon.source: Qt.resolvedUrl("../icons/skip_next.svg")
                icon.color: Theme.textPrimary
                icon.width: 16; icon.height: 16
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
            text: formatTime(AppViewModel.positionMs)
            color: Theme.textPrimary
            font.pixelSize: 10
        }

        Slider {
            id: progressSlider
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            from: 0
            to: AppViewModel.durationMs > 0 ? AppViewModel.durationMs : 1
            value: AppViewModel.positionMs
            hoverEnabled: true
            PointingCursor {}

            property bool seeking: false

            onPressedChanged: {
                if (!pressed && seeking) {
                    AppViewModel.seek(value)
                    seeking = false
                }
            }

            onMoved: {
                seeking = true
            }

            Binding {
                target: progressSlider
                property: "value"
                value: AppViewModel.positionMs
                when: !progressSlider.pressed
            }
        }

        Label {
            text: formatTime(AppViewModel.durationMs)
            color: Theme.textPrimary
            font.pixelSize: 10
        }

        SectionSeparator {}

        // Right: Volume
        Image {
            source: Qt.resolvedUrl("../icons/volume_up.svg")
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            sourceSize: Qt.size(28, 28)
            fillMode: Image.PreserveAspectFit
        }

        Slider {
            id: volumeSlider
            Layout.preferredWidth: 80
            Layout.preferredHeight: 20
            from: 0
            to: 1
            value: Settings.volume
            hoverEnabled: true
            PointingCursor {}
            onMoved: Settings.volume = value
        }

        Label {
            text: Math.round(Settings.volume * 100) + "%"
            color: Theme.textSecondary
            font.pixelSize: 10
            Layout.preferredWidth: 30
        }

        SectionSeparator {}

        // Settings button
        Button {
            flat: true
            implicitWidth: 28
            implicitHeight: 24
            icon.source: Qt.resolvedUrl("../icons/settings.svg")
            icon.color: Theme.textPrimary
            icon.width: 16; icon.height: 16
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
