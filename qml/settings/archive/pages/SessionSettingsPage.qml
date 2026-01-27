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
            title: qsTr("Session")
            Layout.fillWidth: true
            
            SettingsCheckBox {
                label: qsTr("Restore session on startup")
                checked: Settings.restoreSession
                onToggled: function(checked) { Settings.restoreSession = checked }
            }
        }
        
        Item { Layout.fillHeight: true }
    }
}
