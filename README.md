# MusicPlayer (MVP)
Qt 6 + QML desktop music player with a light-weight minimal FFmpeg-based audio engine. Native Linux support, Windows support comming soon.

## Core Features
- Minimal audio engine supporting gapless and bit-perfect playback
- Playback queue in form of various playlists 
- Completely customisable UI allowing for multiple layouts and custom QML panels

## Requirements
- Qt 6.10 (Quick, Multimedia)
- CMake >= 3.21
- TagLib
- FFmpeg libraries: libavformat, libavcodec, libavutil, libswresample

## Build
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./appmusicplayer
```

## Notes
- Supported formats depend on Qt Multimedia and the installed FFmpeg codecs on your system.
