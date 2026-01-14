# MusicPlayer

## Requirements

- Qt 6.5 or higher
- CMake 3.16 or higher
- FFmpeg development libraries (libavformat, libavcodec, libavutil, libswresample)
- TagLib development libraries

## Build

### Linux (Ubuntu/Debian)
```bash
sudo apt install qt6-base-dev qt6-declarative-dev qt6-multimedia-dev qt6-tools-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libtag1-dev cmake build-essential pkg-config
cd MusicPlayer
mkdir build && cd build
cmake ..
make
```

### Windows
```bash
# Install Qt 6.5+, CMake, FFmpeg dev libraries, TagLib
cd MusicPlayer
mkdir build && cd build
cmake ..
cmake --build .
```

## Run

```bash
./appmusicplayer
```
