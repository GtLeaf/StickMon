#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
PROFILE="claw"
BUILD_DIR="$PROJECT_DIR/build-$PROFILE"
IDF_PATH="${IDF_PATH:-$HOME/.espressif/v5.5.4/esp-idf}"
PORT=""
ERASE=0
DEBUG=0

usage() {
    printf 'Usage: %s --port PORT [--variant claw|lite] [--debug] [--erase]\n' "$0"
    printf '\n'
    printf 'Builds and flashes the V1 firmware and resources from an isolated variant directory.\n'
    printf 'The default variant is claw (WeChat enabled).\n'
    printf 'Use --debug to expose the Debug entry in the main menu.\n'
    printf 'Use --erase once when recovering from a mixed or unknown image.\n'
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

BUILD_DIR="$PROJECT_DIR/build-$PROFILE"
BUILD_ARGS=(v1 "$PROFILE")
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

if ((ERASE)); then
    idf.py -B "$BUILD_DIR" -p "$PORT" erase-flash
fi

idf.py -B "$BUILD_DIR" -p "$PORT" flash monitor
