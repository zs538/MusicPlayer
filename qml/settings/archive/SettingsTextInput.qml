import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

RowLayout {
    id: root
    
    property string label: ""
    property alias text: textField.text
    property alias placeholderText: textField.placeholderText
    property int labelWidth: 120
    
    signal editingFinished()
    
    spacing: Theme.spacingSmall
    
    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        Layout.preferredWidth: root.labelWidth
        visible: root.label.length > 0
    }
    
    TextField {
        id: textField
        Layout.fillWidth: true
        onEditingFinished: root.editingFinished()
    }
}
