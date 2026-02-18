import QtQuick
import MusicPlayer

// Lightweight SVG icon component. Uses layer + ShaderEffect-free tinting
// by rendering the black SVG and applying a ColorOverlay via multiply blend.
// For buttons, prefer Button { icon.source; icon.color } instead.
Image {
    id: root
    property color iconColor: Theme.textPrimary

    fillMode: Image.PreserveAspectFit
    sourceSize.width: width * 2
    sourceSize.height: height * 2
    smooth: true
}
