# MusicPlayer (MVP)

Qt 6 + QML desktop music player (MVP playback). Target: Linux (Arch), Windows later.

## Requirements
- Qt 6.10 (base, declarative, multimedia)
- CMake >= 3.21

Arch packages (example):
- qt6-base qt6-declarative qt6-multimedia

## Build
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./appmusicplayer
```

## Notes
- Formats depend on your multimedia backend (GStreamer on Linux). Install GStreamer plugins for MP3/AAC/FLAC/Opus, etc.
- Gapless playback: initial implementation prepares the next track; precise gapless will be refined in later milestones.
