#!/usr/bin/env bash
# Build an explicit browser target with the shared Python build driver.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/tools/build_web.py" "$@"
