#!/usr/bin/env python3
"""Compile production engine and both bag UI families for market inventory use."""
from pathlib import Path
import subprocess
import sys
import tempfile
from build_web import ROOT, sources_for, ui_flags_for
from amoled_host import build_amoled_host

def main():
    output = ROOT / '.stick-local/artifacts/market-season-20261113/bag'
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='market-bag-') as temp:
        checksum = Path(temp) / 'checksum.c'
        checksum.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\nunsigned uzlib_crc32(const void*d,unsigned l,unsigned c){(void)d;(void)l;return c;}\n')
        checksum_obj = Path(temp) / 'checksum.o'
        subprocess.run(['cc', '-c', str(checksum), '-o', str(checksum_obj)], check=True)
        obj = Path(temp) / 'tinflate.o'
        subprocess.run(['cc', '-O1', '-c', str(ROOT / 'src/third_party/uzlib/tinflate.c'), '-o', str(obj)], check=True)
        for target in ('sticks3', 'ostb'):
            binary = output / ('engine-' + target)
            sources = [p for p in sources_for(target) if p.suffix == '.cpp' and '/platform/web/' not in str(p)]
            subprocess.run(['c++', '-std=c++17', '-O1', '-ffunction-sections', '-fdata-sections',
                            '-Wl,--gc-sections', '-Isrc', '-DSTICKMON_ENABLE_DEBUG_FEATURES=0',
                            '-DSTICKMON_ENABLE_TRACE_LOGS=0', '-DSTICKMON_HAS_CLAW=0', *ui_flags_for(target),
                            'tools/market_bag_engine_host.cpp', 'src/platform/desktop/DesktopPlatform.cpp',
                            *map(str, sources), str(obj), str(checksum_obj), '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary), str(ROOT / 'data')], check=True, cwd=ROOT)
        journey = build_amoled_host(output / 'amoled', [ROOT / 'tools/market_bag_amoled_host.cpp', checksum], debug=True)
        subprocess.run([str(journey), str(ROOT / 'data')], check=True, cwd=ROOT)

if __name__ == '__main__':
    main()
