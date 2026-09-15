"""Read the AMOLED renderer with its extracted implementation fragments."""

import re
from pathlib import Path


def amoled_ui_sources(root: Path) -> list[Path]:
    """Use the same renderer manifest as both firmware targets."""
    main = root / "firmware/amoled_1_8_v1/main"
    entries = (main / "ui/sources.txt").read_text(encoding="utf-8").splitlines()
    return [main / entry.strip() for entry in entries
            if entry.strip() and not entry.strip().startswith("#")]


def read_home_source(root: Path) -> str:
    """Combined source for legacy source assertions, including remaining .inc files."""
    def read_source(path):
        text = path.read_text(encoding="utf-8")

        def expand(match):
            return read_source(path.parent / match.group(1))

        return re.sub(r'#include "([^" ]+\.inc)"', expand, text)

    return "\n".join(read_source(path) for path in amoled_ui_sources(root))
