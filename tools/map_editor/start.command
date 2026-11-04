#!/bin/sh
set -eu
editor_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$editor_root"
editor_python=${MAP_EDITOR_PYTHON:-"$editor_root/.stick-local/venvs/map-editor/bin/python"}
if [ ! -x "$editor_python" ]; then
    if [ -n "${MAP_EDITOR_PYTHON:-}" ]; then
        echo "MAP_EDITOR_PYTHON does not point to an executable Python."
        exit 1
    fi
    python3 -m venv "$editor_root/.stick-local/venvs/map-editor"
fi
if ! "$editor_python" -c 'import PIL' >/dev/null 2>&1; then
    "$editor_python" -m pip install -r tools/map_editor/requirements.txt
fi
exec "$editor_python" tools/map_editor/serve.py "$@"
