cmake_minimum_required(VERSION 3.21)
project(MusicPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Avoid optional Vulkan dependency during Qt Quick configuration
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

# Qt 6
find_package(Qt6 6.10 REQUIRED COMPONENTS Quick Multimedia)

# TagLib for reliable metadata reading
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)

# FFmpeg for PlaybackEngine (required)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# Configure Qt project defaults and policies
qt_standard_project_setup(REQUIRES 6.10)
qt_policy(SET QTP0001 NEW)

# Source files
set(SOURCES
    src/main.cpp
    src/PlayerController.cpp
    src/PlayerController.h
    src/PlaylistModel.cpp
    src/PlaylistModel.h
    src/TrackMetadata.cpp
    src/TrackMetadata.h
    src/MetadataReader.cpp
    src/MetadataReader.h
    src/PlaybackEngine.cpp
    src/PlaybackEngine.h
    src/PlaybackQueue.cpp
    src/PlaybackQueue.h
)

# Executable
qt_add_executable(appmusicplayer ${SOURCES})

# QML module
qt_add_qml_module(appmusicplayer
    URI MusicPlayer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

# Link Qt modules, TagLib, and FFmpeg
target_link_libraries(appmusicplayer PRIVATE
    Qt6::Quick
    Qt6::Multimedia
    ${TAGLIB_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

# Add include directories
target_include_directories(appmusicplayer PRIVATE
    ${TAGLIB_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
)

# Enable compiler optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(appmusicplayer PRIVATE -O3)
endif()

install(TARGETS appmusicplayer RUNTIME DESTINATION bin)
