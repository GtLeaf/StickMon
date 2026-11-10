#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -Eeuo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
UPLOAD_PORT=""
DEVICE_MAC=""
BUILD_MODE=release
DEBUG_FEATURES=OFF
FULL_STORAGE=OFF
STORAGE_PROFILE=16m
BUILD_ONLY=0
DRY_RUN=0
SIGNING_KEY="$PROJECT_ROOT/.ota/release-key.pem"

usage() {
    cat <<'EOF'
Usage: bash firmware/sdgoods/flash.sh --port PORT --mac MAC [options]

Build, sign and install SDGOODS firmware and its shared 9 MiB LittleFS resource image.
Unchanged regions are skipped after checking the device's complete digest.
Changed regions use esptool's post-write verification without a duplicate pass.
Preserves NVS saves, settings, touch calibration and PHY. No backup or chip
erase. Reserved userdata moves; no old userdata files are migrated.
The installer checks the device MAC, 32 MiB flash and known layout.

Options:
  -p, --port PORT        Serial port; close the serial monitor first.
      --mac MAC         Expected device MAC (required when installing).
      --debug           Enable Debug features; still uses a signed bundle.
      --full-storage    Use the 32 MiB layout with one 24 MiB resource image.
      --private-key KEY Signing key (default: .ota/release-key.pem).
      --build-only      Build and package without accessing the device.
      --dry-run         Print commands without building or accessing the device.
  -h, --help            Show this help.

ESP-IDF is located automatically, as in build.sh. IDF_PATH and
IDF_PYTHON_ENV_PATH can override its installation and Python environment.
EOF
}

fail() { echo "[sdgoods] $*" >&2; exit 2; }
need_value() {
    [[ $# -ge 2 && -n "$2" && "$2" != -* ]] || fail "Missing value for $1"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) need_value "$@"; UPLOAD_PORT="$2"; shift 2 ;;
        --mac) need_value "$@"; DEVICE_MAC="$2"; shift 2 ;;
        --private-key) need_value "$@"; SIGNING_KEY="$2"; shift 2 ;;
        --debug) BUILD_MODE=debug; DEBUG_FEATURES=ON; shift ;;
        --full-storage) FULL_STORAGE=ON; STORAGE_PROFILE=32m; shift ;;
        --build-only) BUILD_ONLY=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) fail "Unknown argument: $1 (see --help)" ;;
    esac
done

if [[ "$BUILD_ONLY" == 0 ]]; then
    [[ -n "$UPLOAD_PORT" ]] || fail "--port is required (or use --build-only)"
    [[ "$DEVICE_MAC" =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]] || fail "--mac must be the expected device MAC"
fi

if [[ "$SIGNING_KEY" != /* ]]; then SIGNING_KEY="$PWD/$SIGNING_KEY"; fi
cd "$PROJECT_ROOT"
# A defaults change needs a fresh sdkconfig; source-only edits reuse the cache.
DEFAULTS_HASH="$(shasum -a 256 firmware/sdgoods/sdkconfig.defaults)"
DEFAULTS_HASH="${DEFAULTS_HASH%% *}"
BUILD_DIR="$PROJECT_ROOT/firmware/sdgoods/build-flash-$BUILD_MODE-$STORAGE_PROFILE-${DEFAULTS_HASH:0:12}"
RUN_ID="$(date +%Y%m%d-%H%M%S)-$$"
BUNDLE_DIR="$PROJECT_ROOT/.stick-local/firmware/sdgoods/flash-$BUILD_MODE-$STORAGE_PROFILE-$RUN_ID"
LOG_DIR="$PROJECT_ROOT/.stick-local/artifacts/sdgoods-flash-$(date +%Y%m%d)/$RUN_ID"

if [[ -z "${IDF_PATH:-}" ]]; then
    for candidate in "$HOME/.espressif/v5.5.4/esp-idf" "$HOME/install/espidf/esp-idf-5.5.4-eim"; do
        if [[ -f "$candidate/export.sh" ]]; then export IDF_PATH="$candidate"; break; fi
    done
fi
export IDF_PYTHON_ENV_PATH="${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf5.5_py3.11_env}"
IDF_PYTHON="$IDF_PYTHON_ENV_PATH/bin/python"

if [[ "$DRY_RUN" == 0 ]]; then
    [[ -f "${IDF_PATH:-}/export.sh" ]] || fail "Set IDF_PATH to an ESP-IDF 5.5.x checkout"
    [[ -x "$IDF_PYTHON" ]] || fail "IDF Python not found: $IDF_PYTHON"
    [[ -f "$SIGNING_KEY" ]] || fail "Signing key not found: $SIGNING_KEY"
    if [[ "$BUILD_ONLY" == 0 ]]; then
        [[ -e "$UPLOAD_PORT" ]] || fail "Serial port not found: $UPLOAD_PORT"
    fi
    mkdir -p "$LOG_DIR"
fi

PHASE=prepare
trap 'echo "[sdgoods] Failed during $PHASE. See $LOG_DIR/run.log" >&2' ERR
run() {
    printf '[sdgoods]'
    printf ' %q' "$@"
    printf '\n'
    if [[ "$DRY_RUN" == 0 ]]; then
        "$@" 2>&1 | tee -a "$LOG_DIR/run.log"
    fi
}

PHASE=build
run bash "$PROJECT_ROOT/firmware/sdgoods/build.sh" -B "$BUILD_DIR" \
    -D "STICKMON_ENABLE_DEBUG_FEATURES=$DEBUG_FEATURES" \
    -D "STICKMON_SDGOODS_FULL_STORAGE=$FULL_STORAGE" \
    -D STICKMON_ENABLE_TRACE_LOGS=OFF -D STICKMON_ENABLE_ROOM_DIAGNOSTICS=OFF
PHASE=package
run "$IDF_PYTHON" tools/sdgoods_release.py package --build "$BUILD_DIR" \
    --output "$BUNDLE_DIR" --private-key "$SIGNING_KEY"
PHASE=inspect
run "$IDF_PYTHON" tools/sdgoods_release.py inspect --bundle "$BUNDLE_DIR"
if [[ "$BUILD_ONLY" == 0 ]]; then
    PHASE=install
    run "$IDF_PYTHON" tools/sdgoods_release.py install --bundle "$BUNDLE_DIR" \
        --port "$UPLOAD_PORT" --mac "$DEVICE_MAC" --execute
fi
if [[ "$DRY_RUN" == 1 ]]; then
    echo '[sdgoods] Dry run complete; no commands executed.'
else
    echo "[sdgoods] Done. Bundle: $BUNDLE_DIR"
    echo "[sdgoods] Log: $LOG_DIR/run.log"
fi
