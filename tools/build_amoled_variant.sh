#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.4/esp-idf}"

usage() {
    printf 'Usage: %s <v1|v2|1_75c> <claw|lite> [debug]\n' "$0"
    printf '\n'
    printf 'Builds one AMOLED firmware variant in an isolated build directory.\n'
}

if (($# < 2 || $# > 3)); then
    usage >&2
    exit 2
fi

case "$1" in
    v1) PROJECT_DIR="$REPO_DIR/firmware/amoled_1_8_v1" ;;
    v2) PROJECT_DIR="$REPO_DIR/firmware/amoled_1_8_v2" ;;
    1_75c) PROJECT_DIR="$REPO_DIR/firmware/amoled_1_75c" ;;
    *)
        printf 'Unknown board: %s\n' "$1" >&2
        usage >&2
        exit 2
        ;;
esac

case "$2" in
    claw) ENABLE_CLAW=ON ;;
    lite) ENABLE_CLAW=OFF ;;
    *)
        printf 'Unknown variant: %s\n' "$2" >&2
        usage >&2
        exit 2
        ;;
esac

if [[ ! -f "$IDF_PATH/export.sh" ]]; then
    printf 'ESP-IDF export script not found: %s/export.sh\n' "$IDF_PATH" >&2
    exit 1
fi

PROFILE="$2"
BUILD_DIR="$PROJECT_DIR/build-$PROFILE"
SDKCONFIG="$BUILD_DIR/sdkconfig"
DEFAULTS="$PROJECT_DIR/sdkconfig.defaults;$PROJECT_DIR/sdkconfig.defaults.$PROFILE"
DEBUG_FEATURES=OFF
if (($# == 3)); then
    if [[ "$3" != debug ]]; then
        printf 'Unknown feature profile: %s\n' "$3" >&2
        usage >&2
        exit 2
    fi
    DEBUG_FEATURES=ON
    BUILD_DIR="$PROJECT_DIR/build-$PROFILE-debug"
    SDKCONFIG="$BUILD_DIR/sdkconfig"
fi

# A build directory may predate the selected profile. ESP-IDF preserves values
# already present in sdkconfig, so discard only a stale profile config; this
# keeps user-specific options while ensuring claw/lite capability flags match.
if [[ -f "$SDKCONFIG" ]]; then
    if [[ "$PROFILE" == claw ]] && {
        ! grep -q '^CONFIG_APP_CLAW_CAP_IM_WECHAT=y$' "$SDKCONFIG" ||
        ! grep -q '^CONFIG_APP_CLAW_CAP_IM_TG=n$' "$SDKCONFIG" ||
        ! grep -q '^CONFIG_APP_CLAW_CAP_IM_FEISHU=y$' "$SDKCONFIG" ||
        ! grep -q '^CONFIG_FATFS_LFN_HEAP=y$' "$SDKCONFIG";
    }; then
        rm -f "$SDKCONFIG"
    elif [[ "$PROFILE" == lite ]] && grep -q '^CONFIG_STICKMON_CLAW_ENABLE=y$' "$SDKCONFIG"; then
        rm -f "$SDKCONFIG"
    fi
fi

source "$IDF_PATH/export.sh"
cd "$PROJECT_DIR"

idf.py -B "$BUILD_DIR" \
    "-DSDKCONFIG=$SDKCONFIG" \
    "-DSDKCONFIG_DEFAULTS=$DEFAULTS" \
    "-DSTICKMON_ENABLE_CLAW=$ENABLE_CLAW" \
    "-DSTICKMON_ENABLE_DEBUG_FEATURES=$DEBUG_FEATURES" \
    build
