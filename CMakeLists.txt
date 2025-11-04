cmake_minimum_required(VERSION 3.21)
project(MusicPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6
find_package(Qt6 6.10 REQUIRED COMPONENTS Quick Multimedia)

# Configure Qt project defaults and policies
qt_standard_project_setup(REQUIRES 6.10)
qt_policy(SET QTP0001 NEW)

# Executable
qt_add_executable(appmusicplayer
    src/main.cpp
    src/PlayerController.cpp
    src/PlayerController.h
)

# QML module
qt_add_qml_module(appmusicplayer
    URI MusicPlayer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

# Link Qt modules
target_link_libraries(appmusicplayer PRIVATE
    Qt6::Quick
    Qt6::Multimedia
)

install(TARGETS appmusicplayer RUNTIME DESTINATION bin)
