import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MusicPlayer

Rectangle {
    id: root

    property var browserModel: null
    signal searchFieldEscapePressed()
    signal searchFieldAccepted()
    property bool showBreadcrumbHomeButton: true
    property var customGroupKeys: []
    property string currentOpenAction: "openPanel"

    signal jumpToBreadcrumb(int index)
    signal sortOptionSelected(string key)
    signal sortAscendingToggled(bool ascending)
    signal subtitleOptionSelected(string key)
    signal openActionChanged(string action)
    signal searchFilterChanged(string text)

    // Toolbar needs to expose the search field so the host can wire focus-loss logic
    readonly property alias searchField: searchField

    function openSortMenu() {
        sortMenu.popup()
        if (sortMenu.count > 0)
            sortMenu.currentIndex = 0
    }
    function openGroupByMenu() {
        groupByMenu.popup()
        if (groupByMenu.count > 0)
            groupByMenu.currentIndex = 0
    }
    function focusSearchField() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    component TopStripIconButton: Item {
        id: control

        property url iconSource: ""
        property real iconSize: 14
        property real idleOpacity: 0.9
        property bool interactive: true
        property real horizontalInset: 0
        property string toolTipText: ""

        signal clicked()

        implicitWidth: 24 - horizontalInset
        implicitHeight: 24
        width: implicitWidth
        height: implicitHeight

        Image {
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            source: control.iconSource
            sourceSize: Qt.size(control.iconSize * 2, control.iconSize * 2)
            opacity: buttonMouseArea.pressed ? Math.max(0.4, control.idleOpacity - 0.18) : control.idleOpacity
        }

        HoverHandler {
            enabled: control.interactive
            cursorShape: Qt.PointingHandCursor
        }

        MouseArea {
            id: buttonMouseArea
            anchors.fill: parent
            enabled: control.interactive
            hoverEnabled: true
            cursorShape: control.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
            StyledHoverToolTip {
                parent: buttonMouseArea
                visible: buttonMouseArea.containsMouse && control.toolTipText.length > 0
                delay: 800
                timeout: 5000
                text: control.toolTipText
            }
            onClicked: control.clicked()
        }
    }

    height: 24
    color: Theme.background

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        spacing: 4

        Item {
            id: breadcrumbContainer
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            clip: true

            Row {
                anchors.fill: parent
                spacing: 0

                TopStripIconButton {
                    visible: root.showBreadcrumbHomeButton
                    iconSource: Qt.resolvedUrl("../icons/home.svg")
                    iconSize: 14
                    horizontalInset: 2
                    toolTipText: "Root"
                    onClicked: root.jumpToBreadcrumb(0)
                }

                Repeater {
                    model: root.browserModel ? (root.showBreadcrumbHomeButton ? Math.max(0, root.browserModel.breadcrumbPath.length - 1) : root.browserModel.breadcrumbPath.length) : 0

                    delegate: Item {
                        required property int index

                        property int breadcrumbIndex: root.showBreadcrumbHomeButton ? index + 1 : index
                        property var crumb: root.browserModel.breadcrumbPath[breadcrumbIndex]
                        property bool isFuture: breadcrumbIndex > root.browserModel.currentBreadcrumbIndex
                        property bool showSeparator: breadcrumbIndex > 0
                        width: crumbLabel.width + (showSeparator ? separatorIcon.width + 8 : 0)
                        height: 24

                        Image {
                            id: separatorIcon
                            visible: parent.showSeparator
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            source: Qt.resolvedUrl("../icons/chevron_right.svg")
                            sourceSize: Qt.size(24, 24)
                            opacity: parent.isFuture ? 0.45 : 0.75
                        }

                        Text {
                            id: crumbLabel
                            anchors.left: parent.showSeparator ? separatorIcon.right : parent.left
                            anchors.leftMargin: parent.showSeparator ? 2 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.crumb.label || ""
                            color: parent.isFuture ? Theme.textDisabled : Theme.textPrimary
                            font.pixelSize: 11
                            font.underline: crumbMouseArea.containsMouse
                            elide: Text.ElideRight
                        }

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        MouseArea {
                            id: crumbMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.jumpToBreadcrumb(parent.breadcrumbIndex)
                        }
                    }
                }
            }
        }

        Item {
            Layout.preferredWidth: searchField.width
            Layout.preferredHeight: 24

            TextField {
                id: searchField
                width: 108
                height: 24
                placeholderText: "Filter..."
                placeholderTextColor: Theme.textSecondary
                color: Theme.textPrimary
                font.pixelSize: 10
                leftPadding: 2
                rightPadding: 18
                topPadding: 1
                bottomPadding: 1
                hoverEnabled: true
                selectByMouse: true
                background: Item {
                    implicitWidth: 108
                    implicitHeight: 24

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: searchField.activeFocus ? "black" : "#aaaaaa"
                    }
                }
                onTextChanged: root.searchFilterChanged(text)
                Keys.onEscapePressed: (event) => {
                    root.searchFieldEscapePressed()
                    event.accepted = true
                }
                Keys.onReturnPressed: (event) => {
                    root.searchFieldAccepted()
                    event.accepted = true
                }
                Keys.onEnterPressed: (event) => {
                    root.searchFieldAccepted()
                    event.accepted = true
                }

                TopStripIconButton {
                    id: searchActionButton
                    z: 1
                    anchors.right: parent.right
                    anchors.rightMargin: 0
                    anchors.verticalCenter: parent.verticalCenter
                    iconSource: searchField.text.length > 0
                        ? Qt.resolvedUrl("../icons/close.svg")
                        : Qt.resolvedUrl("../icons/search.svg")
                    iconSize: searchField.text.length > 0 ? 11 : 14
                    idleOpacity: searchField.activeFocus ? 0.9 : 0.5
                    interactive: searchField.text.length > 0
                    onClicked: searchField.clear()
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 14
            color: Theme.border
        }

        TopStripIconButton {
            id: sortButton
            iconSource: Qt.resolvedUrl("../icons/sort.svg")
            iconSize: 16
            toolTipText: "Sort"
            onClicked: root.openSortMenu()

            Menu {
                id: sortMenu

                Instantiator {
                    model: root.browserModel ? root.browserModel.sortOptions() : []

                    delegate: MenuItem {
                        required property var modelData

                        text: modelData.text
                        checkable: true
                        checked: root.browserModel ? root.browserModel.sortBy === modelData.key : false
                        PointingCursor {}
                        onTriggered: root.sortOptionSelected(modelData.key)
                    }

                    onObjectAdded: function(index, object) {
                        sortMenu.insertItem(index, object)
                    }

                    onObjectRemoved: function(index, object) {
                        sortMenu.removeItem(object)
                    }
                }

                MenuSeparator {}
                MenuItem {
                    text: "Ascending"
                    checkable: true
                    checked: root.browserModel ? root.browserModel.sortAscending : true
                    PointingCursor {}
                    onTriggered: root.sortAscendingToggled(root.browserModel ? !root.browserModel.sortAscending : true)
                }
            }
        }

        TopStripIconButton {
            id: groupByButton
            iconSource: Qt.resolvedUrl("../icons/filter_alt.svg")
            iconSize: 16
            toolTipText: "Group By"
            onClicked: root.openGroupByMenu()

            Menu {
                id: groupByMenu

                property var allOptions: root.browserModel ? root.browserModel.groupByOptions(root.customGroupKeys) : []
                property var mainOptions: allOptions.filter(function(o) { return o.category === "main" })
                property var otherOptions: allOptions.filter(function(o) { return o.category === "other" })
                property var customOptions: allOptions.filter(function(o) { return o.category === "custom" })

                Instantiator {
                    model: groupByMenu.mainOptions

                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        checked: root.browserModel ? root.browserModel.groupBy === modelData.key : false
                        PointingCursor {}
                        onTriggered: root.browserModel.groupBy = modelData.key
                    }

                    onObjectAdded: function(index, object) {
                        groupByMenu.insertItem(index, object)
                    }

                    onObjectRemoved: function(index, object) {
                        groupByMenu.removeItem(object)
                    }
                }

                Menu {
                    id: otherGroupMenu
                    title: "Other"

                    Instantiator {
                        model: groupByMenu.otherOptions

                        delegate: MenuItem {
                            required property var modelData
                            text: modelData.text
                            checkable: true
                            checked: root.browserModel ? root.browserModel.groupBy === modelData.key : false
                            PointingCursor {}
                            onTriggered: root.browserModel.groupBy = modelData.key
                        }

                        onObjectAdded: function(index, object) {
                            otherGroupMenu.insertItem(index, object)
                        }

                        onObjectRemoved: function(index, object) {
                            otherGroupMenu.removeItem(object)
                        }
                    }
                }

                Menu {
                    id: customTagGroupMenu
                    title: "Custom Tag"

                    MenuItem {
                        text: "No custom tags found"
                        enabled: false
                        visible: groupByMenu.customOptions.length === 0
                    }

                    Instantiator {
                        model: groupByMenu.customOptions

                        delegate: MenuItem {
                            required property var modelData
                            text: modelData.text
                            checkable: true
                            checked: root.browserModel ? root.browserModel.groupBy === modelData.key : false
                            PointingCursor {}
                            onTriggered: root.browserModel.groupBy = modelData.key
                        }

                        onObjectAdded: function(index, object) {
                            customTagGroupMenu.insertItem(index, object)
                        }

                        onObjectRemoved: function(index, object) {
                            customTagGroupMenu.removeItem(object)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 14
            color: Theme.border
        }

        TopStripIconButton {
            id: optionsButton
            iconSource: Qt.resolvedUrl("../icons/more_vert.svg")
            iconSize: 16
            toolTipText: "Menu"
            onClicked: optionsMenu.popup()

            Menu {
                id: optionsMenu
                title: "Options"

                Menu {
                    id: subtitleMenu
                    title: "Subtitle"

                    property var allOptions: root.browserModel ? root.browserModel.subtitleOptions(root.customGroupKeys) : []
                    property var mainOptions: allOptions.filter(function(o) { return o.category === "main" })
                    property var otherOptions: allOptions.filter(function(o) { return o.category === "other" })
                    property var customOptions: allOptions.filter(function(o) { return o.category === "custom" })

                    Instantiator {
                        model: subtitleMenu.mainOptions

                        delegate: MenuItem {
                            required property var modelData

                            text: modelData.text
                            checkable: true
                            checked: root.browserModel ? root.browserModel.subtitleKey === modelData.key : false
                            PointingCursor {}
                            onTriggered: root.subtitleOptionSelected(modelData.key)
                        }

                        onObjectAdded: function(index, object) {
                            subtitleMenu.insertItem(index, object)
                        }

                        onObjectRemoved: function(index, object) {
                            subtitleMenu.removeItem(object)
                        }
                    }

                    Menu {
                        id: otherSubtitleMenu
                        title: "Other"

                        Instantiator {
                            model: subtitleMenu.otherOptions

                            delegate: MenuItem {
                                required property var modelData

                                text: modelData.text
                                checkable: true
                                checked: root.browserModel ? root.browserModel.subtitleKey === modelData.key : false
                                PointingCursor {}
                                onTriggered: root.subtitleOptionSelected(modelData.key)
                            }

                            onObjectAdded: function(index, object) {
                                otherSubtitleMenu.insertItem(index, object)
                            }

                            onObjectRemoved: function(index, object) {
                                otherSubtitleMenu.removeItem(object)
                            }
                        }
                    }

                    Menu {
                        id: customSubtitleMenu
                        title: "Custom"

                        MenuItem {
                            text: "No custom tags found"
                            enabled: false
                            visible: subtitleMenu.customOptions.length === 0
                        }

                        Instantiator {
                            model: subtitleMenu.customOptions

                            delegate: MenuItem {
                                required property var modelData

                                text: modelData.text
                                checkable: true
                                checked: root.browserModel ? root.browserModel.subtitleKey === modelData.key : false
                                PointingCursor {}
                                onTriggered: root.subtitleOptionSelected(modelData.key)
                            }

                            onObjectAdded: function(index, object) {
                                customSubtitleMenu.insertItem(index, object)
                            }

                            onObjectRemoved: function(index, object) {
                                customSubtitleMenu.removeItem(object)
                            }
                        }
                    }
                }

                Menu {
                    title: "On double-click"

                    ButtonGroup { id: openActionGroup }

                    MenuItem {
                        text: "Queue tracks"
                        ButtonGroup.group: openActionGroup
                        checkable: true
                        checked: root.currentOpenAction === "queueTracks"
                        PointingCursor {}
                        onTriggered: root.openActionChanged("queueTracks")
                    }
                    MenuItem {
                        text: "Explore"
                        ButtonGroup.group: openActionGroup
                        checkable: true
                        checked: root.currentOpenAction !== "queueTracks"
                        PointingCursor {}
                        onTriggered: root.openActionChanged("openPanel")
                    }
                }
            }
        }
    }
}
