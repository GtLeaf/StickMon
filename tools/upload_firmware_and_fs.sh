#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_NAME="m5stick-s3-debug"
UPLOAD_PORT=""
DRY_RUN=0
# 编译产物导出目录（供 tools/web-flasher 上传固件用），可用 OUT_DIR 覆盖
OUT_DIR="${OUT_DIR:-$ROOT_DIR/tools/out}"
# boot_app0.bin 在 PlatformIO 框架目录而非工程构建目录，可用 BOOT_APP0_BIN 覆盖
BOOT_APP0_BIN="${BOOT_APP0_BIN:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

if [[ -n "${PIO_BIN:-}" ]]; then
    PIO_BIN="$PIO_BIN"
elif command -v pio >/dev/null 2>&1; then
    PIO_BIN="$(command -v pio)"
else
    PIO_BIN="$HOME/.platformio/penv/bin/pio"
fi

usage() {
    cat <<'EOF'
Usage: tools/upload_firmware_and_fs.sh [options]

Builds and uploads the firmware (including the partition table), then uploads
the complete data/ directory as a LittleFS image. After building, the files
needed by tools/web-flasher (bootloader / partitions / boot_app0 / firmware /
littlefs .bin) are exported to tools/out/.

Options:
  -p, --port PORT          Upload port, for example /dev/cu.usbmodem2101.
                           Omit this option to let PlatformIO auto-detect it.
      --release            Build and upload the m5stick-s3 release environment.
                           Default: m5stick-s3-debug.
  -e, --environment NAME   Override the PlatformIO environment.
  -o, --out DIR            Export directory for build artifacts.
                           Default: tools/out.
      --dry-run            Print the commands without executing them.
  -h, --help               Show this help.

Environment:
  PIO_BIN                  Override the PlatformIO executable path.
  OUT_DIR                  Same as --out (the option wins if both are given).
  BOOT_APP0_BIN            Override the boot_app0.bin path.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)
            [[ $# -ge 2 ]] || { echo "[flash] Missing value for $1" >&2; exit 2; }
            UPLOAD_PORT="$2"
            shift 2
            ;;
        -e|--environment)
            [[ $# -ge 2 ]] || { echo "[flash] Missing value for $1" >&2; exit 2; }
            ENV_NAME="$2"
            shift 2
            ;;
        --release)
            ENV_NAME="m5stick-s3"
            shift
            ;;
        -o|--out)
            [[ $# -ge 2 ]] || { echo "[flash] Missing value for $1" >&2; exit 2; }
            OUT_DIR="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[flash] Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ ! -x "$PIO_BIN" ]]; then
    echo "[flash] PlatformIO not found: $PIO_BIN" >&2
    echo "[flash] Set PIO_BIN=/path/to/pio if needed." >&2
    exit 1
fi

if [[ ! -f "$ROOT_DIR/platformio.ini" ]]; then
    echo "[flash] platformio.ini not found under: $ROOT_DIR" >&2
    exit 1
fi

if [[ ! -d "$ROOT_DIR/data" ]]; then
    echo "[flash] LittleFS source directory not found: $ROOT_DIR/data" >&2
    exit 1
fi

run_pio() {
    if [[ "$DRY_RUN" == "1" ]]; then
        printf '[flash] Would run:'
        printf ' %q' "$PIO_BIN" run -e "$ENV_NAME" "$@"
        printf '\n'
        return 0
    fi
    (cd "$ROOT_DIR" && "$PIO_BIN" run -e "$ENV_NAME" "$@")
}

on_error() {
    local exit_code=$?
    echo "[flash] Failed with exit code $exit_code." >&2
    echo "[flash] Make sure the device is connected and no serial monitor is using the port." >&2
    exit "$exit_code"
}
trap on_error ERR

echo "[flash] Project: $ROOT_DIR"
echo "[flash] Environment: $ENV_NAME"
echo "[flash] Port: ${UPLOAD_PORT:-auto-detect}"
echo "[flash] PlatformIO: $PIO_BIN"
echo "[flash] Close any running 'pio device monitor' before continuing."

echo "[flash] Checking resource pack compatibility..."
if [[ "$DRY_RUN" == "1" ]]; then
    printf '[flash] Would run: %q %q --validate-packs\n' \
        python3 "$ROOT_DIR/tools/generate_game_assets.py"
else
    (cd "$ROOT_DIR" && PYTHONDONTWRITEBYTECODE=1 \
        python3 tools/generate_game_assets.py --validate-packs)
fi

echo "[flash] 1/5 Building firmware..."
run_pio

echo "[flash] 2/5 Building LittleFS image..."
run_pio -t buildfs

echo "[flash] 3/5 Exporting build artifacts to: $OUT_DIR"
BUILD_DIR="$ROOT_DIR/.pio/build/$ENV_NAME"
# 与 tools/web-flasher 上传表单一一对应的 5 个 bin
ARTIFACTS=(
    "$BUILD_DIR/bootloader.bin"
    "$BUILD_DIR/partitions.bin"
    "$BOOT_APP0_BIN"
    "$BUILD_DIR/firmware.bin"
    "$BUILD_DIR/littlefs.bin"
)
if [[ "$DRY_RUN" == "1" ]]; then
    for src in "${ARTIFACTS[@]}"; do
        printf '[flash] Would copy: %q -> %q\n' "$src" "$OUT_DIR/$(basename "$src")"
    done
else
    for src in "${ARTIFACTS[@]}"; do
        if [[ ! -f "$src" ]]; then
            echo "[flash] Build artifact missing: $src" >&2
            exit 1
        fi
    done
    mkdir -p "$OUT_DIR"
    for src in "${ARTIFACTS[@]}"; do
        cp -f "$src" "$OUT_DIR/$(basename "$src")"
        echo "[flash] Exported: $OUT_DIR/$(basename "$src")"
    done
fi

echo "[flash] 4/5 Uploading firmware and partition table..."
if [[ -n "$UPLOAD_PORT" ]]; then
    run_pio -t upload --upload-port "$UPLOAD_PORT"
else
    run_pio -t upload
fi

if [[ "$DRY_RUN" != "1" ]]; then
    sleep 2
fi

echo "[flash] 5/5 Uploading LittleFS resources..."
if [[ -n "$UPLOAD_PORT" ]]; then
    run_pio -t uploadfs --upload-port "$UPLOAD_PORT"
else
    run_pio -t uploadfs
fi

echo "[flash] Firmware and LittleFS upload completed."
if [[ "$DRY_RUN" != "1" ]]; then
    echo "[flash] Build artifacts for web-flasher are in: $OUT_DIR"
fi
