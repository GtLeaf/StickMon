#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/../../tools/flash_amoled_factory.sh" \
    "ESP32-S3-Touch-AMOLED-1.8 V1 (SH8601/FT3168)" \
    "$SCRIPT_DIR/factory/ESP32-S3-Touch-AMOLED-1.8-FactoryXiaozhi_250805.bin" \
    16732160 \
    033ba27f0d1824835e90fe6b41d2db8c1f13cda7e1d80c82b3f7537dafb8dc8d \
    "$@"
