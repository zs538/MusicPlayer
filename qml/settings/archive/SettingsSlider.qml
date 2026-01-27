import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

RowLayout {
    id: root
    
    property string label: ""
    property alias value: slider.value
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    property string suffix: ""
    property int labelWidth: 120
    
    signal moved(real value)
    
    spacing: Theme.spacingSmall
    
    Label {
        text: root.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeNormal
        Layout.preferredWidth: root.labelWidth
    }
    
    Slider {
        id: slider
        Layout.fillWidth: true
        onMoved: root.moved(value)
    }
    
    Label {
        text: Math.round(slider.value) + root.suffix
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeNormal
        Layout.preferredWidth: 50
        horizontalAlignment: Text.AlignRight
    }
}
