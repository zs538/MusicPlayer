pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#f5f5f5"
    readonly property color surface: "#ffffff"
    readonly property color surfaceAlt: "#fafafa"
    readonly property color border: "#e0e0e0"
    readonly property color borderStrong: "#cccccc"
    
    readonly property color textPrimary: "#1a1a1a"
    readonly property color textSecondary: "#666666"
    readonly property color textMuted: "#999999"
    readonly property color textDisabled: "#bbbbbb"
    
    readonly property color accent: "#1565c0"
    readonly property color accentLight: "#e3f2fd"
    readonly property color accentDark: "#0d47a1"
    
    readonly property color success: "#2e7d32"
    readonly property color successBg: "#e8f5e9"
    readonly property color error: "#c62828"
    readonly property color errorBg: "#ffebee"
    
    readonly property color hover: "#f0f0f0"
    readonly property color selected: "#e6e6e6"
    readonly property color pressed: "#d0d0d0"
    
    readonly property int radiusNone: 0
    readonly property int radiusSmall: 2
    
    readonly property int spacingTiny: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24
    
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeMedium: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeTitle: 20
    
    readonly property int borderWidth: 1
    
    readonly property int controlHeight: 28
    readonly property int controlHeightLarge: 36
    readonly property int listItemHeight: 32
}
