import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer
import ".."

ScrollView {
    id: root
    
    contentWidth: availableWidth
    
    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spacingLarge
        
        SettingsGroup {
            title: qsTr("Playback Mode")
            Layout.fillWidth: true
            
            SettingsRadioGroup {
                options: [
                    {text: qsTr("Gapless Session"), value: 0},
                    {text: qsTr("Bit-Perfect (Same Rate)"), value: 1}
                ]
                currentValue: Settings.playbackMode
                onValueChanged: function(value) { Settings.playbackMode = value }
            }
        }
        
        SettingsGroup {
            title: qsTr("Audio Buffer")
            Layout.fillWidth: true
            
            SettingsSlider {
                label: qsTr("Buffer size:")
                from: 10
                to: 1000
                stepSize: 10
                value: Settings.bufferSizeMs
                suffix: " ms"
                Layout.fillWidth: true
                onMoved: function(value) { Settings.bufferSizeMs = value }
            }
        }
        
        Item { Layout.fillHeight: true }
    }
}
