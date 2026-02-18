cmake_minimum_required(VERSION 3.16)

project(MusicPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Concurrent Sql)
find_package(PkgConfig REQUIRED)

pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
pkg_check_modules(TAGLIB REQUIRED taglib)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appmusicplayer
    src/main.cpp
    src/AppViewModel.h
    src/AppViewModel.cpp
    src/TrackListModel.h
    src/TrackListModel.cpp
    src/PlaylistStore.h
    src/PlaylistStore.cpp
    src/PlaylistTabsModel.h
    src/PlaylistTabsModel.cpp
    src/QueueManager.h
    src/QueueManager.cpp
    src/audio/AudioEngine.h
    src/audio/AudioEngine.cpp
    src/audio/AudioOutputWorker.h
    src/audio/AudioOutputWorker.cpp
    src/audio/SPSCRingBuffer.h
    src/audio/SPSCRingBuffer.cpp
    src/audio/BufferIODevice.h
    src/audio/BufferIODevice.cpp
    src/library/LibraryDatabase.h
    src/library/LibraryDatabase.cpp
    src/library/LibraryScanner.h
    src/library/LibraryScanner.cpp
    src/library/LibraryWatcher.h
    src/library/LibraryWatcher.cpp
    src/Settings.h
    src/Settings.cpp
    src/SessionManager.h
    src/SessionManager.cpp
    src/MetadataExtractor.h
    src/MetadataExtractor.cpp
    src/ViewedPlaylistRouter.h
    src/ViewedPlaylistRouter.cpp
    src/PlaylistPanelController.h
    src/PlaylistPanelController.cpp
    src/BrowseActivationService.h
    src/BrowseActivationService.cpp
    src/WindowManager.h
    src/WindowManager.cpp
    src/CoverImageProvider.h
    src/CoverImageProvider.cpp
    src/TrackFilter.h
    src/CollectionBrowseModel.h
    src/CollectionBrowseModel.cpp
)

set_source_files_properties(src/AppViewModel.h PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(src/Settings.h PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(src/SessionManager.h PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(src/ViewedPlaylistRouter.h PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(src/WindowManager.h PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(qml/DragManager.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(appmusicplayer
    URI MusicPlayer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/DragManager.qml
        qml/components/CollectionBrowserContent.qml
        qml/components/IconImage.qml
        qml/panels/CollectionPanel.qml
        qml/panels/PlaylistPanel.qml
        qml/panels/CoverPanel.qml
        qml/panels/ControlsStrip.qml
        qml/windows/CollectionWindow.qml
        qml/windows/CoverWindow.qml
        qml/settings/SettingsWindow.qml
    RESOURCES
        qml/icons/skip_previous.svg
        qml/icons/play_arrow.svg
        qml/icons/pause.svg
        qml/icons/stop.svg
        qml/icons/skip_next.svg
        qml/icons/settings.svg
        qml/icons/arrow_back.svg
        qml/icons/arrow_forward.svg
        qml/icons/arrow_upward.svg
        qml/icons/home.svg
        qml/icons/folder.svg
        qml/icons/queue_music.svg
        qml/icons/music_note.svg
        qml/icons/album.svg
        qml/icons/expand_more.svg
        qml/icons/chevron_right.svg
        qml/icons/more_vert.svg
        qml/icons/volume_up.svg
    SOURCES
        src/AppViewModel.h src/AppViewModel.cpp
        src/Settings.h src/Settings.cpp
        src/SessionManager.h src/SessionManager.cpp
        src/ViewedPlaylistRouter.h src/ViewedPlaylistRouter.cpp
        src/PlaylistPanelController.h src/PlaylistPanelController.cpp
        src/WindowManager.h src/WindowManager.cpp
        src/CollectionBrowseModel.h src/CollectionBrowseModel.cpp
        src/library/LibraryDatabase.h src/library/LibraryDatabase.cpp
)

target_include_directories(appmusicplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(appmusicplayer PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Sql
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${TAGLIB_LIBRARIES}
)

install(TARGETS appmusicplayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
