#include "Settings.h"
#include "CoverArtProvider.h"

static Settings *s_instance = nullptr;

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

int Settings::playbackMode() const
{
    return m_playbackMode;
}

void Settings::setPlaybackMode(int mode)
{
    if (m_playbackMode != mode) {
        m_playbackMode = mode;
        m_settings.setValue("playback/mode", mode);
        emit playbackModeChanged();
        emit settingsChanged();
    }
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
        
        // Update the CoverArtProvider
        CoverArtProvider *provider = CoverArtProvider::instance();
        if (provider) {
            provider->setCoverPatterns(patterns);
        }
        
        emit coverArtPatternsChanged();
        emit settingsChanged();
    }
}

void Settings::save()
{
    m_settings.setValue("playback/mode", m_playbackMode);
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
    m_settings.sync();
}

void Settings::load()
{
    m_playbackMode = m_settings.value("playback/mode", GaplessSession).toInt();
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
    
    // Load cover art patterns with defaults
    QStringList defaultPatterns = {
        "cover.jpg", "cover.jpeg", "cover.png",
        "folder.jpg", "folder.png",
        "front.jpg", "front.png",
        "%album%.jpg", "%album%.png",
        "*.jpg", "*.png"
    };
    m_coverArtPatterns = m_settings.value("coverArt/patterns", defaultPatterns).toStringList();
    
    // Apply to CoverArtProvider if it exists
    CoverArtProvider *provider = CoverArtProvider::instance();
    if (provider) {
        provider->setCoverPatterns(m_coverArtPatterns);
    }
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

QString Settings::groupTypeViewMode(const QString &groupType) const
{
    QString key = QString("collection/%1/viewMode").arg(groupType);
    
    // Defaults: grid for visual types, list for others
    static const QHash<QString, QString> defaults = {
        {"albumartist", "grid"},
        {"artist", "grid"},
        {"album", "grid"},
        {"genre", "grid"}
    };
    
    return m_settings.value(key, defaults.value(groupType, "list")).toString();
}

void Settings::setGroupTypeViewMode(const QString &groupType, const QString &viewMode)
{
    QString key = QString("collection/%1/viewMode").arg(groupType);
    m_settings.setValue(key, viewMode);
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
