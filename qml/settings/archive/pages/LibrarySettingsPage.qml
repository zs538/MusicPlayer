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
            title: qsTr("Watch Folders")
            Layout.fillWidth: true
            
            ListView {
                id: folderList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 200)
                interactive: false
                
                model: AppViewModel.watchFolders
                
                delegate: RowLayout {
                    width: folderList.width
                    required property string modelData
                    
                    Label {
                        text: modelData
                        elide: Text.ElideMiddle
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    
                    Button {
                        text: "✕"
                        flat: true
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: AppViewModel.removeLibraryFolder(modelData)
                    }
                }
            }
            
            Button {
                text: qsTr("Add folder...")
                onClicked: folderDialog.open()
            }
        }
        
        SettingsGroup {
            title: qsTr("Library Status")
            Layout.fillWidth: true
            
            RowLayout {
                spacing: Theme.spacingMedium
                
                Button {
                    text: qsTr("Rescan Library")
                    enabled: !AppViewModel.libraryScanning
                    onClicked: AppViewModel.rescanLibrary()
                }
                
                Label {
                    text: AppViewModel.libraryScanning 
                        ? qsTr("Scanning... %1%").arg(AppViewModel.libraryScanProgress)
                        : qsTr("%1 tracks").arg(AppViewModel.libraryTrackCount)
                    color: Theme.textSecondary
                }
            }
        }
        
        Item { Layout.fillHeight: true }
    }
    
    Dialog {
        id: folderDialog
        title: qsTr("Add Watch Folder")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        
        TextField {
            id: folderPathField
            width: 300
            placeholderText: qsTr("Enter folder path...")
        }
        
        onAccepted: {
            if (folderPathField.text.trim().length > 0) {
                AppViewModel.addLibraryFolder(folderPathField.text.trim())
                folderPathField.text = ""
            }
        }
    }
}
