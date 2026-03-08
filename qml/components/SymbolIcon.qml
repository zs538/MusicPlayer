import QtQuick
import MusicPlayer

Text {
    id: root
    property string source: ""
    property color iconColor: Theme.textPrimary

    color: root.iconColor
    text: {
        const key = root.source.split("/").pop().replace(".svg", "")
        switch (key) {
        case "skip_previous": return "⏮"
        case "play_arrow": return "▶"
        case "pause": return "⏸"
        case "stop": return "⏹"
        case "skip_next": return "⏭"
        case "settings": return "⚙"
        case "arrow_back": return "←"
        case "arrow_forward": return "→"
        case "arrow_upward": return "↑"
        case "home": return "⌂"
        case "folder": return "📁"
        case "queue_music": return "≡"
        case "music_note": return "♪"
        case "album": return "◼"
        case "expand_more": return "▾"
        case "chevron_right": return "▸"
        case "more_vert": return "⋮"
        case "volume_up": return "🔊"
        default: return "?"
        }
    }
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    font.pixelSize: Math.max(10, Math.min(width, height))
    font.family: "Segoe UI Symbol"
    renderType: Text.NativeRendering
}
