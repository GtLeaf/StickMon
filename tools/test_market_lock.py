#!/usr/bin/env python3
"""Market idle/manual lock must return the visible party before locking."""
from pathlib import Path
import subprocess
import tempfile

from amoled_host import ROOT, build_amoled_host


def main():
    with tempfile.TemporaryDirectory(prefix="market-lock-") as directory:
        binary = build_amoled_host(
            Path(directory) / "market-lock",
            [ROOT / "tools/market_lock_amoled_host.cpp"], debug=True)
        for mode in ("idle30", "idle120", "solo", "manual", "tutor", "never", "muted"):
            subprocess.run([str(binary), str(ROOT / "data"), mode], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
