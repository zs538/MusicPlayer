import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

RowLayout {
    id: root
    
    property string label: ""
    property alias checked: checkBox.checked
    
    signal toggled(bool checked)
    
    spacing: Theme.spacingSmall
    
    CheckBox {
        id: checkBox
        onClicked: root.toggled(checked)
    }
    
    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        Layout.fillWidth: true
        
        MouseArea {
            anchors.fill: parent
            onClicked: {
                checkBox.checked = !checkBox.checked
                root.toggled(checkBox.checked)
            }
        }
    }
}
