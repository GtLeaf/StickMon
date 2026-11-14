#!/usr/bin/env python3
"""Market idle/manual lock must return the visible party before locking."""
from pathlib import Path
import subprocess
import sys
import tempfile

from amoled_host import ROOT, build_amoled_host


def main():
    with tempfile.TemporaryDirectory(prefix="market-lock-") as directory:
        extra = []
        if sys.platform != "darwin":
            # GNU ld retains unused uzlib checksum references, as in other App hosts.
            stub = Path(directory) / "uzlib_stub.c"
            stub.write_text("unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n"
                            "unsigned uzlib_crc32(const void*d,unsigned l,unsigned c){(void)d;(void)l;return c;}\n")
            extra.append(stub)
        binary = build_amoled_host(
            Path(directory) / "market-lock",
            [ROOT / "tools/market_lock_amoled_host.cpp", *extra], debug=True)
        for mode in ("idle30", "idle120", "solo", "manual", "tutor", "never", "muted"):
            subprocess.run([str(binary), str(ROOT / "data"), mode], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
