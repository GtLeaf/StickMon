#!/usr/bin/env python3
"""Run market entry/return journeys against maintained device UI families."""
from pathlib import Path
import argparse
import subprocess
import sys

from amoled_host import build_amoled_host
from build_web import ROOT, sources_for
from test_explore_ui_parity import compile_and_run


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/market-controls-20261102/scenes')
    output=parser.parse_args().output.resolve()
    output.mkdir(parents=True,exist_ok=True)
    result,_=compile_and_run('sticks3',output/'stick',host=ROOT/'tools/market_scene_host.cpp',sanitize=True)
    print(result.strip())
    amoled=build_amoled_host(output/'market-amoled',[ROOT/'tools/market_amoled_host.cpp'])
    (output/'amoled').mkdir(exist_ok=True)
    subprocess.run([str(amoled),str(ROOT/'data'),str(output/'amoled')],check=True)
    # Select the compact family source list, then apply the SDGOODS round
    # overrides and explicit hardware-independent UI profile. No OSTB target
    # or hardware sources are built.
    overrides=list((ROOT/'src/ui/round_touch').glob('*.cpp'))
    replaced={p.name for p in overrides}
    sources=[p for p in sources_for('ostb') if '/platform/web/' not in str(p) and
             not (p.parent.name=='compact_touch' and p.name in replaced)]
    sources+=overrides+[ROOT/'src/platform/desktop/DesktopPlatform.cpp',ROOT/'tools/market_scene_host.cpp']
    flags=['-DSTICKMON_UI_COMPACT_TOUCH=1','-DSTICKMON_UI_ROUND_TOUCH=1','-DSTICKMON_TOUCH_INPUT=1',
           '-DSTICKMON_DISPLAY_WIDTH=360','-DSTICKMON_DISPLAY_HEIGHT=360',
           '-DSTICKMON_UI_HEADER_INSET_X=60','-DSTICKMON_UI_HEADER_TOP=32',
           '-DSTICKMON_ROOM_PIXEL_SCALE=1','-DSTICKMON_DEDICATED_POWER_BUTTON=1',
           '-DSTICKMON_ENABLE_DEBUG_FEATURES=0','-DSTICKMON_ENABLE_TRACE_LOGS=0','-DSTICKMON_HAS_CLAW=0']
    obj=output/'tinflate.o'
    subprocess.run(['cc','-O1','-fsanitize=address,undefined','-c',str(ROOT/'src/third_party/uzlib/tinflate.c'),'-o',str(obj)],check=True)
    binary=output/'market-round'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-ffunction-sections','-fdata-sections','-I'+str(ROOT/'src'),*flags,
                    *[str(p) for p in sources if p.suffix=='.cpp'],str(obj),
                    '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections','-o',str(binary)],check=True)
    subprocess.run([str(binary),str(ROOT/'data'),str(output)],check=True)


if __name__=='__main__':main()
