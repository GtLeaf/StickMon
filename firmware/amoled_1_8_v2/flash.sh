#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
PROFILE="claw"
BUILD_DIR="$PROJECT_DIR/build-$PROFILE"
IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.4/esp-idf}"
if [[ -z "${IDF_PYTHON_ENV_PATH:-}" &&
      -x "$HOME/.espressif/python_env/idf5.5_py3.11_env/bin/python" ]]; then
    export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.11_env"
fi
PORT=""
BAUD=115200
ERASE=0
DEBUG=0

usage() {
    printf 'Usage: %s --port PORT [--variant claw|lite] [--debug] [--erase]\n' "$0"
    printf '\n'
    printf 'Builds and flashes the V2 firmware and resources from this directory.\n'
    printf 'The default variant is claw (WeChat enabled).\n'
    printf 'Use --debug to expose the Debug entry in the main menu.\n'
    printf 'Flashing uses 115200 baud; close serial monitors and log capture first.\n'
    printf 'WARNING: --erase deletes all flash data, including saves and credentials.\n'
    printf 'Switching V1/V2 firmware does not normally require --erase.\n'
}

check_port_available() {
    local holders status
    if ! command -v lsof >/dev/null 2>&1; then
        printf 'lsof is required to check serial port ownership.\n' >&2
        exit 1
    fi
    if holders="$(lsof -n -P "$PORT" 2>&1)"; then
        printf 'Serial port is busy: %s\n%s\n' "$PORT" "$holders" >&2
        printf 'Stop the listed serial monitor or log capture before retrying.\n' >&2
        exit 1
    else
        status=$?
        # lsof returns 1 with no output when no process holds the port.
        if [[ "$status" != 1 || -n "$holders" ]]; then
            printf 'Unable to check serial port: %s\n%s\n' "$PORT" "$holders" >&2
            exit 1
        fi
    fi
}

while (($# > 0)); do
    case "$1" in
        --port|-p)
            if (($# < 2)); then
                printf '%s\n' "Missing value for $1" >&2
                usage >&2
                exit 2
            fi
            PORT="$2"
            shift 2
            ;;
        --variant|-v)
            if (($# < 2)); then
                printf '%s\n' "Missing value for $1" >&2
                usage >&2
                exit 2
            fi
            PROFILE="$2"
            shift 2
            ;;
        --erase)
            ERASE=1
            shift
            ;;
        --debug)
            DEBUG=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf '%s\n' "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$PORT" ]]; then
    printf '%s\n' "A serial port is required." >&2
    usage >&2
    exit 2
fi

case "$PROFILE" in
    claw|lite) ;;
    *)
        printf 'Unknown variant: %s\n' "$PROFILE" >&2
        usage >&2
        exit 2
        ;;
esac

check_port_available

BUILD_DIR="$PROJECT_DIR/build-$PROFILE"
BUILD_ARGS=(v2 "$PROFILE")
if ((DEBUG)); then
    BUILD_DIR+="-debug"
    BUILD_ARGS+=(debug)
fi

if [[ ! -f "$IDF_PATH/export.sh" ]]; then
    printf 'ESP-IDF export script not found: %s/export.sh\n' "$IDF_PATH" >&2
    exit 1
fi

source "$IDF_PATH/export.sh"
cd "$PROJECT_DIR"

"$SCRIPT_DIR/../../tools/build_amoled_variant.sh" "${BUILD_ARGS[@]}"

printf 'Flashing AMOLED V2 (CO5300 / CST820 target), variant=%s, debug=%s\n' \
    "$PROFILE" "$DEBUG"
printf 'Build directory: %s\n' "$BUILD_DIR"

check_port_available

if ((ERASE)); then
    idf.py -B "$BUILD_DIR" -p "$PORT" -b "$BAUD" erase-flash
    check_port_available
fi

idf.py -B "$BUILD_DIR" -p "$PORT" -b "$BAUD" flash monitor
