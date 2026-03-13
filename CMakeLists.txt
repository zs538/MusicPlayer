cmake_minimum_required(VERSION 3.16)

project(MusicPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(MSYS2_MINGW_PREFIX "C:/msys64/mingw64")

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Concurrent Sql Svg)

set(THIRD_PARTY_INCLUDE_DIRS "")
set(THIRD_PARTY_LIBRARIES "")

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
    pkg_check_modules(TAGLIB REQUIRED taglib)

    list(APPEND THIRD_PARTY_INCLUDE_DIRS
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
        ${TAGLIB_INCLUDE_DIRS}
    )

    list(APPEND THIRD_PARTY_LIBRARIES
        ${AVFORMAT_LIBRARIES}
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${TAGLIB_LIBRARIES}
    )
else()
    if(WIN32)
        find_path(FFMPEG_INCLUDE_DIR
            NAMES libavformat/avformat.h
            PATHS ${MSYS2_MINGW_PREFIX}/include
            NO_DEFAULT_PATH
        )
        find_path(TAGLIB_INCLUDE_DIR
            NAMES taglib/fileref.h
            PATHS ${MSYS2_MINGW_PREFIX}/include
            NO_DEFAULT_PATH
        )

        find_library(AVFORMAT_LIBRARY
            NAMES avformat libavformat.dll.a avformat-62
            PATHS ${MSYS2_MINGW_PREFIX}/lib
            NO_DEFAULT_PATH
        )
        find_library(AVCODEC_LIBRARY
            NAMES avcodec libavcodec.dll.a avcodec-62
            PATHS ${MSYS2_MINGW_PREFIX}/lib
            NO_DEFAULT_PATH
        )
        find_library(AVUTIL_LIBRARY
            NAMES avutil libavutil.dll.a avutil-60
            PATHS ${MSYS2_MINGW_PREFIX}/lib
            NO_DEFAULT_PATH
        )
        find_library(SWRESAMPLE_LIBRARY
            NAMES swresample libswresample.dll.a swresample-5
            PATHS ${MSYS2_MINGW_PREFIX}/lib
            NO_DEFAULT_PATH
        )
        find_library(TAGLIB_LIBRARY
            NAMES tag libtag.dll.a taglib
            PATHS ${MSYS2_MINGW_PREFIX}/lib
            NO_DEFAULT_PATH
        )

        if(NOT FFMPEG_INCLUDE_DIR OR NOT TAGLIB_INCLUDE_DIR OR NOT AVFORMAT_LIBRARY OR NOT AVCODEC_LIBRARY OR NOT AVUTIL_LIBRARY OR NOT SWRESAMPLE_LIBRARY OR NOT TAGLIB_LIBRARY)
            message(FATAL_ERROR "Could not find FFmpeg/TagLib in C:/msys64/mingw64. Install the MSYS2 mingw64 FFmpeg and TagLib packages, then re-run CMake.")
        endif()

        list(APPEND THIRD_PARTY_INCLUDE_DIRS
            ${FFMPEG_INCLUDE_DIR}
            ${TAGLIB_INCLUDE_DIR}
        )

        list(APPEND THIRD_PARTY_LIBRARIES
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWRESAMPLE_LIBRARY}
            ${TAGLIB_LIBRARY}
            # MinGW system libraries for threading/time functions
            -lwinpthread -lwinmm
        )
    else()
        find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swresample)
        find_package(TagLib REQUIRED)

        list(APPEND THIRD_PARTY_INCLUDE_DIRS
            ${FFMPEG_INCLUDE_DIRS}
            ${TagLib_INCLUDE_DIRS}
        )

        if(TARGET FFmpeg::avformat)
            list(APPEND THIRD_PARTY_LIBRARIES
                FFmpeg::avformat
                FFmpeg::avcodec
                FFmpeg::avutil
                FFmpeg::swresample
            )
        else()
            list(APPEND THIRD_PARTY_LIBRARIES
                ${FFMPEG_LIBRARIES}
            )
        endif()

        if(TARGET TagLib::TagLib)
            list(APPEND THIRD_PARTY_LIBRARIES TagLib::TagLib)
        else()
            list(APPEND THIRD_PARTY_LIBRARIES ${TagLib_LIBRARIES})
        endif()
    endif()
endif()

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

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(qml/DragManager.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

set_source_files_properties(qml/components/TrackListColumnsSupport.qml PROPERTIES
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
        qml/components/TrackColumnsHeader.qml
        qml/components/TrackColumnsRow.qml
        qml/components/TrackListColumnsSupport.qml
        qml/panels/CollectionPanel.qml
        qml/panels/PlaylistPanel.qml
        qml/panels/CoverPanel.qml
        qml/panels/ControlsStrip.qml
        qml/windows/CoverWindow.qml
        qml/settings/SettingsWindow.qml
    RESOURCES
        qml/logo-image-t.png
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
        qml/icons/search.svg
        qml/icons/close.svg
        qml/icons/sort.svg
        qml/icons/filter_alt.svg
        qml/icons/tab_group.svg
        qml/icons/more_vert.svg
        qml/icons/volume_up.svg
        qml/icons/volume_off.svg
    SOURCES
        src/AppViewModel.h src/AppViewModel.cpp
        src/Settings.h src/Settings.cpp
        src/SessionManager.h src/SessionManager.cpp
        src/ViewedPlaylistRouter.h src/ViewedPlaylistRouter.cpp
        src/PlaylistPanelController.h src/PlaylistPanelController.cpp
        src/CollectionBrowseModel.h src/CollectionBrowseModel.cpp
        src/library/LibraryDatabase.h src/library/LibraryDatabase.cpp
)

target_include_directories(appmusicplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${THIRD_PARTY_INCLUDE_DIRS}
)

target_link_libraries(appmusicplayer PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Sql
    Qt6::Svg
    ${THIRD_PARTY_LIBRARIES}
)

target_compile_definitions(appmusicplayer PRIVATE
    APP_VERSION="Pre-Release"
)

install(TARGETS appmusicplayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
