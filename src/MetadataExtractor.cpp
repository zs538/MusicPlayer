#include "MetadataExtractor.h"
#include <QFileInfo>
#include <QDir>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/flacfile.h>
#include <taglib/wavfile.h>

namespace MetadataExtractor {

TrackInfo extractTrackInfo(const QString &filePath)
{
    TrackInfo track;
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        return track;
    }
    
    track.filePath = QDir::cleanPath(fileInfo.absoluteFilePath());
    track.fileName = fileInfo.fileName();
    track.fileSize = fileInfo.size();
    track.fileType = fileInfo.suffix().toUpper();
    track.dateCreated = fileInfo.birthTime();
    track.dateModified = fileInfo.lastModified();

    TagLib::FileRef file(track.filePath.toUtf8().constData());
    if (file.isNull() || !file.tag()) {
        return track;
    }

    TagLib::Tag *tag = file.tag();
    track.title = QString::fromStdString(tag->title().to8Bit(true));
    track.artist = QString::fromStdString(tag->artist().to8Bit(true));
    track.album = QString::fromStdString(tag->album().to8Bit(true));
    track.trackNumber = tag->track();
    track.year = tag->year();
    track.genre = QString::fromStdString(tag->genre().to8Bit(true));
    track.comment = QString::fromStdString(tag->comment().to8Bit(true));

    if (file.file()) {
        TagLib::PropertyMap props = file.file()->properties();

        auto propString = [&props](const char *key) -> QString {
            if (!props.contains(key) || props[key].isEmpty())
                return {};
            return QString::fromStdString(props[key].front().to8Bit(true));
        };

        auto propInt = [&propString](const char *key) -> int {
            QString s = propString(key);
            if (s.isEmpty())
                return 0;
            bool ok = false;
            int v = s.section('/', 0, 0).toInt(&ok);
            if (ok)
                return v;
            v = s.toInt(&ok);
            return ok ? v : 0;
        };

        QString albumArtist = propString("ALBUMARTIST");
        if (albumArtist.isEmpty())
            albumArtist = propString("ALBUM ARTIST");
        track.albumArtist = albumArtist;

        track.performer = propString("PERFORMER");
        track.composer = propString("COMPOSER");
        track.discNumber = propInt("DISCNUMBER");

        track.originalYear = propInt("ORIGINALYEAR");
        if (track.originalYear == 0) {
            QString odate = propString("ORIGINALDATE");
            if (odate.isEmpty())
                odate = propString("ORIGINAL DATE");
            bool ok = false;
            int v = odate.left(4).toInt(&ok);
            track.originalYear = ok ? v : 0;
        }

        track.bpm = propInt("BPM");

        QString initialKey = propString("INITIALKEY");
        if (initialKey.isEmpty())
            initialKey = propString("INITIAL KEY");
        track.initialKey = initialKey;

        track.url = propString("URL");
    }

    if (file.audioProperties()) {
        track.durationMs = file.audioProperties()->lengthInMilliseconds();
        track.sampleRate = file.audioProperties()->sampleRate();
        track.bitrate = file.audioProperties()->bitrate() * 1000;

        if (auto flacFile = dynamic_cast<TagLib::FLAC::File*>(file.file())) {
            if (flacFile->audioProperties()) {
                track.bitDepth = flacFile->audioProperties()->bitsPerSample();
            }
        } else if (auto wavFile = dynamic_cast<TagLib::RIFF::WAV::File*>(file.file())) {
            if (wavFile->audioProperties()) {
                track.bitDepth = wavFile->audioProperties()->bitsPerSample();
            }
        }
    }

    // Fallback to filename if no title
    if (track.title.isEmpty()) {
        track.title = fileInfo.fileName();
    }

    return track;
}

LibraryTrack extractLibraryTrack(const QString &filePath)
{
    return toLibraryTrack(extractTrackInfo(filePath));
}

LibraryTrack toLibraryTrack(const TrackInfo &track, qint64 id)
{
    LibraryTrack lib;
    lib.id = id;
    lib.filePath = track.filePath;
    lib.title = track.title;
    lib.artist = track.artist;
    lib.album = track.album;
    lib.albumArtist = track.albumArtist;
    lib.performer = track.performer;
    lib.composer = track.composer;
    lib.trackNumber = track.trackNumber;
    lib.discNumber = track.discNumber;
    lib.year = track.year;
    lib.originalYear = track.originalYear;
    lib.durationMs = track.durationMs;
    lib.genre = track.genre;
    lib.sampleRate = track.sampleRate;
    lib.bitDepth = track.bitDepth;
    lib.bitrate = track.bitrate;
    lib.url = track.url;
    lib.fileName = track.fileName;
    lib.fileType = track.fileType;
    lib.fileSize = track.fileSize;
    lib.createdTime = track.dateCreated.toSecsSinceEpoch();
    lib.modifiedTime = track.dateModified.toSecsSinceEpoch();
    lib.comment = track.comment;
    lib.bpm = track.bpm;
    lib.initialKey = track.initialKey;
    // lib.channels not available in TrackInfo
    // lib.codec not available in TrackInfo
    return lib;
}

TrackInfo toTrackInfo(const LibraryTrack &lib)
{
    TrackInfo track;
    track.filePath = lib.filePath;
    track.title = lib.title;
    track.artist = lib.artist;
    track.album = lib.album;
    track.albumArtist = lib.albumArtist;
    track.performer = lib.performer;
    track.composer = lib.composer;
    track.trackNumber = lib.trackNumber;
    track.discNumber = lib.discNumber;
    track.year = lib.year;
    track.originalYear = lib.originalYear;
    track.durationMs = lib.durationMs;
    track.genre = lib.genre;
    track.sampleRate = lib.sampleRate;
    track.bitDepth = lib.bitDepth;
    track.bitrate = lib.bitrate;
    track.url = lib.url;
    track.fileName = lib.fileName;
    track.fileType = lib.fileType;
    track.fileSize = lib.fileSize;
    track.dateCreated = QDateTime::fromSecsSinceEpoch(lib.createdTime);
    track.dateModified = QDateTime::fromSecsSinceEpoch(lib.modifiedTime);
    track.comment = lib.comment;
    track.bpm = lib.bpm;
    track.initialKey = lib.initialKey;
    return track;
}

} // namespace MetadataExtractor
