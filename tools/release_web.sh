#!/usr/bin/env bash
# Local publication only: no Git push, remote deployment or device flashing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if ! command -v emcc >/dev/null; then
  source "${EMSDK:-/workspace/emsdk}/emsdk_env.sh"
fi
exec python3 "$ROOT/tools/publish_web.py" --flasher "${FLASHER:-/workspace/projects}" "$@"
