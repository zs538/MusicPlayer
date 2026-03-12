#include "Settings.h"
#include "CoverImageProvider.h"

static Settings *s_instance = nullptr;

namespace {

QString collectionViewSettingKey(const QString &groupType, const QString &groupBy, const QString &settingName)
{
    return QStringLiteral("collection/%1/views/%2/%3").arg(groupType, groupBy, settingName);
}

QString legacyCollectionViewSettingKey(const QString &groupBy, const QString &settingName)
{
    return QStringLiteral("collection/%1/%2").arg(groupBy, settingName);
}

}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings("MusicPlayer", "MusicPlayer-")
{
    s_instance = this;
    load();
}

Settings *Settings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(jsEngine)
    
    if (!s_instance) {
        s_instance = new Settings(qmlEngine);
    }
    return s_instance;
}

Settings *Settings::instance()
{
    return s_instance;
}

double Settings::volume() const
{
    return m_volume;
}

void Settings::setVolume(double value)
{
    value = qBound(0.0, value, 1.0);
    if (!qFuzzyCompare(m_volume, value)) {
        m_volume = value;
        m_settings.setValue("playback/volume", value);
        emit volumeChanged();
        emit settingsChanged();
    }
}

QString Settings::outputDevice() const
{
    return m_outputDevice;
}

void Settings::setOutputDevice(const QString &device)
{
    if (m_outputDevice != device) {
        m_outputDevice = device;
        m_settings.setValue("audio/outputDevice", device);
        emit outputDeviceChanged();
        emit settingsChanged();
    }
}

int Settings::bufferSizeMs() const
{
    return m_bufferSizeMs;
}

void Settings::setBufferSizeMs(int ms)
{
    ms = qBound(10, ms, 1000);
    if (m_bufferSizeMs != ms) {
        m_bufferSizeMs = ms;
        m_settings.setValue("audio/bufferSizeMs", ms);
        emit bufferSizeMsChanged();
        emit settingsChanged();
    }
}

bool Settings::restoreSession() const
{
    return m_restoreSession;
}

void Settings::setRestoreSession(bool restore)
{
    if (m_restoreSession != restore) {
        m_restoreSession = restore;
        m_settings.setValue("session/restore", restore);
        emit restoreSessionChanged();
        emit settingsChanged();
    }
}

QStringList Settings::coverArtPatterns() const
{
    return m_coverArtPatterns;
}

void Settings::setCoverArtPatterns(const QStringList &patterns)
{
    if (m_coverArtPatterns != patterns) {
        m_coverArtPatterns = patterns;
        m_settings.setValue("coverArt/patterns", patterns);
        
        // Update the CoverImageProvider
        CoverImageProvider::setCoverPatterns(patterns);
        
        emit coverArtPatternsChanged();
        emit settingsChanged();
    }
}

void Settings::save()
{
    m_settings.setValue("playback/volume", m_volume);
    m_settings.setValue("audio/outputDevice", m_outputDevice);
    m_settings.setValue("audio/bufferSizeMs", m_bufferSizeMs);
    m_settings.setValue("session/restore", m_restoreSession);
    m_settings.setValue("coverArt/patterns", m_coverArtPatterns);
    m_settings.setValue("behavior/addTracksPolicy", m_addTracksPolicy);
    m_settings.setValue("behavior/previousButtonAction", m_previousButtonAction);
    m_settings.setValue("behavior/openingTracksAction", m_openingTracksAction);
    m_settings.setValue("behavior/generatedPlaylistCount", m_generatedPlaylistCount);
    m_settings.setValue("appearance/gridCellMinWidth", m_gridCellMinWidth);
    m_settings.setValue("appearance/gridCellMaxWidth", m_gridCellMaxWidth);
    m_settings.setValue("library/watcherEnabled", m_watcherEnabled);
    m_settings.setValue("library/periodicRescanMinutes", m_periodicRescanMinutes);
    m_settings.setValue("collection/playButtonEnabled", m_collectionPlayButtonEnabled);
    m_settings.setValue("collection/singleClickOpen", m_collectionSingleClickOpen);
    m_settings.sync();
}

void Settings::load()
{
    m_volume = m_settings.value("playback/volume", 1.0).toDouble();
    m_outputDevice = m_settings.value("audio/outputDevice", "").toString();
    m_bufferSizeMs = m_settings.value("audio/bufferSizeMs", 100).toInt();
    m_restoreSession = m_settings.value("session/restore", true).toBool();
    m_addTracksPolicy = m_settings.value("behavior/addTracksPolicy", AddNeverStart).toInt();
    m_previousButtonAction = m_settings.value("behavior/previousButtonAction", RestartThenJump).toInt();
    m_openingTracksAction = m_settings.value("behavior/openingTracksAction", OpeningAppendToViewed).toInt();
    m_generatedPlaylistCount = m_settings.value("behavior/generatedPlaylistCount", 5).toInt();
    m_gridCellMinWidth = m_settings.value("appearance/gridCellMinWidth", 100).toInt();
    m_gridCellMaxWidth = m_settings.value("appearance/gridCellMaxWidth", 200).toInt();
    m_watcherEnabled = m_settings.value("library/watcherEnabled", true).toBool();
    m_periodicRescanMinutes = m_settings.value("library/periodicRescanMinutes", 10).toInt();
    m_collectionPlayButtonEnabled = m_settings.value("collection/playButtonEnabled", true).toBool();
    m_collectionSingleClickOpen = m_settings.value("collection/singleClickOpen", false).toBool();
    
    // Load cover art patterns with defaults
    QStringList defaultPatterns = {
        "cover.jpg", "cover.jpeg", "cover.png",
        "folder.jpg", "folder.png",
        "front.jpg", "front.png",
        "%album%.jpg", "%album%.png",
        "*.jpg", "*.png"
    };
    m_coverArtPatterns = m_settings.value("coverArt/patterns", defaultPatterns).toStringList();
    
    // Apply to CoverImageProvider
    CoverImageProvider::setCoverPatterns(m_coverArtPatterns);
}


int Settings::addTracksPolicy() const
{
    return m_addTracksPolicy;
}

void Settings::setAddTracksPolicy(int policy)
{
    if (m_addTracksPolicy != policy) {
        m_addTracksPolicy = policy;
        m_settings.setValue("behavior/addTracksPolicy", policy);
        emit addTracksPolicyChanged();
        emit settingsChanged();
    }
}

int Settings::previousButtonAction() const
{
    return m_previousButtonAction;
}

void Settings::setPreviousButtonAction(int action)
{
    if (m_previousButtonAction != action) {
        m_previousButtonAction = action;
        m_settings.setValue("behavior/previousButtonAction", action);
        emit previousButtonActionChanged();
        emit settingsChanged();
    }
}

int Settings::openingTracksAction() const
{
    return m_openingTracksAction;
}

void Settings::setOpeningTracksAction(int action)
{
    if (m_openingTracksAction != action) {
        m_openingTracksAction = action;
        m_settings.setValue("behavior/openingTracksAction", action);
        emit openingTracksActionChanged();
        emit settingsChanged();
    }
}

int Settings::generatedPlaylistCount() const
{
    return m_generatedPlaylistCount;
}

void Settings::setGeneratedPlaylistCount(int count)
{
    count = qBound(1, count, 20);
    if (m_generatedPlaylistCount != count) {
        m_generatedPlaylistCount = count;
        m_settings.setValue("behavior/generatedPlaylistCount", count);
        emit generatedPlaylistCountChanged();
        emit settingsChanged();
    }
}

int Settings::gridCellMinWidth() const
{
    return m_gridCellMinWidth;
}

void Settings::setGridCellMinWidth(int width)
{
    width = qBound(60, width, 300);
    if (m_gridCellMinWidth != width) {
        m_gridCellMinWidth = width;
        m_settings.setValue("appearance/gridCellMinWidth", width);
        emit gridCellMinWidthChanged();
        emit settingsChanged();
    }
}

int Settings::gridCellMaxWidth() const
{
    return m_gridCellMaxWidth;
}

void Settings::setGridCellMaxWidth(int width)
{
    width = qBound(80, width, 400);
    if (m_gridCellMaxWidth != width) {
        m_gridCellMaxWidth = width;
        m_settings.setValue("appearance/gridCellMaxWidth", width);
        emit gridCellMaxWidthChanged();
        emit settingsChanged();
    }
}

QString Settings::groupTypeNextGroupBy(const QString &groupType) const
{
    QString key = QString("collection/%1/nextGroupBy").arg(groupType);
    
    // Defaults per group type
    static const QHash<QString, QString> defaults = {
        {"all", "albumartist"},
        {"albumartist", "album"},
        {"artist", "album"},
        {"album", "none"},
        {"disc", "none"},
        {"genre", "albumartist"},
        {"year", "album"},
        {"filetype", "bitrate"},
        {"bitrate", "genre"}
    };
    
    return m_settings.value(key, defaults.value(groupType, "none")).toString();
}

void Settings::setGroupTypeNextGroupBy(const QString &groupType, const QString &nextGroupBy)
{
    QString key = QString("collection/%1/nextGroupBy").arg(groupType);
    m_settings.setValue(key, nextGroupBy);
    emit settingsChanged();
}

QString Settings::groupTypeOpenAction(const QString &groupType) const
{
    QString key = QString("collection/%1/openAction").arg(groupType);
    return m_settings.value(key, "openPanel").toString();
}

void Settings::setGroupTypeOpenAction(const QString &groupType, const QString &openAction)
{
    QString key = QString("collection/%1/openAction").arg(groupType);
    m_settings.setValue(key, openAction);
    emit settingsChanged();
}

QString Settings::groupTypeSortBy(const QString &groupType, const QString &groupBy) const
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("sortBy"));
    const QString legacyKey = legacyCollectionViewSettingKey(groupBy, QStringLiteral("sortBy"));

    static const QHash<QString, QString> defaults = {
        {"all", "name"},
        {"albumartist", "name"},
        {"artist", "name"},
        {"album", "year"},
        {"disc", "name"},
        {"genre", "name"},
        {"year", "year"},
        {"filetype", "name"},
        {"bitrate", "name"},
        {"none", "name"}
    };

    if (m_settings.contains(key))
        return m_settings.value(key).toString();

    if (m_settings.contains(legacyKey))
        return m_settings.value(legacyKey).toString();

    return defaults.value(groupBy, "name");
}

void Settings::setGroupTypeSortBy(const QString &groupType, const QString &groupBy, const QString &sortBy)
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("sortBy"));
    m_settings.setValue(key, sortBy);
    emit settingsChanged();
}

bool Settings::groupTypeSortAscending(const QString &groupType, const QString &groupBy) const
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("sortAscending"));
    const QString legacyKey = legacyCollectionViewSettingKey(groupBy, QStringLiteral("sortAscending"));

    static const QHash<QString, bool> defaults = {
        {"all", true},
        {"albumartist", true},
        {"artist", true},
        {"album", false},
        {"disc", true},
        {"genre", true},
        {"year", false},
        {"filetype", true},
        {"bitrate", true},
        {"none", true}
    };

    if (m_settings.contains(key))
        return m_settings.value(key).toBool();

    if (m_settings.contains(legacyKey))
        return m_settings.value(legacyKey).toBool();

    return defaults.value(groupBy, true);
}

void Settings::setGroupTypeSortAscending(const QString &groupType, const QString &groupBy, bool ascending)
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("sortAscending"));
    m_settings.setValue(key, ascending);
    emit settingsChanged();
}

QString Settings::groupTypeSubtitle(const QString &groupType, const QString &groupBy) const
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("subtitle"));
    const QString legacyKey = legacyCollectionViewSettingKey(groupBy, QStringLiteral("subtitle"));

    static const QHash<QString, QString> defaults = {
        {"none", "duration"}
    };

    if (m_settings.contains(key))
        return m_settings.value(key).toString();

    if (m_settings.contains(legacyKey))
        return m_settings.value(legacyKey).toString();

    return defaults.value(groupBy, "count");
}

void Settings::setGroupTypeSubtitle(const QString &groupType, const QString &groupBy, const QString &subtitle)
{
    const QString key = collectionViewSettingKey(groupType, groupBy, QStringLiteral("subtitle"));
    m_settings.setValue(key, subtitle);
    emit settingsChanged();
}

bool Settings::watcherEnabled() const
{
    return m_watcherEnabled;
}

void Settings::setWatcherEnabled(bool enabled)
{
    if (m_watcherEnabled != enabled) {
        m_watcherEnabled = enabled;
        m_settings.setValue("library/watcherEnabled", enabled);
        emit watcherEnabledChanged();
        emit settingsChanged();
    }
}

int Settings::periodicRescanMinutes() const
{
    return m_periodicRescanMinutes;
}

void Settings::setPeriodicRescanMinutes(int minutes)
{
    minutes = qBound(0, minutes, 1440);  // 0 = disabled, max 24 hours
    if (m_periodicRescanMinutes != minutes) {
        m_periodicRescanMinutes = minutes;
        m_settings.setValue("library/periodicRescanMinutes", minutes);
        emit periodicRescanMinutesChanged();
        emit settingsChanged();
    }
}

bool Settings::collectionPlayButtonEnabled() const
{
    return m_collectionPlayButtonEnabled;
}

void Settings::setCollectionPlayButtonEnabled(bool enabled)
{
    if (m_collectionPlayButtonEnabled != enabled) {
        m_collectionPlayButtonEnabled = enabled;
        m_settings.setValue("collection/playButtonEnabled", enabled);
        emit collectionPlayButtonEnabledChanged();
        emit settingsChanged();
    }
}

bool Settings::collectionSingleClickOpen() const
{
    return m_collectionSingleClickOpen;
}

void Settings::setCollectionSingleClickOpen(bool enabled)
{
    if (m_collectionSingleClickOpen != enabled) {
        m_collectionSingleClickOpen = enabled;
        m_settings.setValue("collection/singleClickOpen", enabled);
        emit collectionSingleClickOpenChanged();
        emit settingsChanged();
    }
}
