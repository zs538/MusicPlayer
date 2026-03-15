import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import QtCore
import MusicPlayer

Window {
    id: root
    readonly property int defaultWidth: 500
    readonly property int defaultHeight: 680
    property int periodicRescanFallbackMinutes: Settings.periodicRescanMinutes > 0 ? Settings.periodicRescanMinutes : 10
    property bool spinBoxEditorActive: false

    width: defaultWidth
    height: defaultHeight
    minimumWidth: defaultWidth
    minimumHeight: 520
    title: qsTr("Settings")
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    modality: Qt.NonModal
    color: Theme.background

    function resetGeometry(referenceWindow) {
        width = defaultWidth
        height = defaultHeight
        if (referenceWindow) {
            x = referenceWindow.x + (referenceWindow.width - width) / 2
            y = referenceWindow.y + (referenceWindow.height - height) / 2
        } else if (Screen) {
            x = (Screen.width - width) / 2
            y = (Screen.height - height) / 2
        }
    }

    component CategoryHeader: Label {
        font.bold: true
        font.pixelSize: 14
        color: Theme.textPrimary
        Layout.fillWidth: true
    }

    component CategoryRule: Rectangle {
        implicitHeight: 1
        color: Theme.border
        Layout.fillWidth: true
    }

    component CategorySection: ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
    }

    component SettingGroup: ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 2
        Layout.bottomMargin: 2
        spacing: 4
    }

    component SettingsSpinBox: SpinBox {
        id: control
        property bool cancelingEdit: false
        signal valueCommitted(int committedValue)

        editable: true
        live: false

        function commitEditorText() {
            const committedValue = valueFromText(editor.text, locale)
            value = committedValue
            valueCommitted(value)
            editor.text = textFromValue(value, locale)
        }

        onValueModified: valueCommitted(value)

        onValueChanged: {
            if (!editor.activeFocus)
                editor.text = textFromValue(value, locale)
        }

        contentItem: TextInput {
            id: editor
            text: control.textFromValue(control.value, control.locale)
            font: control.font
            color: control.palette.text
            selectionColor: control.palette.highlight
            selectedTextColor: control.palette.highlightedText
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !control.editable
            validator: control.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            selectByMouse: true

            onActiveFocusChanged: {
                root.spinBoxEditorActive = activeFocus
                if (activeFocus) {
                    selectAll()
                } else {
                    if (control.cancelingEdit) {
                        control.cancelingEdit = false
                        text = control.textFromValue(control.value, control.locale)
                    } else {
                        control.commitEditorText()
                    }
                }
            }

            onAccepted: {
                control.commitEditorText()
                pageFlickable.forceActiveFocus()
            }

            Keys.onEscapePressed: function(event) {
                control.cancelingEdit = true
                text = control.textFromValue(control.value, control.locale)
                pageFlickable.forceActiveFocus()
                event.accepted = true
            }
        }
    }

    onVisibleChanged: {
        if (visible)
            refreshWatchFolders()
    }

    function refreshWatchFolders() {
        folderModel.clear()
        const folders = AppViewModel.watchFolders
        for (let i = 0; i < folders.length; ++i)
            folderModel.append({ path: folders[i] })
    }

    function openLibraryFolderDialog(referenceWindow) {
        if (!visible) {
            resetGeometry(referenceWindow)
            show()
        }
        raise()
        requestActivate()
        Qt.callLater(function() {
            folderDialog.open()
        })
    }

    ListModel {
        id: folderModel
    }

    Connections {
        target: AppViewModel
        function onLibraryFoldersChanged() {
            root.refreshWatchFolders()
        }
    }

    Connections {
        target: Settings
        function onPeriodicRescanMinutesChanged() {
            if (Settings.periodicRescanMinutes > 0)
                root.periodicRescanFallbackMinutes = Settings.periodicRescanMinutes
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        context: Qt.WindowShortcut
        enabled: root.visible && root.active && !root.spinBoxEditorActive
        onActivated: root.close()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    Flickable {
        id: pageFlickable
        anchors.fill: parent
        anchors.topMargin: 1
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + 24
        boundsBehavior: Flickable.StopAtBounds
        boundsMovement: Flickable.StopAtBounds
        interactive: false
        flickableDirection: Flickable.VerticalFlick

        Behavior on contentY {
            NumberAnimation {
                duration: 110
                easing.type: Easing.OutCubic
            }
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (wheel) => {
                const rawDelta = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y * 0.8
                const maxY = Math.max(0, pageFlickable.contentHeight - pageFlickable.height)
                pageFlickable.contentY = Math.max(0, Math.min(maxY, pageFlickable.contentY - rawDelta))
                wheel.accepted = true
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        ColumnLayout {
            id: contentColumn
            x: 12
            y: 12
            width: pageFlickable.width - 24
            spacing: 18

            CategorySection {
                CategoryHeader {
                    text: "Collection"
                }
                CategoryRule {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 12

                    SettingGroup {
                        Label {
                            text: "Collection Folders"
                            font.bold: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            border.width: 1
                            border.color: Theme.border
                            color: Theme.background
                            clip: true

                            ListView {
                                id: folderListView
                                anchors.fill: parent
                                anchors.margins: 2
                                model: folderModel
                                interactive: false
                                boundsBehavior: Flickable.StopAtBounds
                                boundsMovement: Flickable.StopAtBounds

                                Behavior on contentY {
                                    NumberAnimation {
                                        duration: 110
                                        easing.type: Easing.OutCubic
                                    }
                                }

                                delegate: Rectangle {
                                    width: folderListView.width
                                    height: 24
                                    color: libraryPage.selectedIdx === index ? Theme.accent : (index % 2 === 0 ? "transparent" : Qt.rgba(0, 0, 0, 0.03))

                                    required property string path
                                    required property int index

                                    Label {
                                        anchors.fill: parent
                                        anchors.leftMargin: 6
                                        anchors.rightMargin: 6
                                        text: path
                                        elide: Text.ElideMiddle
                                        verticalAlignment: Text.AlignVCenter
                                        color: libraryPage.selectedIdx === index ? "white" : Theme.textPrimary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: libraryPage.selectedIdx = index
                                    }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: "(no folders added)"
                                    color: Theme.textSecondary
                                    visible: folderListView.count === 0
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                hoverEnabled: true
                                onWheel: (wheel) => {
                                    const maxY = Math.max(0, folderListView.contentHeight - folderListView.height)
                                    if (maxY <= 0) {
                                        wheel.accepted = false
                                        return
                                    }
                                    const rawDelta = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y * 0.8
                                    folderListView.contentY = Math.max(0, Math.min(maxY, folderListView.contentY - rawDelta))
                                    wheel.accepted = true
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: "Add Folder..."
                                PointingCursor {}
                                onClicked: folderDialog.open()
                            }

                            Button {
                                text: "Remove"
                                enabled: libraryPage.selectedIdx >= 0 && libraryPage.selectedIdx < folderModel.count
                                PointingCursor {}
                                onClicked: {
                                    if (libraryPage.selectedIdx >= 0 && libraryPage.selectedIdx < folderModel.count) {
                                        const path = folderModel.get(libraryPage.selectedIdx).path
                                        libraryPage.selectedIdx = -1
                                        AppViewModel.removeLibraryFolder(path)
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: "Rescan Collection"
                                enabled: !AppViewModel.libraryScanning
                                PointingCursor {}
                                onClicked: AppViewModel.rescanLibrary()
                            }

                            Label {
                                text: AppViewModel.libraryScanning
                                    ? "Scanning... " + AppViewModel.libraryScanProgress + " files processed"
                                    : AppViewModel.libraryTrackCount + " tracks in collection"
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Collection Updates"
                            font.bold: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            CheckBox {
                                text: "Enable filesystem watcher"
                                checked: Settings.watcherEnabled
                                PointingCursor {}
                                onClicked: Settings.watcherEnabled = checked
                            }

                            RowLayout {
                                enabled: Settings.watcherEnabled
                                spacing: 8

                                CheckBox {
                                    id: periodicRescanCheckBox
                                    text: "Enable periodic rescan"
                                    checked: Settings.periodicRescanMinutes > 0
                                    PointingCursor {}
                                    onClicked: Settings.periodicRescanMinutes = checked ? root.periodicRescanFallbackMinutes : 0
                                }

                                SettingsSpinBox {
                                    from: 1
                                    to: 1440
                                    stepSize: 1
                                    value: Settings.periodicRescanMinutes > 0 ? Settings.periodicRescanMinutes : root.periodicRescanFallbackMinutes
                                    enabled: periodicRescanCheckBox.checked
                                    Layout.preferredWidth: 90
                                    PointingCursor {}
                                    onValueCommitted: {
                                        root.periodicRescanFallbackMinutes = committedValue
                                        if (Settings.periodicRescanMinutes > 0)
                                            Settings.periodicRescanMinutes = committedValue
                                    }
                                }

                                Label {
                                    text: "min"
                                    color: Theme.textSecondary
                                    enabled: periodicRescanCheckBox.checked
                                }
                            }
                        }

                        Label {
                            text: "Periodic rescan keeps track of metadata changes the filesystem watcher can't see."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    FolderDialog {
                        id: folderDialog
                        title: "Select Collection Folder"
                        onAccepted: {
                            const path = String(selectedFolder)
                            AppViewModel.addLibraryFolder(path)
                        }
                    }
                }
            }

            CategorySection {
                CategoryHeader {
                    text: "Behaviour"
                }
                CategoryRule {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 12

                    SettingGroup {
                        Label {
                            text: "Adding track(s) will"
                            font.bold: true
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Never start playing", "Play only if the playback is stopped", "Always start playing"]
                            currentIndex: Settings.addTracksPolicy
                            PointingCursor {}
                            onCurrentIndexChanged: Settings.addTracksPolicy = currentIndex
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Pressing previous button will"
                            font.bold: true
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: ["Jump to previous track right away", "Restart song, then jump to the previous one if pressed again"]
                            currentIndex: Settings.previousButtonAction
                            PointingCursor {}
                            onCurrentIndexChanged: Settings.previousButtonAction = currentIndex
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Opening track(s) will"
                            font.bold: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RadioButton {
                                text: "Append to viewed playlist"
                                checked: Settings.openingTracksAction === 0
                                PointingCursor {}
                                onClicked: Settings.openingTracksAction = 0
                            }

                            RadioButton {
                                text: "Generate a new playlist"
                                checked: Settings.openingTracksAction === 1
                                enabled: Settings.generatedPlaylistsEnabled
                                PointingCursor {}
                                onClicked: Settings.openingTracksAction = 1
                            }
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Generated Playlists"
                            font.bold: true
                        }

                        RowLayout {
                            spacing: 8

                            CheckBox {
                                text: "Enable generated playlists"
                                checked: Settings.generatedPlaylistsEnabled
                                PointingCursor {}
                                onClicked: {
                                    Settings.generatedPlaylistsEnabled = checked
                                    if (!checked && Settings.openingTracksAction === 1)
                                        Settings.openingTracksAction = 0
                                }
                            }

                            SettingsSpinBox {
                                from: 1
                                to: 20
                                value: Settings.generatedPlaylistCount
                                enabled: Settings.generatedPlaylistsEnabled
                                Layout.preferredWidth: 80
                                PointingCursor {}
                                onValueCommitted: Settings.generatedPlaylistCount = committedValue
                            }
                        }

                        Label {
                            text: "Sets the maximum number of generated playlists the app may create."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Collection Browser"
                            font.bold: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            CheckBox {
                                text: "Open group on single click"
                                checked: Settings.collectionSingleClickOpen
                                PointingCursor {}
                                onClicked: Settings.collectionSingleClickOpen = checked
                            }

                            CheckBox {
                                text: "Display play button"
                                checked: Settings.collectionPlayButtonEnabled
                                enabled: Settings.generatedPlaylistsEnabled
                                PointingCursor {}
                                onClicked: Settings.collectionPlayButtonEnabled = checked
                            }

                            Label {
                                text: "Play button starts playing always, in new generated playlist."
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            CategorySection {
                CategoryHeader {
                    text: "Appearance"
                }
                CategoryRule {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 12

                    SettingGroup {
                        Label {
                            text: "Grid Cell Width"
                            font.bold: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                spacing: 4

                                Label {
                                    text: "Minimum:"
                                    Layout.preferredWidth: 64
                                    horizontalAlignment: Text.AlignLeft
                                }

                                SettingsSpinBox {
                                    from: 60
                                    to: 300
                                    stepSize: 10
                                    value: Settings.gridCellMinWidth
                                    Layout.preferredWidth: 84
                                    PointingCursor {}
                                    onValueCommitted: Settings.gridCellMinWidth = committedValue
                                }

                                Label {
                                    text: "px"
                                    color: Theme.textSecondary
                                }
                            }

                            RowLayout {
                                spacing: 4

                                Label {
                                    text: "Maximum:"
                                    Layout.preferredWidth: 64
                                    horizontalAlignment: Text.AlignLeft
                                }

                                SettingsSpinBox {
                                    from: 80
                                    to: 400
                                    stepSize: 10
                                    value: Settings.gridCellMaxWidth
                                    Layout.preferredWidth: 84
                                    PointingCursor {}
                                    onValueCommitted: Settings.gridCellMaxWidth = committedValue
                                }

                                Label {
                                    text: "px"
                                    color: Theme.textSecondary
                                }
                            }
                        }

                        Label {
                            text: "Keep at least 20px apart for best performance."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            CategorySection {
                CategoryHeader {
                    text: "Other"
                }
                CategoryRule {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 12

                    SettingGroup {
                        Label {
                            text: "Buffer Size"
                            font.bold: true
                        }

                        RowLayout {
                            SettingsSpinBox {
                                id: bufferSpinBox
                                from: 50
                                to: 500
                                stepSize: 10
                                value: Settings.bufferSizeMs
                                PointingCursor {}
                                onValueCommitted: Settings.bufferSizeMs = committedValue
                            }

                            Label {
                                text: "ms"
                                color: Theme.textSecondary
                            }
                        }

                        Label {
                            text: "Lower = less latency, higher = more stable."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Gapless Lead-in Window"
                            font.bold: true
                        }

                        RowLayout {
                            SettingsSpinBox {
                                id: gaplessLeadInSpinBox
                                from: 100
                                to: 10000
                                stepSize: 100
                                value: Settings.gaplessLeadInMs
                                PointingCursor {}
                                onValueCommitted: Settings.gaplessLeadInMs = committedValue
                            }

                            Label {
                                text: "ms"
                                color: Theme.textSecondary
                            }
                        }

                        Label {
                            text: "If too low some slower systems may not load the next track in time."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    SettingGroup {
                        Label {
                            text: "Startup"
                            font.bold: true
                        }

                        CheckBox {
                            text: "Restore session on startup"
                            checked: Settings.restoreSession
                            PointingCursor {}
                            onClicked: Settings.restoreSession = checked
                        }
                    }
                }
            }

            CategorySection {
                CategoryHeader {
                    text: "About"
                }
                CategoryRule {}

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 12

                    SettingGroup {
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Image {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 200
                                source: "../logo-image-t.png"
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 400
                                sourceSize.height: 400
                                asynchronous: true
                                cache: true
                                smooth: true
                            }

                            Label {
                                text: "Version: " + AppVersion
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                Layout.alignment: Qt.AlignTop
                            }
                        }
                    }
                }
            }
        }
    }

    QtObject {
        id: libraryPage
        property int selectedIdx: -1
    }
}
