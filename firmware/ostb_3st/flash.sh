#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
port="${1:-/dev/cu.usbmodem2101}"
build_dir="${project_dir}/build"
if [[ ! -f "${build_dir}/flash_args" ]]; then
    echo "Build this target with ESP-IDF before flashing." >&2
    exit 1
fi
cd "${build_dir}"
python -m esptool --chip esp32s3 --port "${port}" --baud 115200 \
    --before default_reset --after hard_reset write_flash @flash_args
