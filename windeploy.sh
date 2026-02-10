#!/bin/bash
set -e

# =============================================================================
# deploy.sh — Create a self-contained distributable Windows folder
#
# Run this from the MSYS2 MinGW64 shell after building:
#   bash deploy.sh
#
# Result: ./deploy/ folder containing the exe, all DLLs, Qt plugins, and
#         QML modules. Zip it and distribute.
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DEPLOY_DIR="$SCRIPT_DIR/deploy"
EXE_NAME="appmusicplayer.exe"
MINGW_PREFIX="/mingw64"
MINGW_BIN="$MINGW_PREFIX/bin"
QT_PLUGIN_DIR="$MINGW_PREFIX/share/qt6/plugins"
QT_QML_DIR="$MINGW_PREFIX/share/qt6/qml"

if [ ! -f "$BUILD_DIR/$EXE_NAME" ]; then
    echo "ERROR: $BUILD_DIR/$EXE_NAME not found. Build the project first."
    exit 1
fi

echo "=== Creating deployment directory ==="
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# Copy the executable
cp "$BUILD_DIR/$EXE_NAME" "$DEPLOY_DIR/"

# ---- Step 1: Collect all MinGW DLLs recursively via ldd ----
echo "=== Collecting DLLs (recursive ldd) ==="

collect_deps() {
    local binary="$1"
    ldd "$binary" 2>/dev/null \
        | grep -i "$(cygpath -u "$MINGW_BIN" 2>/dev/null || echo "$MINGW_BIN")" \
        | awk '{print $3}' \
        | sort -u
}

PREV_COUNT=0
while true; do
    for f in "$DEPLOY_DIR"/*.exe "$DEPLOY_DIR"/*.dll; do
        [ -f "$f" ] || continue
        collect_deps "$f" | while read -r dll; do
            BASENAME="$(basename "$dll")"
            if [ -f "$dll" ] && [ ! -f "$DEPLOY_DIR/$BASENAME" ]; then
                cp "$dll" "$DEPLOY_DIR/"
                echo "  + $BASENAME"
            fi
        done
    done
    CUR_COUNT=$(ls -1 "$DEPLOY_DIR"/*.dll 2>/dev/null | wc -l)
    [ "$CUR_COUNT" -eq "$PREV_COUNT" ] && break
    PREV_COUNT=$CUR_COUNT
done

echo "  Collected $(ls -1 "$DEPLOY_DIR"/*.dll 2>/dev/null | wc -l) DLLs"

# ---- Step 2: Copy required Qt plugins ----
echo "=== Copying Qt plugins ==="

QT_PLUGINS_NEEDED=(
    platforms        # qwindows.dll — mandatory for any Qt GUI app
    imageformats     # JPEG, PNG, SVG, etc.
    sqldrivers       # SQLite driver (used by LibraryDatabase)
    multimedia       # media backend
    styles           # Windows style
    tls              # HTTPS/TLS support
    networkinformation
    generic
)

for plugin in "${QT_PLUGINS_NEEDED[@]}"; do
    SRC="$QT_PLUGIN_DIR/$plugin"
    if [ -d "$SRC" ]; then
        mkdir -p "$DEPLOY_DIR/$plugin"
        cp -r "$SRC"/* "$DEPLOY_DIR/$plugin/"
        echo "  + $plugin/"
        # Also resolve DLL deps from plugin DLLs
        for pdll in "$DEPLOY_DIR/$plugin"/*.dll; do
            [ -f "$pdll" ] || continue
            collect_deps "$pdll" | while read -r dll; do
                BASENAME="$(basename "$dll")"
                if [ -f "$dll" ] && [ ! -f "$DEPLOY_DIR/$BASENAME" ]; then
                    cp "$dll" "$DEPLOY_DIR/"
                    echo "    + $BASENAME (from plugin)"
                fi
            done
        done
    fi
done

# ---- Step 3: Copy required QML modules ----
echo "=== Copying QML modules ==="

QML_MODULES_NEEDED=(
    QtQuick
    QtQuick/Controls
    QtQuick/Layouts
    QtQuick/Templates
    QtQuick/Window
    QtQuick/Dialogs
    QtQml
    QtQml/Models
    QtQml/WorkerScript
    QtCore
    QtMultimedia
    Qt/labs
)

for mod in "${QML_MODULES_NEEDED[@]}"; do
    SRC="$QT_QML_DIR/$mod"
    if [ -d "$SRC" ]; then
        mkdir -p "$DEPLOY_DIR/qml/$mod"
        cp -r "$SRC"/* "$DEPLOY_DIR/qml/$mod/"
        echo "  + qml/$mod/"
        # Resolve DLL deps from QML plugin DLLs
        find "$DEPLOY_DIR/qml/$mod" -name "*.dll" 2>/dev/null | while read -r qdll; do
            collect_deps "$qdll" | while read -r dll; do
                BASENAME="$(basename "$dll")"
                if [ -f "$dll" ] && [ ! -f "$DEPLOY_DIR/$BASENAME" ]; then
                    cp "$dll" "$DEPLOY_DIR/"
                    echo "    + $BASENAME (from QML module)"
                fi
            done
        done
    fi
done

# ---- Step 4: Create qt.conf to tell Qt where to find plugins/QML ----
echo "=== Creating qt.conf ==="
cat > "$DEPLOY_DIR/qt.conf" << 'EOF'
[Paths]
Plugins = .
QmlImports = qml
EOF

echo ""
echo "========================================="
echo "  Deployment complete!"
echo "========================================="
echo "Folder: $DEPLOY_DIR"
echo ""
DLL_COUNT=$(ls -1 "$DEPLOY_DIR"/*.dll 2>/dev/null | wc -l)
TOTAL_SIZE=$(du -sh "$DEPLOY_DIR" | cut -f1)
echo "  DLLs:      $DLL_COUNT"
echo "  Total size: $TOTAL_SIZE"
echo ""
echo "To distribute: zip the 'deploy' folder and share it."
echo "The user just needs to unzip and run $EXE_NAME."
