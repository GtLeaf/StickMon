#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/../../tools/flash_amoled_factory.sh" \
    "ESP32-S3-Touch-AMOLED-1.8 V2 (CO5300/CST820)" \
    "$SCRIPT_DIR/factory/ESP32-S3-Touch-AMOLED-1.8-V2-FactoryXiaozhi_260601.bin" \
    16777216 \
    6f188fb9d35ee793a3423934a4fa4e7c1fef9cc9dae76f9f177dabe854a6cdb3 \
    "$@"
