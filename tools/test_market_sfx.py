#!/usr/bin/env python3
"""Exercise market sound timing through the real application and PCM output."""
from pathlib import Path
import subprocess
import sys
import tempfile
from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]

if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='market-sfx-') as tmp:
        extra = []
        if sys.platform != 'darwin':
            stub = Path(tmp) / 'uzlib_stub.c'
            stub.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n'
                            'unsigned uzlib_crc32(const void*d,unsigned l,unsigned c){(void)d;(void)l;return c;}\n')
            extra.append(stub)
        binary = build_amoled_host(Path(tmp) / 'market-sfx',
                                  [ROOT / 'tools/market_sfx_amoled_host.cpp', *extra], debug=True)
        subprocess.run([str(binary), str(ROOT / 'data')], check=True, cwd=ROOT)
