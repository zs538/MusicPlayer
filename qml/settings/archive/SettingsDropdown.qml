import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

RowLayout {
    id: root
    
    property string label: ""
    property alias model: comboBox.model
    property alias currentIndex: comboBox.currentIndex
    property alias currentText: comboBox.currentText
    property int labelWidth: 120
    
    signal activated(int index)
    
    spacing: Theme.spacingSmall
    
    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        Layout.preferredWidth: root.labelWidth
    }
    
    ComboBox {
        id: comboBox
        Layout.fillWidth: true
        onActivated: root.activated(index)
    }
}
