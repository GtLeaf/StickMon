#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "room_editor" / "generate_sprite_previews.py"


def main():
    return subprocess.call([sys.executable, str(GENERATOR)])


if __name__ == "__main__":
    raise SystemExit(main())
