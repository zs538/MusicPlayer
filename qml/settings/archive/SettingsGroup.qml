import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

ColumnLayout {
    id: root
    
    property string title: ""
    default property alias content: contentColumn.children
    
    spacing: Theme.spacingSmall
    
    Label {
        text: root.title
        font.bold: true
        font.pixelSize: Theme.fontSizeMedium
        color: Theme.textPrimary
        visible: root.title.length > 0
    }
    
    ColumnLayout {
        id: contentColumn
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacingMedium
        spacing: Theme.spacingSmall
    }
}
