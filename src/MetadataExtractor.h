#ifndef METADATAEXTRACTOR_H
#define METADATAEXTRACTOR_H

#include "TrackListModel.h"
#include "library/LibraryDatabase.h"

namespace MetadataExtractor {

/**
 * @brief Extract metadata from an audio file using TagLib.
 * 
 * This is the single source of truth for metadata extraction.
 * Used by both PlaylistManager (for drag-drop) and LibraryScanner.
 * 
 * @param filePath Path to the audio file
 * @return TrackInfo with extracted metadata, or invalid TrackInfo if extraction fails
 */
TrackInfo extractTrackInfo(const QString &filePath);

/**
 * @brief Extract metadata and return as LibraryTrack.
 * 
 * Convenience wrapper that extracts metadata and converts to LibraryTrack format.
 * 
 * @param filePath Path to the audio file
 * @return LibraryTrack with extracted metadata, or invalid LibraryTrack if extraction fails
 */
LibraryTrack extractLibraryTrack(const QString &filePath);

/**
 * @brief Convert TrackInfo to LibraryTrack.
 * 
 * @param track Source TrackInfo
 * @param id Database ID (default -1 for new tracks)
 * @return LibraryTrack with copied fields
 */
LibraryTrack toLibraryTrack(const TrackInfo &track, qint64 id = -1);

/**
 * @brief Convert LibraryTrack to TrackInfo.
 * 
 * @param track Source LibraryTrack
 * @return TrackInfo with copied fields
 */
TrackInfo toTrackInfo(const LibraryTrack &track);

} // namespace MetadataExtractor

#endif // METADATAEXTRACTOR_H
