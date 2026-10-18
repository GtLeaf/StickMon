#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.4/esp-idf}"

# ESP-IDF 5.5 currently has a Python 3.11 environment installed on the
# development machine, while macOS may provide a newer system python first in
# PATH. Pin the ESP-IDF environment before export.sh selects its interpreter.
if [[ -z "${IDF_PYTHON_ENV_PATH:-}" &&
      -x "$HOME/.espressif/python_env/idf5.5_py3.11_env/bin/python" ]]; then
    export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.11_env"
fi

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

# Existing build directories must adopt the room power-management settings too.
if [[ -f "$SDKCONFIG" && "$1" != 1_75c ]]; then
    python3 - "$SDKCONFIG" <<'PYCONFIG'
import re
import sys
from pathlib import Path
path = Path(sys.argv[1])
text = path.read_text()
for key, value in {
    "CONFIG_PM_ENABLE": "y",
    "CONFIG_PM_DFS_INIT_AUTO": "n",
    "CONFIG_FREERTOS_USE_TICKLESS_IDLE": "y",
    "CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION": "y",
}.items():
    line = key + "=" + value
    pattern = r"(?m)^(?:" + key + r"=.*|# " + key + r" is not set)$"
    if re.search(pattern, text):
        text = re.sub(pattern, line, text)
    else:
        text += "\n" + line + "\n"
path.write_text(text)
PYCONFIG
fi

# OTA requires a rollback-aware V2 bootloader even with cached sdkconfig.
if [[ "$1" == v2 && -f "$SDKCONFIG" ]]; then
    sed -i.bak 's/^# CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is not set$/CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y/' "$SDKCONFIG"
    rm -f "$SDKCONFIG.bak"
fi

source "$IDF_PATH/export.sh"
cd "$PROJECT_DIR"

AMOLED_ITEMS_DIR="${STICKMON_ESSENTIALS_DIR:-${ESSENTIALS_DIR:-$REPO_DIR/external/pokemon-essentials}}/Graphics/Items"
AMOLED_SHARED_UI="$REPO_DIR/data/packs/dev/game/ui.smonfx"
AMOLED_NATIVE_UI="$REPO_DIR/data/packs/dev/game/ui_amoled.smonfx"
AMOLED_UI_VALIDATOR="$REPO_DIR/tools/validate_amoled_ui_assets.py"
AMOLED_UI_CURRENT=false
if python3 "$AMOLED_UI_VALIDATOR" "$AMOLED_SHARED_UI" "$AMOLED_NATIVE_UI" \
        >/dev/null 2>&1 && [[ ! "$AMOLED_SHARED_UI" -nt "$AMOLED_NATIVE_UI" ]]; then
    AMOLED_UI_CURRENT=true
fi
if [[ -d "$AMOLED_ITEMS_DIR" && "$AMOLED_UI_CURRENT" == false ]]; then
    ASSET_PYTHON=""
    for CANDIDATE in "${STICKMON_ASSET_PYTHON:-}" \
                     /opt/homebrew/bin/python3 \
                     /usr/local/bin/python3 \
                     /usr/bin/python3 \
                     python3; do
        if [[ -n "$CANDIDATE" ]] && command -v "$CANDIDATE" >/dev/null 2>&1 \
           && "$CANDIDATE" -c 'import PIL' >/dev/null 2>&1; then
            ASSET_PYTHON="$(command -v "$CANDIDATE")"
            break
        fi
    done
    if [[ -z "$ASSET_PYTHON" ]]; then
        printf '%s\n' \
            "Pillow is required to regenerate AMOLED item assets from: $AMOLED_ITEMS_DIR" >&2
        printf '%s\n' \
            "Set STICKMON_ASSET_PYTHON to a Python interpreter that can import PIL." >&2
        exit 1
    fi
    "$ASSET_PYTHON" "$REPO_DIR/tools/generate_amoled_ui_assets.py" \
        --items-dir "$AMOLED_ITEMS_DIR"
elif [[ ! -f "$AMOLED_NATIVE_UI" ]]; then
    printf '%s\n' "AMOLED item sources are missing; set ESSENTIALS_DIR or STICKMON_ESSENTIALS_DIR." >&2
    exit 1
fi
python3 "$AMOLED_UI_VALIDATOR" "$AMOLED_SHARED_UI" "$AMOLED_NATIVE_UI"

OTA_ARGS=()
if [[ "$1" == v2 && -n "${STICKMON_OTA_ENDPOINT:-}" ]]; then
    OTA_ARGS+=("-DSTICKMON_OTA_ENDPOINT=$STICKMON_OTA_ENDPOINT")
fi
CMAKE_ARGS=(
    "-DSDKCONFIG=$SDKCONFIG"
    "-DSDKCONFIG_DEFAULTS=$DEFAULTS"
    "-DSTICKMON_ENABLE_CLAW=$ENABLE_CLAW"
    "-DSTICKMON_ENABLE_DEBUG_FEATURES=$DEBUG_FEATURES"
)
if ((${#OTA_ARGS[@]})); then
    idf.py -B "$BUILD_DIR" "${OTA_ARGS[@]}" "${CMAKE_ARGS[@]}" build
else
    idf.py -B "$BUILD_DIR" "${CMAKE_ARGS[@]}" build
fi

if [[ "$1" == v2 && "$DEBUG_FEATURES" == OFF ]]; then
    python3 "$SCRIPT_DIR/package_amoled_v2_release.py" "$BUILD_DIR"
fi
