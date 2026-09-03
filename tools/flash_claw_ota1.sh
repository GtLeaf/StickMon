#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IDF_PATH_DEFAULT="/Users/gtleaf/.espressif/v5.5.4/esp-idf"
IDF_PATH="${IDF_PATH:-$IDF_PATH_DEFAULT}"
PORT="${PORT:-/dev/cu.usbmodem2101}"
BUILD_DIR="$PROJECT_DIR/firmware/amoled_1_8_v1/build-claw"
IMAGE="$BUILD_DIR/stickmon_amoled_1_8_v1.bin"

usage() {
    cat <<'EOF'
Usage:
  tools/flash_claw_ota1.sh            Flash the Claw image to ota_1
  tools/flash_claw_ota1.sh monitor    Open the build-claw serial monitor

Environment overrides:
  PORT=/dev/cu.usbmodemXXXX
  IDF_PATH=/path/to/esp-idf
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ ! -f "$IDF_PATH/export.sh" ]]; then
    printf 'ESP-IDF export script not found: %s/export.sh\n' "$IDF_PATH" >&2
    exit 1
fi

if [[ ! -e "$PORT" ]]; then
    printf 'Serial port not found: %s\n' "$PORT" >&2
    exit 1
fi

source "$IDF_PATH/export.sh" >/dev/null
cd "$PROJECT_DIR"

if [[ "${1:-}" == "monitor" ]]; then
    exec idf.py -B "$BUILD_DIR" -p "$PORT" monitor
fi

if [[ "${1:-}" != "" ]]; then
    usage >&2
    exit 2
fi

if [[ ! -f "$IMAGE" ]]; then
    printf 'Claw firmware image not found: %s\n' "$IMAGE" >&2
    printf 'Build it first with: STICKMON_ESPCLAW_ROOT=/Users/gtleaf/project/esp/espclaw/esp-claw-master ./tools/build_amoled_variant.sh v1 claw\n' >&2
    exit 1
fi

cat <<EOF
About to write the Claw image to ota_1 at 0x320000.
No flash erase will be performed.
Port: $PORT
Image: $IMAGE

If connection fails, hold BOOT, press RESET once, keep BOOT held for about
one second, release it, and run this script again.
EOF

python -m esptool \
    --chip esp32s3 \
    --port "$PORT" \
    -b 460800 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    --flash_mode dio \
    --flash_size 16MB \
    --flash_freq 80m \
    0x320000 "$IMAGE"

printf '\nFlash complete. Start the monitor with:\n'
printf '  %s monitor\n' "$PROJECT_DIR/tools/flash_claw_ota1.sh"
