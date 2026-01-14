import QtQuick
import QtQuick.Controls
import MusicPlayer 1.0

Window {
    id: root
    
    property string windowId: ""
    property string panelSource: ""
    property var panelProperties: ({})
    property string areaRole: "floating"
    
    width: 500
    height: 600
    minimumWidth: 300
    minimumHeight: 200
    color: Theme.background
    
    // Make window floating in tiling WMs like i3
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    modality: Qt.NonModal
    transientParent: null
    
    // Generate unique area ID for this floating window
    readonly property string areaId: "floating_" + windowId
    
    Area {
        id: floatingArea
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        areaId: root.areaId
        role: root.areaRole
        panelSource: root.panelSource
        panelProperties: root.panelProperties
    }
    
    onClosing: {
        // Notify router that this window is closing
        console.log("PanelWindow closing:", root.windowId)
    }
}
