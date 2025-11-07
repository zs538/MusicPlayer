import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 900
    height: 600
    title: "Music Player (MVP)"

    FileDialog {
        id: fileDialog
        title: "Open Audio File"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.openFile(selectedFile)
    }

    FileDialog {
        id: nextDialog
        title: "Select Next Track (Gapless)"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.setNextFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            spacing: 8
            Button { text: "Open"; onClicked: fileDialog.open() }
            Button { text: "Set Next"; onClicked: nextDialog.open() }
            Button { text: player.playing ? "Pause" : "Play"; onClicked: player.playing ? player.pause() : player.play() }
            ComboBox {
                id: outputBox
                Layout.preferredWidth: 240
                model: player.audioOutputs
                onActivated: player.selectOutputByIndex(index)
                Component.onCompleted: currentIndex = Math.max(0, player.audioOutputs.indexOf(player.currentOutput))
                ToolTip.visible: hovered
                ToolTip.text: `Output: ${player.currentOutput}`
            }
            Slider {
                id: pos
                Layout.fillWidth: true
                from: 0
                to: player.duration
                value: player.position
                onMoved: player.seek(value)
            }
            Slider {
                id: vol
                from: 0
                to: 1
                value: 0.8
                stepSize: 0.01
                onValueChanged: player.volume = value
                implicitWidth: 150
            }
        }

        Label {
            text: `Position: ${Math.round(player.position/1000)}s / ${Math.round(player.duration/1000)}s`
        }

        // Diagnostics row removed after fix; ComboBox remains for device selection.

        Rectangle {
            color: "#202020"
            radius: 8
            Layout.fillWidth: true
            Layout.fillHeight: true
            border.color: "#404040"
            Text { anchors.centerIn: parent; color: "#aaaaaa"; text: "Collection/Queue UI will go here" }
        }
    }
}
