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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 8

        // Left: Transport buttons
        RowLayout {
            spacing: 2

            Button {
                text: "⏮"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                font.pixelSize: 12
                onClicked: AppViewModel.previous()
            }

            Button {
                text: AppViewModel.playbackState === AppViewModel.Playing ? "⏸" : "▶"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                font.pixelSize: 14
                onClicked: {
                    if (AppViewModel.playbackState === AppViewModel.Playing)
                        AppViewModel.pause()
                    else
                        AppViewModel.play()
                }
            }

            Button {
                text: "⏹"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                font.pixelSize: 12
                onClicked: AppViewModel.stop()
            }

            Button {
                text: "⏭"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                font.pixelSize: 12
                onClicked: AppViewModel.next()
            }
        }

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

        // Right: Volume
        Slider {
            id: volumeSlider
            Layout.preferredWidth: 80
            Layout.preferredHeight: 20
            from: 0
            to: 1
            value: Settings.volume

            onMoved: {
                Settings.volume = value
            }
        }

        Label {
            text: Math.round(Settings.volume * 100) + "%"
            color: Theme.textSecondary
            font.pixelSize: 10
            Layout.preferredWidth: 30
        }

        // Settings button
        Button {
            text: "⚙"
            flat: true
            implicitWidth: 28
            implicitHeight: 24
            font.pixelSize: 14
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
