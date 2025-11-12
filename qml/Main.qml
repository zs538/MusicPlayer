import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import Qt.labs.folderlistmodel 2.15
import Qt.labs.platform 1.1

ApplicationWindow {
    id: window
    visible: true
    width: 1200
    height: 800
    title: "Music Player"

    // Tracks the index of the currently playing item to handle duplicates
    property int currentIdx: -1
    property int browserMode: 0 // 0 = Collection, 1 = Files

    function indexOfUrl(u) {
        const s = u.toString()
        for (let i = 0; i < playlist.count(); ++i) {
            const it = playlist.get(i)
            if (it && it.url.toString() === s)
                return i
        }
        return -1
    }

    function indexOfUrlFrom(u, startAt) {
        const s = u.toString()
        // forward search from startAt
        for (let i = Math.max(0, startAt); i < playlist.count(); ++i) {
            const it = playlist.get(i)
            if (it && it.url.toString() === s) return i
        }
        // fallback: from 0 to startAt-1
        for (let j = 0; j < Math.min(startAt, playlist.count()); ++j) {
            const it2 = playlist.get(j)
            if (it2 && it2.url.toString() === s) return j
        }
        return -1
    }

    function armNextFromQueue() {
        const idx = currentIdx
        if (idx >= 0 && idx + 1 < playlist.count()) {
            const it = playlist.get(idx + 1)
            if (it) player.setNextFile(it.url)
        } else {
            player.setNextFile("")
        }
    }

    function playIndex(i) {
        if (i < 0 || i >= playlist.count()) return
        const it = playlist.get(i)
        if (!it) return
        const u = it.url
        player.openFile(u)
        currentIdx = i
        if (i + 1 < playlist.count()) {
            const nx = playlist.get(i + 1)
            if (nx) player.setNextFile(nx.url)
        } else {
            player.setNextFile("")
        }
    }

    function nextFromQueue() {
        const idx = currentIdx
        if (idx === -1) {
            if (playlist.count() > 0) playIndex(0)
            return
        }
        if (idx + 1 < playlist.count()) {
            playIndex(idx + 1)
        } else {
            player.setNextFile("")
            const endPos = Math.max(0, player.duration - 1)
            player.seek(endPos)
            if (!player.playing) {
                player.play()
            }
        }
    }

    function prevFromQueue() {
        const idx = currentIdx
        if (idx > 0) playIndex(idx - 1)
    }

    function handlePlayToggle() {
        if (player.playing) {
            player.pause()
            return
        }
        const idx = currentIdx
        const finished = player.duration > 0 && player.position >= (player.duration - 5)
        if (idx === -1) {
            if (playlist.count() > 0) {
                playIndex(0)
            } else {
                player.play()
            }
        } else if (idx === playlist.count() - 1 && finished) {
            playIndex(0)
        } else {
            player.play()
        }
    }

    // Main layout structure
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top section with resizable left/right panes
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Section: Playlist/Queue
            Rectangle {
                id: leftSection
                Layout.preferredWidth: parent.width * 0.4
                Layout.minimumWidth: 200
                Layout.maximumWidth: parent.width * 0.7
                Layout.fillHeight: true
                color: "#1a1a1a"
                border.color: "#333"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    // Playlist tabs area
                    TabBar {
                        id: playlistTabBar
                        Layout.fillWidth: true
                        height: 40
                        
                        TabButton {
                            text: "Queue"
                            width: implicitWidth
                        }
                        
                        TabButton {
                            text: "Playlist 1"
                            width: implicitWidth
                        }
                        
                        TabButton {
                            text: "+"
                            width: 30
                            font.pixelSize: 16
                        }
                    }

                    // Playlist content area
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: playlistTabBar.currentIndex

                        // Queue view
                        ListView {
                            id: queueView
                            clip: true
                            model: playlist
                            delegate: Rectangle {
                                width: queueView.width
                                height: 48
                                color: ListView.isCurrentItem ? "#334155" : (index % 2 === 0 ? "#1f2937" : "#111827")
                                
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 2
                                    
                                    Text { 
                                        Layout.fillWidth: true
                                        color: "#e5e7eb"
                                        text: title ? `${title} - ${artist || "Unknown Artist"}` : display
                                        font.pixelSize: 13
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    
                                    Text {
                                        Layout.fillWidth: true
                                        color: "#9ca3af"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        text: {
                                            const parts = []
                                            if (album) parts.push(album)
                                            if (year) parts.push(year)
                                            if (genre) parts.push(genre)
                                            return parts.join(" • ")
                                        }
                                        visible: album || year || genre
                                    }
                                    
                                    Text {
                                        color: "#6b7280"
                                        font.pixelSize: 10
                                        text: duration > 0 ? 
                                              `${Math.floor(duration / 60000)}:${String(Math.floor((duration % 60000) / 1000)).padStart(2, '0')}` : 
                                              ""
                                        visible: duration > 0
                                    }
                                }
                                
                                MouseArea { 
                                    anchors.fill: parent
                                    onClicked: queueView.currentIndex = index 
                                }
                            }
                            ScrollBar.vertical: ScrollBar {}
                        }

                        // Other playlist tabs (placeholder)
                        Rectangle {
                            color: "#1a1a1a"
                            Text {
                                anchors.centerIn: parent
                                text: "Playlist content"
                                color: "#666"
                            }
                        }

                        // New playlist button content
                        Rectangle {
                            color: "#1a1a1a"
                            Text {
                                anchors.centerIn: parent
                                text: "Create new playlist"
                                color: "#666"
                            }
                        }
                    }

                    // Playlist control buttons
                    RowLayout {
                        Layout.fillWidth: true
                        height: 40
                        spacing: 8
                        
                        Button {
                            text: "Add"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                            onClicked: addDialog.open()
                        }
                        
                        Button {
                            text: "Remove"
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 30
                            enabled: queueView.currentIndex >= 0
                            onClicked: playlist.removeAt(queueView.currentIndex)
                        }
                        
                        Button {
                            text: "Save"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                        }
                        
                        Button {
                            text: "Export"
                            Layout.preferredWidth: 70
                            Layout.preferredHeight: 30
                            onClicked: exportDialog.open()
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: "Clear"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 30
                        }
                    }
                }
            }

            // Resize handle
            Rectangle {
                width: 4
                Layout.fillHeight: true
                color: resizeHandleArea.pressed ? "#555" : "#333"
                MouseArea {
                    id: resizeHandleArea
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    drag.target: parent
                    drag.axis: Drag.XAxis
                    drag.minimumX: 200
                    drag.maximumX: window.width - 300
                    onPositionChanged: {
                        if (drag.active) {
                            leftSection.Layout.preferredWidth = parent.x
                        }
                    }
                }
            }

            // Right Section: Collection/Files Browser
            Rectangle {
                id: rightSection
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1a1a1a"
                border.color: "#333"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    // View mode toggle
                    RowLayout {
                        Layout.fillWidth: true
                        height: 40
                        spacing: 8
                        
                        Button {
                            text: "Collection"
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 30
                            checkable: true
                            checked: browserMode === 0
                            onClicked: browserMode = 0
                        }
                        
                        Button {
                            text: "Files"
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 30
                            checkable: true
                            checked: browserMode === 1
                            onClicked: browserMode = 1
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        ComboBox {
                            id: groupingCombo
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 30
                            model: ["Artist", "Album", "Year", "Genre"]
                        }
                        
                        Button {
                            text: "Settings"
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 30
                        }
                    }

                    // Navigation path bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 30
                        color: "#252525"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            text: browserMode === 1 ? filesModel.folder : "Album Artist > The Beatles > Abbey Road"
                            color: "#ccc"
                            font.pixelSize: 12
                        }
                    }

                    // Search bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 30
                        color: "#252525"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        
                        TextField {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            color: "#ccc"
                            font.pixelSize: 12
                            placeholderText: "Search by artist, album, or song..."
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }

                    FolderListModel {
                        id: filesModel
                        folder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
                        showDirs: true
                        showDotAndDotDot: false
                        showFiles: true
                        nameFilters: [
                            "*.wav", "*.flac", "*.mp3", "*.ogg", "*.opus", "*.aac", "*.m4a", "*.mp4", "*.mkv"
                        ]
                        sortField: FolderListModel.Name
                        sortReversed: false
                        caseSensitive: false
                    }

                    // Collection grid area
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1f1f1f"
                        border.color: "#333"
                        border.width: 1
                        radius: 4
                        StackLayout {
                            anchors.fill: parent
                            currentIndex: browserMode
                            
                            GridView {
                                id: collectionGrid
                                anchors.fill: parent
                                anchors.margins: 4
                                cellWidth: 120
                                cellHeight: 150
                                model: 24
                                delegate: Rectangle {
                                    width: collectionGrid.cellWidth - 8
                                    height: collectionGrid.cellHeight - 8
                                    color: "#2a2a2a"
                                    border.color: "#444"
                                    border.width: 1
                                    radius: 4
                                    anchors.margins: 4
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4
                                        
                                        Rectangle {
                                            width: 60
                                            height: 60
                                            color: "#333"
                                            border.color: "#555"
                                            border.width: 1
                                            radius: 4
                                            Layout.alignment: Qt.AlignHCenter
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: "♪"
                                                color: "#666"
                                                font.pixelSize: 24
                                            }
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "Album " + (index + 1)
                                            color: "#ccc"
                                            font.pixelSize: 11
                                            font.bold: true
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "Artist Name"
                                            color: "#999"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: "2000"
                                            color: "#666"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: console.log("Selected album", index)
                                    }
                                }
                                ScrollBar.vertical: ScrollBar {}
                            }

                            GridView {
                                id: filesGrid
                                anchors.fill: parent
                                anchors.margins: 4
                                cellWidth: 140
                                cellHeight: 140
                                model: filesModel
                                delegate: Rectangle {
                                    width: filesGrid.cellWidth - 12
                                    height: filesGrid.cellHeight - 12
                                    color: "#2a2a2a"
                                    border.color: "#444"
                                    border.width: 1
                                    radius: 4
                                    anchors.margins: 6
                                    
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 6
                                        
                                        Rectangle {
                                            width: 64
                                            height: 64
                                            color: "#333"
                                            border.color: "#555"
                                            border.width: 1
                                            radius: 4
                                            Layout.alignment: Qt.AlignHCenter
                                            
                                            Text {
                                                anchors.centerIn: parent
                                                text: fileIsDir ? "📁" : "♪"
                                                color: "#bbb"
                                                font.pixelSize: 28
                                            }
                                        }
                                        
                                        Text {
                                            Layout.fillWidth: true
                                            text: fileName
                                            color: "#ccc"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton
                                        onClicked: {
                                            console.log("filesGrid clicked", fileName)
                                            const urlRole = (typeof fileURL !== 'undefined') ? fileURL : (typeof fileUrl !== 'undefined' ? fileUrl : null)
                                            if (urlRole === null) { console.warn('No fileURL role in delegate'); return }
                                            const asString = (typeof urlRole === 'string') ? urlRole : urlRole.toString()
                                            if (fileIsDir) {
                                                filesModel.folder = asString
                                            } else {
                                                const beforeCount = playlist.count()
                                                playlist.add(urlRole)
                                                const idx = currentIdx
                                                if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                                                    player.setNextFile(urlRole)
                                                }
                                            }
                                        }
                                        onDoubleClicked: {
                                            console.log("filesGrid doubleClicked", fileName)
                                            const urlRole = (typeof fileURL !== 'undefined') ? fileURL : (typeof fileUrl !== 'undefined' ? fileUrl : null)
                                            if (urlRole === null) { console.warn('No fileURL role in delegate'); return }
                                            const asString = (typeof urlRole === 'string') ? urlRole : urlRole.toString()
                                            if (fileIsDir) {
                                                filesModel.folder = asString
                                            } else {
                                                const beforeCount = playlist.count()
                                                playlist.add(urlRole)
                                                const idx = currentIdx
                                                if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                                                    player.setNextFile(urlRole)
                                                }
                                            }
                                        }
                                    }
                                }
                                ScrollBar.vertical: ScrollBar {}
                            }
                        }
                    }
                }
            }
        }

        // Playing Section (above controls)
        Rectangle {
            id: playingSection
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            Layout.minimumHeight: 60
            Layout.maximumHeight: 120
            color: "#252525"
            border.color: "#333"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Cover art
                Rectangle {
                    width: 56
                    height: 56
                    color: "#333"
                    border.color: "#555"
                    border.width: 1
                    radius: 4
                    
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        fillMode: Image.PreserveAspectFit
                        source: player.currentMetadata ? player.currentMetadata.coverArtUrl : ""
                        visible: source !== ""
                        
                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            color: "#666"
                            font.pixelSize: 20
                            visible: !parent.visible
                        }
                    }
                }

                // Track info
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: player.currentMetadata ? player.currentMetadata.title : "No track playing"
                        color: "#fff"
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: player.currentMetadata ? `${player.currentMetadata.artist || "Unknown Artist"} • ${player.currentMetadata.album || "Unknown Album"}` : ""
                        color: "#aaa"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                // Quick actions
                RowLayout {
                    spacing: 8

                    Button {
                        text: "♥"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        font.pixelSize: 14
                    }

                    Button {
                        text: "≡"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        font.pixelSize: 14
                    }
                }
            }
        }

        // Controls Section
        Rectangle {
            id: controlsSection
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            Layout.minimumHeight: 80
            Layout.maximumHeight: 120
            color: "#1a1a1a"
            border.color: "#333"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Progress bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: formatTime(player.position)
                        color: "#ccc"
                        font.pixelSize: 11
                        Layout.preferredWidth: 50
                    }

                    Slider {
                        id: positionSlider
                        Layout.fillWidth: true
                        from: 0
                        to: player.duration
                        value: player.position
                        onMoved: player.seek(value)
                    }

                    Text {
                        text: formatTime(player.duration)
                        color: "#ccc"
                        font.pixelSize: 11
                        Layout.preferredWidth: 50
                    }
                }

                // Playback controls
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // Left side controls
                    RowLayout {
                        spacing: 8

                        Button {
                            text: "⏮"
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            font.pixelSize: 14
                            onClicked: prevFromQueue()
                        }

                        Button {
                            text: player.playing ? "⏸" : "▶"
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            font.pixelSize: 16
                            onClicked: handlePlayToggle()
                        }

                        Button {
                            text: "⏭"
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            font.pixelSize: 14
                            onClicked: nextFromQueue()
                        }

                        Button {
                            text: "🔀"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                        }

                        Button {
                            text: "🔁"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Right side controls
                    RowLayout {
                        spacing: 8

                        Text {
                            text: "🔊"
                            color: "#ccc"
                            font.pixelSize: 14
                        }

                        Slider {
                            id: volumeSlider
                            from: 0
                            to: 1
                            value: 0.8
                            stepSize: 0.01
                            onValueChanged: player.volume = value
                            Layout.preferredWidth: 100
                        }

                        Button {
                            text: "≡"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }
    }

    // Helper function for time formatting
    function formatTime(milliseconds) {
        if (!milliseconds || milliseconds < 0) return "0:00"
        const seconds = Math.floor(milliseconds / 1000)
        const minutes = Math.floor(seconds / 60)
        const secs = seconds % 60
        return `${minutes}:${secs.toString().padStart(2, '0')}`
    }

    // File dialogs (kept for functionality)
    FileDialog {
        id: fileDialog
        title: "Open Audio File"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.openFile(selectedFile)
    }

    FileDialog {
        id: nextDialog
        title: "Select Next Track (Gapless)"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: player.setNextFile(selectedFile)
    }

    FileDialog {
        id: addDialog
        title: "Add to Playlist"
        nameFilters: [
            "Audio files (*.wav *.flac *.mp3 *.ogg *.opus *.aac *.m4a *.mp4 *.mkv)",
            "All files (*)"
        ]
        onAccepted: {
            const beforeCount = playlist.count()
            playlist.add(selectedFile)
            const idx = currentIdx
            if (idx >= 0 && (idx === beforeCount - 1 || beforeCount === 0)) {
                player.setNextFile(selectedFile)
            }
        }
    }

    FileDialog {
        id: importDialog
        title: "Import Playlist (M3U/M3U8)"
        nameFilters: [
            "Playlists (*.m3u *.m3u8)",
            "All files (*)"
        ]
        onAccepted: playlist.importM3U8(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Export Playlist (M3U8)"
        nameFilters: [
            "Playlists (*.m3u8)",
            "All files (*)"
        ]
        onAccepted: playlist.exportM3U8(selectedFile)
    }

    // Output device selector (kept for functionality)
    ComboBox {
        id: outputBox
        visible: false
        model: player.audioOutputs
        onActivated: player.selectOutputByIndex(index)
        Component.onCompleted: currentIndex = Math.max(0, player.audioOutputs.indexOf(player.currentOutput))
    }

    Connections {
        target: player
        function onCurrentSourceChanged() {
            // Resync currentIdx on source changes, prefer next index if duplicates
            if (currentIdx >= 0)
                currentIdx = indexOfUrlFrom(player.currentSource, currentIdx)
            else
                currentIdx = indexOfUrl(player.currentSource)
            armNextFromQueue()
        }
        function onCurrentMetadataChanged() {
            // Update playlist metadata when metadata becomes available
            if (currentIdx >= 0 && player.currentMetadata) {
                playlist.updateMetadata(currentIdx, player.currentMetadata)
            }
        }
    }
}
