#pragma once

#include <QObject>
#include <QUrl>
#include <QString>
#include <taglib/tag.h>
#include <taglib/fileref.h>

class TrackMetadata;

class MetadataReader : public QObject {
    Q_OBJECT

public:
    explicit MetadataReader(QObject* parent = nullptr);
    
    // Static method for standalone metadata reading (perfect for collection browser)
    static TrackMetadata* readMetadataStandalone(const QUrl& url, QObject* parent = nullptr);
    
    // Instance methods
    Q_INVOKABLE TrackMetadata* readMetadata(const QUrl& url);
    Q_INVOKABLE bool hasMetadata(const QUrl& url);

private:
    static TrackMetadata* readFromTagLib(const QString& filePath, QObject* parent);
    static QString tagLibStringToQString(const TagLib::String& str);
    static void extractFromProperties(const TagLib::PropertyMap& properties, TrackMetadata* metadata);
    static void extractCoverArt(TagLib::File* file, const QString& filePath, TrackMetadata* metadata);
    static TrackMetadata* extractFromGeneric(TagLib::FileRef& fileRef, TrackMetadata* metadata, const QString& filePath);
};
