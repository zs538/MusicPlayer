pragma Singleton
import QtQuick

QtObject {
    readonly property var mainBuiltinKeys: [
        "trackNumber",
        "title",
        "artist",
        "album",
        "durationMs"
    ]

    readonly property var otherBuiltinKeys: [
        "albumArtist",
        "performer",
        "composer",
        "year",
        "originalYear",
        "discNumber",
        "genre",
        "sampleRate",
        "bitDepth",
        "bitrate",
        "fileType",
        "fileName",
        "fileSize",
        "dateCreated",
        "dateModified",
        "comment",
        "bpm",
        "initialKey",
        "filePath"
    ]

    readonly property var builtinKeys: mainBuiltinKeys.concat(otherBuiltinKeys)

    function normalizeKey(key) {
        let text = String(key === undefined || key === null ? "" : key).trim()
        if (!text.length)
            return ""
        if (text.indexOf("custom:") === 0)
            return "custom:" + text.slice(7).trim().toUpperCase()
        return text
    }

    function cloneValue(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function columnMeta(key) {
        key = normalizeKey(key)
        if (!key.length)
            return { title: "None", weight: 0.18, minWidth: 28, alignment: "left", tone: "secondary" }
        if (key.indexOf("custom:") === 0) {
            let tagKey = key.slice(7)
            return { title: tagKey, weight: 0.22, minWidth: 48, alignment: "left", tone: "primary" }
        }

        let meta = {
            trackNumber: { title: "#", weight: 42 / 354, minWidth: 36, alignment: "right", tone: "secondary" },
            title: { title: "Title", weight: 240 / 354, minWidth: 72, alignment: "left", tone: "primary" },
            durationMs: { title: "Duration", weight: 72 / 354, minWidth: 48, alignment: "right", tone: "secondary" },
            artist: { title: "Artist", weight: 160 / 354, minWidth: 64, alignment: "left", tone: "primary" },
            album: { title: "Album", weight: 180 / 354, minWidth: 64, alignment: "left", tone: "primary" },
            albumArtist: { title: "Album Artist", weight: 160 / 354, minWidth: 64, alignment: "left", tone: "primary" },
            performer: { title: "Performer", weight: 150 / 354, minWidth: 64, alignment: "left", tone: "primary" },
            composer: { title: "Composer", weight: 150 / 354, minWidth: 64, alignment: "left", tone: "primary" },
            year: { title: "Year", weight: 64 / 354, minWidth: 42, alignment: "right", tone: "secondary" },
            originalYear: { title: "Original Year", weight: 96 / 354, minWidth: 56, alignment: "right", tone: "secondary" },
            discNumber: { title: "Disc", weight: 54 / 354, minWidth: 40, alignment: "right", tone: "secondary" },
            genre: { title: "Genre", weight: 120 / 354, minWidth: 56, alignment: "left", tone: "secondary" },
            sampleRate: { title: "Sample Rate", weight: 96 / 354, minWidth: 56, alignment: "right", tone: "secondary" },
            bitDepth: { title: "Bit Depth", weight: 88 / 354, minWidth: 52, alignment: "right", tone: "secondary" },
            bitrate: { title: "Bitrate", weight: 86 / 354, minWidth: 52, alignment: "right", tone: "secondary" },
            fileType: { title: "Type", weight: 68 / 354, minWidth: 48, alignment: "left", tone: "secondary" },
            fileName: { title: "File Name", weight: 220 / 354, minWidth: 72, alignment: "left", tone: "secondary" },
            fileSize: { title: "Size", weight: 92 / 354, minWidth: 52, alignment: "right", tone: "secondary" },
            dateCreated: { title: "Created", weight: 110 / 354, minWidth: 64, alignment: "left", tone: "secondary" },
            dateModified: { title: "Modified", weight: 110 / 354, minWidth: 64, alignment: "left", tone: "secondary" },
            comment: { title: "Comment", weight: 200 / 354, minWidth: 72, alignment: "left", tone: "secondary" },
            bpm: { title: "BPM", weight: 64 / 354, minWidth: 42, alignment: "right", tone: "secondary" },
            initialKey: { title: "Key", weight: 70 / 354, minWidth: 44, alignment: "left", tone: "secondary" },
            filePath: { title: "Path", weight: 280 / 354, minWidth: 80, alignment: "left", tone: "secondary" }
        }
        return meta[key] || { title: String(key), weight: 0.2, minWidth: 48, alignment: "left", tone: "secondary" }
    }

    function customColumnKeys(customTagKeys) {
        let custom = []
        if (customTagKeys && customTagKeys.length !== undefined) {
            for (let i = 0; i < customTagKeys.length; ++i) {
                let key = String(customTagKeys[i] || "").trim()
                if (key.length)
                    custom.push("custom:" + key.toUpperCase())
            }
        }
        custom.sort(function(a, b) { return a.localeCompare(b) })
        return custom
    }

    function availableColumnKeys(customTagKeys) {
        return builtinKeys.concat(customColumnKeys(customTagKeys))
    }

    function titleForKey(key) {
        return columnMeta(key).title
    }

    function createColumn(key) {
        key = normalizeKey(key)
        let meta = columnMeta(key)
        return {
            key: key,
            title: key.length ? meta.title : "",
            weight: meta.weight,
            alignment: meta.alignment,
            tone: meta.tone
        }
    }

    function normalizedColumnWeight(source, legacyTotal) {
        let directWeight = Number(source && source.weight)
        if (isFinite(directWeight) && directWeight > 0)
            return directWeight
        let legacyWidth = Number(source && source.width)
        if (isFinite(legacyWidth) && legacyWidth > 0 && legacyTotal > 0)
            return legacyWidth / legacyTotal
        return 0
    }

    function ensureColumn(column, legacyTotal) {
        let source = column || {}
        let key = normalizeKey(source.key)
        let meta = columnMeta(key)
        let weight = normalizedColumnWeight(source, legacyTotal)
        if (!isFinite(weight) || weight <= 0)
            weight = meta.weight
        return {
            key: key,
            title: key.length ? String(source.title || meta.title) : "",
            weight: Math.max(0.0001, weight),
            alignment: source.alignment || meta.alignment,
            tone: source.tone || meta.tone
        }
    }

    function normalizeColumns(columns) {
        let normalized = []
        let total = 0
        for (let i = 0; i < columns.length; ++i) {
            let column = cloneValue(columns[i])
            let weight = Number(column.weight)
            if (!isFinite(weight) || weight <= 0)
                weight = columnMeta(column.key).weight
            column.weight = Math.max(0.0001, weight)
            total += column.weight
            normalized.push(column)
        }
        if (!normalized.length)
            return normalized
        if (!isFinite(total) || total <= 0) {
            let equalWeight = 1 / normalized.length
            for (let i = 0; i < normalized.length; ++i)
                normalized[i].weight = equalWeight
            return normalized
        }
        for (let i = 0; i < normalized.length; ++i)
            normalized[i].weight = normalized[i].weight / total
        return normalized
    }

    function defaultLayout() {
        return {
            headerVisible: true,
            headerLocked: false,
            columns: normalizeColumns([
                createColumn("trackNumber"),
                createColumn("title"),
                createColumn("durationMs")
            ])
        }
    }

    function ensureLayout(layout) {
        let normalized = {
            headerVisible: true,
            headerLocked: false,
            columns: []
        }

        if (layout && typeof layout.headerVisible === "boolean")
            normalized.headerVisible = layout.headerVisible
        if (layout && typeof layout.headerLocked === "boolean")
            normalized.headerLocked = layout.headerLocked

        let legacyTotal = 0
        if (layout && layout.columns && layout.columns.length !== undefined) {
            for (let i = 0; i < layout.columns.length; ++i) {
                let source = layout.columns[i] || {}
                let directWeight = Number(source.weight)
                if (isFinite(directWeight) && directWeight > 0)
                    legacyTotal += directWeight
                else {
                    let legacyWidth = Number(source.width)
                    if (isFinite(legacyWidth) && legacyWidth > 0)
                        legacyTotal += legacyWidth
                    else
                        legacyTotal += columnMeta(source.key).weight
                }
            }
            for (let i = 0; i < layout.columns.length; ++i)
                normalized.columns.push(ensureColumn(layout.columns[i], legacyTotal))
        }

        if (!normalized.columns.length)
            normalized.columns = defaultLayout().columns

        normalized.columns = normalizeColumns(normalized.columns)
        return cloneValue(normalized)
    }

    function setHeaderVisible(layout, visible) {
        let next = ensureLayout(layout)
        next.headerVisible = visible
        return next
    }

    function setHeaderLocked(layout, locked) {
        let next = ensureLayout(layout)
        next.headerLocked = locked
        return next
    }

    function appendEmptyColumn(layout) {
        let next = ensureLayout(layout)
        next.columns.push(createColumn(""))
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function splitColumn(layout, index) {
        let next = ensureLayout(layout)
        if (index < 0 || index >= next.columns.length)
            return next
        let existing = next.columns[index]
        let inserted = createColumn("")
        let splitWeight = Math.max(existing.weight || 0, 0.02)
        existing.weight = splitWeight / 2
        inserted.weight = splitWeight - existing.weight
        next.columns.splice(index + 1, 0, inserted)
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function setColumn(layout, index, key) {
        let next = ensureLayout(layout)
        if (index < 0 || index >= next.columns.length)
            return next
        let existing = next.columns[index]
        let replacement = createColumn(key)
        replacement.weight = existing.weight
        next.columns[index] = replacement
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function setColumnAlignment(layout, index, alignment) {
        let next = ensureLayout(layout)
        if (index < 0 || index >= next.columns.length)
            return next
        let normalizedAlignment = String(alignment || "left")
        if (normalizedAlignment !== "left" && normalizedAlignment !== "center" && normalizedAlignment !== "right")
            normalizedAlignment = "left"
        next.columns[index].alignment = normalizedAlignment
        return next
    }

    function removeColumn(layout, index) {
        let next = ensureLayout(layout)
        if (next.columns.length <= 1 || index < 0 || index >= next.columns.length)
            return next
        next.columns.splice(index, 1)
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function moveColumn(layout, fromIndex, toIndex) {
        let next = ensureLayout(layout)
        if (fromIndex < 0 || fromIndex >= next.columns.length)
            return next
        let targetIndex = Math.max(0, Math.min(toIndex, next.columns.length - 1))
        if (targetIndex === fromIndex)
            return next
        let moved = next.columns.splice(fromIndex, 1)[0]
        next.columns.splice(targetIndex, 0, moved)
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function minColumnWidthPixels(column) {
        let metaMinWidth = Number(columnMeta(column && column.key).minWidth || 24)
        if (!isFinite(metaMinWidth) || metaMinWidth <= 0)
            metaMinWidth = 24
        return Math.max(10, Math.round(metaMinWidth * 0.35))
    }

    function resolveColumnWidths(columns, totalWidth) {
        let result = []
        let width = Math.max(0, Math.round(Number(totalWidth) || 0))
        if (!columns || columns.length === undefined)
            return result
        if (!columns.length) {
            return result
        }
        if (width <= 0) {
            for (let i = 0; i < columns.length; ++i)
                result.push(0)
            return result
        }

        let normalized = normalizeColumns(columns)
        let fractions = []
        let used = 0
        for (let i = 0; i < normalized.length; ++i) {
            let exact = normalized[i].weight * width
            let floored = Math.floor(exact)
            result.push(floored)
            fractions.push(exact - floored)
            used += floored
        }

        let remaining = width - used
        while (remaining > 0) {
            let bestIndex = 0
            let bestFraction = -1
            for (let i = 0; i < fractions.length; ++i) {
                if (fractions[i] > bestFraction) {
                    bestFraction = fractions[i]
                    bestIndex = i
                }
            }
            result[bestIndex] += 1
            fractions[bestIndex] = -1
            remaining -= 1
        }
        return result
    }

    function separatorPositions(widths) {
        let positions = [0]
        let x = 0
        for (let i = 0; i < widths.length; ++i) {
            x += widths[i]
            positions.push(x)
        }
        return positions
    }

    function columnStartPositions(widths) {
        let positions = []
        let x = 0
        for (let i = 0; i < widths.length; ++i) {
            positions.push(x)
            x += widths[i]
        }
        return positions
    }

    function resizeBetween(layout, index, deltaPixels, totalWidth) {
        let next = ensureLayout(layout)
        if (index < 0 || index >= next.columns.length - 1)
            return next
        let widths = resolveColumnWidths(next.columns, totalWidth)
        let leftWidth = widths[index]
        let rightWidth = widths[index + 1]
        let pairWidth = leftWidth + rightWidth
        if (pairWidth <= 0)
            return next

        let minLeft = Math.min(minColumnWidthPixels(next.columns[index]), pairWidth)
        let minRight = Math.min(minColumnWidthPixels(next.columns[index + 1]), pairWidth)
        let maxLeft = Math.max(minLeft, pairWidth - minRight)
        let requestedLeft = Math.round(leftWidth + deltaPixels)
        let clampedLeft = Math.max(minLeft, Math.min(maxLeft, requestedLeft))
        let clampedRight = pairWidth - clampedLeft
        let pairWeight = next.columns[index].weight + next.columns[index + 1].weight
        if (pairWeight <= 0)
            pairWeight = 1
        next.columns[index].weight = pairWeight * clampedLeft / pairWidth
        next.columns[index + 1].weight = pairWeight - next.columns[index].weight
        next.columns = normalizeColumns(next.columns)
        return next
    }

    function baseName(path) {
        let text = String(path || "")
        if (!text.length)
            return ""
        let slash = text.lastIndexOf("/")
        return slash >= 0 ? text.slice(slash + 1) : text
    }

    function formatDuration(ms) {
        let value = Number(ms || 0)
        if (!isFinite(value) || value <= 0)
            return ""
        let totalSeconds = Math.floor(value / 1000)
        let seconds = totalSeconds % 60
        let minutes = Math.floor(totalSeconds / 60)
        let hours = Math.floor(minutes / 60)
        if (hours > 0)
            return hours + ":" + String(minutes % 60).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")
        return minutes + ":" + String(seconds).padStart(2, "0")
    }

    function formatBitrate(value) {
        let number = Number(value || 0)
        if (!isFinite(number) || number <= 0)
            return ""
        return Math.round(number / 1000) + " kbps"
    }

    function formatSampleRate(value) {
        let number = Number(value || 0)
        if (!isFinite(number) || number <= 0)
            return ""
        return (number / 1000).toFixed(number % 1000 === 0 ? 0 : 1) + " kHz"
    }

    function formatFileSize(bytes) {
        let value = Number(bytes || 0)
        if (!isFinite(value) || value <= 0)
            return ""
        let units = ["B", "KB", "MB", "GB", "TB"]
        let unitIndex = 0
        while (value >= 1024 && unitIndex < units.length - 1) {
            value /= 1024
            unitIndex += 1
        }
        return value.toFixed(value >= 100 || unitIndex === 0 ? 0 : 1) + " " + units[unitIndex]
    }

    function formatDate(value) {
        if (!value)
            return ""
        try {
            return Qt.formatDateTime(value, "yyyy-MM-dd")
        } catch (error) {
            return String(value)
        }
    }

    function customTagValue(trackData, key) {
        if (!trackData || !trackData.customTags)
            return ""
        let tags = trackData.customTags
        let tagKey = normalizeKey(key).slice(7)
        let value = tags[tagKey]
        if (value === undefined || value === null)
            return ""
        if (value.join)
            return value.join("; ")
        return String(value)
    }

    function valueForTrack(trackData, key) {
        key = normalizeKey(key)
        if (!key.length || !trackData)
            return ""
        if (key.indexOf("custom:") === 0)
            return customTagValue(trackData, key)
        if (key === "title")
            return trackData.title || trackData.displayText || trackData.fileName || baseName(trackData.filePath)
        return trackData[key]
    }

    function textForColumn(trackData, column) {
        let key = normalizeKey(column && column.key !== undefined ? column.key : column)
        if (!key.length)
            return ""
        let value = valueForTrack(trackData, key)
        if (value === undefined || value === null)
            return ""
        if (key === "durationMs")
            return formatDuration(value)
        if (key === "bitrate")
            return formatBitrate(value)
        if (key === "sampleRate")
            return formatSampleRate(value)
        if (key === "fileSize")
            return formatFileSize(value)
        if (key === "dateCreated" || key === "dateModified")
            return formatDate(value)
        if (Array.isArray(value))
            return value.join("; ")
        if (typeof value === "number")
            return value > 0 ? String(value) : ""
        return String(value)
    }
}
