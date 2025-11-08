import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    visible: true
    width: 900
    height: 600
    title: "Music Player (MVP)"

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

    // Tracks the index of the currently playing item to handle duplicates
    property int currentIdx: -1

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            spacing: 8
            Button { text: "Open"; onClicked: fileDialog.open() }
            Button { text: "Set Next"; onClicked: nextDialog.open() }
            Button { text: player.playing ? "Pause" : "Play"; onClicked: handlePlayToggle() }
            Button { text: "Prev"; onClicked: prevFromQueue() }
            Button { text: "Next"; onClicked: nextFromQueue() }
            Button { text: "Add"; onClicked: addDialog.open() }
            Button { text: "Import"; onClicked: importDialog.open() }
            Button { text: "Export"; onClicked: exportDialog.open() }
            Button {
                text: "Remove"
                enabled: queueView.currentIndex >= 0
                onClicked: playlist.removeAt(queueView.currentIndex)
            }
            Button {
                text: "Play Selected"
                enabled: queueView.currentIndex >= 0
                onClicked: playIndex(queueView.currentIndex)
            }
            ComboBox {
                id: outputBox
                Layout.preferredWidth: 240
                model: player.audioOutputs
                onActivated: player.selectOutputByIndex(index)
                Component.onCompleted: currentIndex = Math.max(0, player.audioOutputs.indexOf(player.currentOutput))
                ToolTip.visible: hovered
                ToolTip.text: `Output: ${player.currentOutput}`
            }
            Slider {
                id: pos
                Layout.fillWidth: true
                from: 0
                to: player.duration
                value: player.position
                onMoved: player.seek(value)
            }
            Slider {
                id: vol
                from: 0
                to: 1
                value: 0.8
                stepSize: 0.01
                onValueChanged: player.volume = value
                implicitWidth: 150
            }
        }

        Label {
            text: `Position: ${Math.round(player.position/1000)}s / ${Math.round(player.duration/1000)}s`
        }

        // Metadata testing display
        Rectangle {
            Layout.fillWidth: true
            height: 200
            color: "#1f2937"
            border.color: "#374151"
            border.width: 1
            radius: 8
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                // Cover art display (keep for testing)
                Rectangle {
                    width: 80
                    height: 80
                    color: "#374151"
                    border.color: "#4b5563"
                    border.width: 1
                    radius: 4
                    
                    Image {
                        id: coverArtImage
                        anchors.fill: parent
                        anchors.margins: 2
                        fillMode: Image.PreserveAspectFit
                        source: player.currentMetadata ? player.currentMetadata.coverArtUrl : ""
                        visible: source !== ""
                        
                        // Fallback music icon
                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            color: "#6b7280"
                            font.pixelSize: 24
                            visible: !parent.visible
                        }
                    }
                }
                
                // Metadata labels
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    
                    Label {
                        text: "Title: " + (player.currentMetadata ? player.currentMetadata.title : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Artist: " + (player.currentMetadata ? player.currentMetadata.artist : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Album: " + (player.currentMetadata ? (player.currentMetadata.album || "[EMPTY]") : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Genre: " + (player.currentMetadata ? (player.currentMetadata.genre || "[EMPTY]") : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Year: " + (player.currentMetadata ? (player.currentMetadata.year || "[EMPTY]") : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Track: " + (player.currentMetadata ? player.currentMetadata.trackNumber.toString() : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Duration: " + (player.currentMetadata ? Math.round(player.currentMetadata.duration/1000).toString() + "s" : "LOAD_ERROR")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Cover: " + (player.currentMetadata && player.currentMetadata.coverArtUrl !== "" ? "LOADED" : "NONE")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Has Metadata: " + (player.currentMetadata ? "YES" : "NO")
                        color: "#e5e7eb"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Diagnostics row removed after fix; ComboBox remains for device selection.

        ListView {
            id: queueView
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                    
                    // Main track info (title - artist)
                    Text { 
                        Layout.fillWidth: true
                        color: "#e5e7eb"
                        text: title ? `${title} - ${artist || "Unknown Artist"}` : display
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    
                    // Secondary info (album, year, genre)
                    Text {
                        Layout.fillWidth: true
                        color: "#9ca3af"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        
                        // Build secondary text from available metadata
                        text: {
                            const parts = []
                            if (album) parts.push(album)
                            if (year) parts.push(year)
                            if (genre) parts.push(genre)
                            return parts.join(" • ")
                        }
                        
                        visible: album || year || genre
                    }
                    
                    // Track duration
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
