#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "$0")" && pwd)"
# Accept configuration options only; no additional IDF action can be requested.
BUILD_OPTIONS=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        -B|--build-dir|-D|--define-cache-entry)
            if [ "$#" -lt 2 ]; then echo "Missing value for $1" >&2; exit 2; fi
            BUILD_OPTIONS+=("$1" "$2")
            shift 2
            ;;
        -D?*|--build-dir=*|--define-cache-entry=*)
            BUILD_OPTIONS+=("$1")
            shift
            ;;
        -h|--help)
            echo 'Usage: build.sh [-B BUILD_DIR] [-D NAME=VALUE] (build only)'
            exit 0
            ;;
        *) echo "Unsupported build option: $1" >&2; exit 2 ;;
    esac
done
if [ "${#BUILD_OPTIONS[@]}" -eq 0 ]; then BUILD_OPTIONS=(-B build-native); fi
if [ -z "${IDF_PATH:-}" ]; then
    for candidate in "$HOME/.espressif/v5.5.4/esp-idf" "$HOME/install/espidf/esp-idf-5.5.4-eim"; do
        if [ -f "$candidate/export.sh" ]; then export IDF_PATH="$candidate"; break; fi
    done
fi
if [ ! -f "${IDF_PATH:-}/export.sh" ]; then
    echo 'Set IDF_PATH to an ESP-IDF 5.5.x checkout.' >&2
    exit 1
fi
export IDF_PYTHON_ENV_PATH="${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf5.5_py3.11_env}"
source "$IDF_PATH/export.sh" >/dev/null 2>&1
cd "$TASK_ROOT"
# Intentionally build only. No serial operations or flash commands.
env -u CODEBUDDY_SAFE_DELETE_SANDBOX -u CODEBUDDY_BROKERED_FS_HOOK_ENABLED \
    -u CODEBUDDY_SAFE_DELETE_ENABLED idf.py "${BUILD_OPTIONS[@]}" build
echo 'SDGOODS uses two OTA apps with one shared resource image. Package/install with tools/sdgoods_release.py; do not use idf.py flash or generated flash_args.'
