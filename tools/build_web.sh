#!/usr/bin/env bash
# Build StickMon for Web (Emscripten/WASM)
# Usage: source /path/to/emsdk/emsdk_env.sh && ./tools/build_web.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT/web"
SRC_DIR="$ROOT/src"
OBJ_DIR="$ROOT/build/web-obj"

mkdir -p "$OUT_DIR" "$OBJ_DIR"

CFLAGS="-I$SRC_DIR -O2 -DNDEBUG -DSTICKMON_ENABLE_TRACE_LOGS=0"
CXXFLAGS="$CFLAGS -std=c++17"

echo "[build_web] Collecting sources..."

# All game/platform-agnostic sources:
#  - exclude Arduino entry (main.cpp)
#  - exclude m5stick_s3 platform (ESP32 hardware)
#  - exclude desktop platform (native test harness)
CPP_SOURCES=$(find "$SRC_DIR" -name "*.cpp" \
    -not -path "*/platform/m5stick_s3/*" \
    -not -path "*/platform/desktop/*" \
    -not -name "main.cpp" | sort)
C_SOURCES=$(find "$SRC_DIR" -name "*.c" \
    -not -path "*/platform/m5stick_s3/*" \
    -not -path "*/platform/desktop/*" | sort)

echo "[build_web] $(echo "$CPP_SOURCES" | wc -l) C++ files, $(echo "$C_SOURCES" | wc -l) C files"

echo "[build_web] Compiling..."
OBJECTS=""

for src in $C_SOURCES; do
    obj="$OBJ_DIR/$(echo "$src" | sed "s|$SRC_DIR/||; s|/|_|g; s|\.c$|.o|")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        emcc -c "$src" $CFLAGS -o "$obj"
    fi
    OBJECTS="$OBJECTS $obj"
done

for src in $CPP_SOURCES; do
    obj="$OBJ_DIR/$(echo "$src" | sed "s|$SRC_DIR/||; s|/|_|g; s|\.cpp$|.o|")"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        em++ -c "$src" $CXXFLAGS -o "$obj"
    fi
    OBJECTS="$OBJECTS $obj"
done

echo "[build_web] Linking..."

em++ $OBJECTS \
    -O2 \
    -s WASM=1 \
    -s INITIAL_MEMORY=67108864 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s MAXIMUM_MEMORY=268435456 \
    -s EXPORTED_FUNCTIONS='["_main","_stickmon_button_down","_stickmon_button_up"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s FORCE_FILESYSTEM=1 \
    -s EXIT_RUNTIME=0 \
    --preload-file "$ROOT/data"@/ \
    -o "$OUT_DIR/stickmon.js"

echo "[build_web] Done. Output:"
ls -lh "$OUT_DIR"/stickmon.{js,wasm,data} 2>/dev/null
echo ""
echo "Serve the web/ directory over HTTP and open index.html"
