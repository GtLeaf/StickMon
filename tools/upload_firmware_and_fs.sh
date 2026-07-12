#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_NAME="m5stick-s3"
UPLOAD_PORT=""
DRY_RUN=0

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
the complete data/ directory as a LittleFS image.

Options:
  -p, --port PORT          Upload port, for example /dev/cu.usbmodem2101.
                           Omit this option to let PlatformIO auto-detect it.
  -e, --environment NAME   PlatformIO environment. Default: m5stick-s3.
      --dry-run            Print the commands without executing them.
  -h, --help               Show this help.

Environment:
  PIO_BIN                  Override the PlatformIO executable path.
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

echo "[flash] 1/4 Building firmware..."
run_pio

echo "[flash] 2/4 Building LittleFS image..."
run_pio -t buildfs

echo "[flash] 3/4 Uploading firmware and partition table..."
if [[ -n "$UPLOAD_PORT" ]]; then
    run_pio -t upload --upload-port "$UPLOAD_PORT"
else
    run_pio -t upload
fi

if [[ "$DRY_RUN" != "1" ]]; then
    sleep 2
fi

echo "[flash] 4/4 Uploading LittleFS resources..."
if [[ -n "$UPLOAD_PORT" ]]; then
    run_pio -t uploadfs --upload-port "$UPLOAD_PORT"
else
    run_pio -t uploadfs
fi

echo "[flash] Firmware and LittleFS upload completed."
