"""Read the explicit AMOLED source manifests used by every runtime target."""

import re
from pathlib import Path


MAIN = Path("firmware/amoled_1_8_v1/main")


def read_source_manifest(manifest: Path, base: Path) -> list[Path]:
    """Reject missing, duplicate and non-local source entries early."""
    sources = []
    for line in manifest.read_text(encoding="utf-8").splitlines():
        entry = line.strip()
        if not entry or entry.startswith("#"):
            continue
        relative = Path(entry)
        if relative.is_absolute() or ".." in relative.parts or relative.suffix not in (".cpp", ".c"):
            raise ValueError(f"Invalid source entry in {manifest}: {entry}")
        source = base / relative
        if not source.is_file():
            raise ValueError(f"Source listed in {manifest} does not exist: {source}")
        if source in sources:
            raise ValueError(f"Duplicate source in {manifest}: {entry}")
        sources.append(source)
    return sources


def amoled_app_sources(root: Path) -> list[Path]:
    main = root / MAIN
    return read_source_manifest(main / "app/sources.txt", main)


def amoled_ui_sources(root: Path) -> list[Path]:
    main = root / MAIN
    return read_source_manifest(main / "ui/sources.txt", main)


def amoled_shared_sources(root: Path) -> list[Path]:
    return read_source_manifest(root / "firmware/common/amoled_shared_sources.txt", root)


def amoled_runtime_sources(root: Path) -> list[Path]:
    sources = amoled_app_sources(root) + amoled_ui_sources(root) + amoled_shared_sources(root)
    if len(set(sources)) != len(sources):
        raise ValueError("Duplicate source across AMOLED manifests")
    return sources


def read_app_source(root: Path) -> str:
    """Combined application source for the remaining architecture assertions."""
    return "\n".join(path.read_text(encoding="utf-8") for path in amoled_app_sources(root))


def read_home_source(root: Path) -> str:
    """Combined source for legacy source assertions, including remaining .inc files."""
    def read_source(path):
        text = path.read_text(encoding="utf-8")

        def expand(match):
            return read_source(path.parent / match.group(1))

        return re.sub(r'#include "([^" ]+\.inc)"', expand, text)

    return "\n".join(read_source(path) for path in amoled_ui_sources(root))
