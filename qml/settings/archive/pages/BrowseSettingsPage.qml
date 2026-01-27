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
            title: qsTr("Target Policy")
            Layout.fillWidth: true
            
            SettingsRadioGroup {
                options: [
                    {text: qsTr("Append to viewed playlist"), value: 0},
                    {text: qsTr("Replace generated playlist (prefer viewed)"), value: 1},
                    {text: qsTr("Create new playlist"), value: 2}
                ]
                currentValue: Settings.browseTargetPolicy
                onValueChanged: function(value) { Settings.browseTargetPolicy = value }
            }
        }
        
        SettingsGroup {
            title: qsTr("Autoplay Policy")
            Layout.fillWidth: true
            
            SettingsRadioGroup {
                options: [
                    {text: qsTr("Never start playback"), value: 0},
                    {text: qsTr("Start if stopped"), value: 1},
                    {text: qsTr("Always start playback"), value: 2}
                ]
                currentValue: Settings.browseAutoplayPolicy
                onValueChanged: function(value) { Settings.browseAutoplayPolicy = value }
            }
        }
        
        Item { Layout.fillHeight: true }
    }
}
