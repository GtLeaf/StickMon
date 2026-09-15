#!/usr/bin/env bash

set -euo pipefail

if (($# < 4)); then
    printf 'Usage: %s BOARD IMAGE EXPECTED_SIZE EXPECTED_SHA256 [--port PORT] [--baud BAUD] [--skip-erase]\n' "$0" >&2
    exit 2
fi

BOARD="$1"
IMAGE="$2"
EXPECTED_SIZE="$3"
EXPECTED_SHA256="$4"
shift 4

PORT=""
BAUD="460800"
ERASE=1

usage() {
    printf 'Usage: %s BOARD IMAGE EXPECTED_SIZE EXPECTED_SHA256 [--port PORT] [--baud BAUD] [--skip-erase]\n' "$0"
    printf '\n'
    printf 'Flashes a complete Waveshare factory image for %s.\n' "$BOARD"
    printf 'The image is written at 0x0. A full erase is performed by default.\n'
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
        --baud)
            if (($# < 2)); then
                printf '%s\n' "Missing value for $1" >&2
                usage >&2
                exit 2
            fi
            BAUD="$2"
            shift 2
            ;;
        --skip-erase)
            ERASE=0
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

if [[ ! -f "$IMAGE" ]]; then
    printf 'Factory image not found: %s\n' "$IMAGE" >&2
    printf 'Download the matching Waveshare image and place it in the factory directory.\n' >&2
    exit 1
fi

actual_size="$(wc -c < "$IMAGE" | tr -d '[:space:]')"
if [[ "$actual_size" != "$EXPECTED_SIZE" ]]; then
    printf 'Factory image size mismatch: got %s bytes, expected %s bytes\n' \
        "$actual_size" "$EXPECTED_SIZE" >&2
    exit 1
fi

if command -v shasum >/dev/null 2>&1; then
    actual_sha256="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
elif command -v sha256sum >/dev/null 2>&1; then
    actual_sha256="$(sha256sum "$IMAGE" | awk '{print $1}')"
else
    printf '%s\n' "Neither shasum nor sha256sum is available." >&2
    exit 1
fi

if [[ "$actual_sha256" != "$EXPECTED_SHA256" ]]; then
    printf 'Factory image SHA-256 mismatch:\n  got:      %s\n  expected: %s\n' \
        "$actual_sha256" "$EXPECTED_SHA256" >&2
    exit 1
fi

if [[ -n "${ESPTOOL_PYTHON:-}" ]]; then
    PYTHON="$ESPTOOL_PYTHON"
elif [[ -n "${IDF_PYTHON_ENV_PATH:-}" && -x "$IDF_PYTHON_ENV_PATH/bin/python" ]]; then
    PYTHON="$IDF_PYTHON_ENV_PATH/bin/python"
elif [[ -x "$HOME/.espressif/python_env/idf5.5_py3.11_env/bin/python" ]]; then
    PYTHON="$HOME/.espressif/python_env/idf5.5_py3.11_env/bin/python"
else
    PYTHON="python3"
fi

if ! "$PYTHON" -m esptool version >/dev/null 2>&1; then
    printf 'esptool is unavailable for Python: %s\n' "$PYTHON" >&2
    printf 'Set ESPTOOL_PYTHON to a Python environment containing esptool.\n' >&2
    exit 1
fi

printf 'Board: %s\n' "$BOARD"
printf 'Image: %s (%s bytes)\n' "$IMAGE" "$actual_size"
printf 'SHA256: %s\n' "$actual_sha256"
printf 'Port: %s\n' "$PORT"

if ((ERASE)); then
    printf '%s\n' 'Erasing entire flash...'
    "$PYTHON" -m esptool \
        --chip esp32s3 \
        --port "$PORT" \
        --baud "$BAUD" \
        --before default_reset \
        --after hard_reset \
        erase_flash
fi

printf '%s\n' 'Writing factory image at 0x0...'
"$PYTHON" -m esptool \
    --chip esp32s3 \
    --port "$PORT" \
    --baud "$BAUD" \
    --before default_reset \
    --after hard_reset \
    write_flash \
    0x0 "$IMAGE"

printf '%s\n' 'Factory firmware flash complete.'
