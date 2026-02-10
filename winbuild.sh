#!/bin/bash
set -e

echo "=== Installing dependencies ==="
pacman -Syu --noconfirm
pacman -S --noconfirm --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-declarative \
    mingw-w64-x86_64-qt6-multimedia \
    mingw-w64-x86_64-qt6-tools \
    mingw-w64-x86_64-ffmpeg \
    mingw-w64-x86_64-taglib \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja

echo "=== Building project ==="
cd "$(dirname "$0")"
mkdir -p build
cd build
cmake .. -G Ninja
ninja

echo "=== Build complete ==="
echo "Executable: $(pwd)/appmusicplayer.exe"

echo ""
echo "=== Packaging for distribution ==="
cd "$(dirname "$0")"
bash deploy.sh

echo ""
echo "To distribute, zip the 'deploy' folder and share it."
