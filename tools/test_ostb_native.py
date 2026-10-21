#!/usr/bin/env python3
"""Run the real button game at the OSTB resolution and export scene captures."""
from pathlib import Path
import subprocess
from build_web import ROOT, sources_for

def main():
    output = ROOT / 'build/ostb-native'
    output.mkdir(parents=True, exist_ok=True)
    sources = [p for p in sources_for('sticks3') if '/platform/web/' not in str(p)]
    objects = []
    for source in sources:
        if source.suffix == '.c':
            obj = output / (source.stem + '.o')
            subprocess.run(['cc', '-O1', '-ffunction-sections', '-fdata-sections', '-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
    binary = output / 'ostb-native-host'
    subprocess.run(['c++','-std=c++17','-O1','-ffunction-sections','-fdata-sections','-Wl,-dead_strip','-Isrc','-Ifirmware/common',
        '-DSTICKMON_DISPLAY_WIDTH=296','-DSTICKMON_DISPLAY_HEIGHT=240',
        '-DSTICKMON_ROOM_PIXEL_SCALE=1',
        '-DSTICKMON_TOUCH_INPUT=1',
        '-DSTICKMON_DEDICATED_POWER_BUTTON=1','-DSTICKMON_ENABLE_DEBUG_FEATURES=0',
        '-DSTICKMON_ENABLE_TRACE_LOGS=0','-DSTICKMON_HAS_CLAW=0',
        'tools/ostb_native_host.cpp','src/platform/desktop/DesktopPlatform.cpp',
        *[str(p) for p in sources if p.suffix=='.cpp'],*objects,'-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary),str(ROOT/'data'),str(output)],cwd=ROOT,check=True)
    print('Captures:',output)
if __name__=='__main__':
    main()
