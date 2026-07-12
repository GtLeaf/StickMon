#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="$ROOT_DIR/data"
PIO_BIN="${PIO_BIN:-$HOME/.platformio/penv/bin/pio}"
BACKUP_DIR=""
RESTORE_DATA=0
HAD_DATA=0
DRY_RUN=0

usage() {
    cat <<'EOF'
Usage: tools/upload_empty_littlefs.sh [--dry-run]

Temporarily replaces the local data/ directory with an empty one, uploads the
empty LittleFS image to the device, then restores the original local data/.

Options:
  --dry-run   Show what would happen without uploading or moving files.
  -h, --help  Show this help.

Environment:
  PIO_BIN     PlatformIO binary path. Defaults to $HOME/.platformio/penv/bin/pio.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

restore_data() {
    local exit_code=$?
    trap - EXIT INT TERM
    if [[ "$RESTORE_DATA" != "1" ]]; then
        return "$exit_code"
    fi

    RESTORE_DATA=0
    if [[ "$HAD_DATA" == "1" ]]; then
        if [[ -z "$BACKUP_DIR" || ! -e "$BACKUP_DIR" ]]; then
            echo "[emptyfs] Backup missing; leaving current data/ untouched: $BACKUP_DIR" >&2
            return 1
        fi
        rm -rf "$DATA_DIR"
        mv "$BACKUP_DIR" "$DATA_DIR"
        echo "[emptyfs] Restored local data/."
    else
        rm -rf "$DATA_DIR"
        echo "[emptyfs] Removed temporary empty data/."
    fi
    return "$exit_code"
}

handle_signal() {
    local exit_code="$1"
    trap - INT TERM
    exit "$exit_code"
}

trap restore_data EXIT
trap 'handle_signal 130' INT
trap 'handle_signal 143' TERM

if [[ ! -x "$PIO_BIN" ]]; then
    echo "[emptyfs] PlatformIO not found: $PIO_BIN" >&2
    echo "[emptyfs] Set PIO_BIN=/path/to/pio if needed." >&2
    exit 1
fi

echo "[emptyfs] Project: $ROOT_DIR"
echo "[emptyfs] PlatformIO: $PIO_BIN"

if [[ "$DRY_RUN" == "1" ]]; then
    echo "[emptyfs] Dry run only."
    echo "[emptyfs] Would move: $DATA_DIR -> $ROOT_DIR/.tmp_data_before_empty_littlefs_<timestamp>"
    echo "[emptyfs] Would create empty: $DATA_DIR"
    echo "[emptyfs] Would run: $PIO_BIN run -t uploadfs"
    echo "[emptyfs] Would restore original data/ after upload."
    exit 0
fi

BACKUP_DIR="$ROOT_DIR/.tmp_data_before_empty_littlefs_$(date +%Y%m%d_%H%M%S)"
if [[ -e "$DATA_DIR" ]]; then
    if [[ -e "$BACKUP_DIR" ]]; then
        echo "[emptyfs] Backup path already exists: $BACKUP_DIR" >&2
        exit 1
    fi
    HAD_DATA=1
    mv "$DATA_DIR" "$BACKUP_DIR"
    echo "[emptyfs] Moved local data/ to $(basename "$BACKUP_DIR")."
else
    echo "[emptyfs] Local data/ does not exist; creating an empty one."
fi

RESTORE_DATA=1
mkdir -p "$DATA_DIR"

echo "[emptyfs] Uploading empty LittleFS image..."
(cd "$ROOT_DIR" && "$PIO_BIN" run -t uploadfs)

echo "[emptyfs] Empty LittleFS uploaded."
echo "[emptyfs] Device should now show resource-missing alerts until you upload the normal FS image."
