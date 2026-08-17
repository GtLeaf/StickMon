"""Portable paths shared by asset generation tools."""

import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ESSENTIALS_DIR = ROOT / "external" / "pokemon-essentials"


def essentials_dir():
    """Return the configured Pokemon Essentials directory."""
    configured = os.environ.get("ESSENTIALS_DIR")
    return Path(configured).expanduser() if configured else DEFAULT_ESSENTIALS_DIR


def portable_asset_path(path, asset_root=None):
    """Describe a source file without persisting a developer's absolute path."""
    path = Path(path)
    root = Path(asset_root) if asset_root else essentials_dir()
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return path.name
