"""Shared runtime probes for legacy and extracted AMOLED page test suites."""

import atexit
import functools
import subprocess
import tempfile
from pathlib import Path

from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]


@functools.lru_cache(maxsize=1)
def page_controller_binary() -> Path:
    directory = tempfile.TemporaryDirectory(prefix="amoled-page-cases-")
    atexit.register(directory.cleanup)
    return build_amoled_host(
        Path(directory.name) / "amoled-page-controllers",
        ["tools/amoled_page_controller_host.cpp"],
    )


def run_page_case(case: str) -> None:
    subprocess.run([str(page_controller_binary()), str(ROOT / "data"), case],
                   cwd=ROOT, check=True)
