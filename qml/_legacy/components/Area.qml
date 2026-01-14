import QtQuick
import QtQuick.Controls
import MusicPlayer 1.0

Item {
    id: root
    
    // Area identification
    property string areaId: ""
    property string role: ""
    
    // Navigation mode: "replaceNoHistory" or "replaceWithHistory"
    property string navigationMode: "replaceNoHistory"
    
    // Panel loading
    property string panelSource: ""
    property var panelProperties: ({})
    
    // Focus tracking
    property bool areaFocused: activeFocus || (loader.item && loader.item.activeFocus)
    
    // Signals forwarded from panels
    signal settingsRequested(string path)
    
    // Loaded panel reference
    readonly property var panel: loader.item
    
    // Background and border
    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        border.color: root.areaFocused ? Theme.accent : Theme.border
        border.width: Theme.borderWidth
    }
    
    Loader {
        id: loader
        anchors.fill: parent
        anchors.margins: Theme.borderWidth
        source: root.panelSource
        
        onLoaded: {
            if (item) {
                // Set panel properties
                for (var key in root.panelProperties) {
                    if (item.hasOwnProperty(key)) {
                        item[key] = root.panelProperties[key]
                    }
                }
                
                // Connect panel signals to router
                if (item.openRequested) {
                    item.openRequested.connect(function(request) {
                        // Forward to PanelRouter when implemented
                        console.log("Area", root.areaId, "received openRequested:", JSON.stringify(request))
                    })
                }
                
                if (item.settingsRequested) {
                    item.settingsRequested.connect(function(path) {
                        root.settingsRequested(path)
                    })
                }
            }
        }
    }
    
    // Focus handling
    onAreaFocusedChanged: {
        if (areaFocused && loader.item && loader.item.hasOwnProperty("isFocused")) {
            loader.item.isFocused = true
        }
    }
    
    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) {
            root.forceActiveFocus()
            mouse.accepted = false
        }
    }
}
