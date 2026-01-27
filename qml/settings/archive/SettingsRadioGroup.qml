import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

ColumnLayout {
    id: root
    
    property string label: ""
    property var options: []  // [{text: "Option 1", value: 0}, ...]
    property int currentValue: 0
    
    signal valueChanged(int value)
    
    spacing: Theme.spacingTiny
    
    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        visible: root.label.length > 0
    }
    
    ButtonGroup {
        id: buttonGroup
    }
    
    Repeater {
        model: root.options
        
        RadioButton {
            required property var modelData
            required property int index
            
            text: modelData.text
            checked: modelData.value === root.currentValue
            ButtonGroup.group: buttonGroup
            
            onClicked: root.valueChanged(modelData.value)
        }
    }
}
