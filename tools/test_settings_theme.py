"""Verify grouped settings gestures, persistence and theme rendering with real code."""
from pathlib import Path
import argparse
import os
import subprocess
import sys
from amoled_host import ROOT, build_amoled_host


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--debug',action='store_true')
    args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    for debug in (0,1):
        binary=out/f'controls-{debug}'
        subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
            '-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections',
            '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections',
            '-Isrc','-Ifirmware/amoled_1_8_v1/main',
            f'-DSTICKMON_ENABLE_DEBUG_FEATURES={debug}',
            'tools/settings_controls_host.cpp',
            'firmware/amoled_1_8_v1/main/app/SettingsController.cpp',
            'firmware/amoled_1_8_v1/main/ui/SettingsScreens.cpp',
            'firmware/amoled_1_8_v1/main/ui/UiCommon.cpp','-o',str(binary)],cwd=ROOT,check=True)
        subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    binary=build_amoled_host(out/'settings-app',
        ['tools/amoled_page_controller_host.cpp','tools/amoled_checksum_guard.cpp'],debug=args.debug)
    for case in ('settings-sliders','settings-actions','settings-reset','page-exit-gestures'):
        subprocess.run([str(binary),str(ROOT/'data'),case],cwd=ROOT,check=True)
    subprocess.run([str(binary),str(ROOT/'data'),'settings-theme',str(out)],cwd=ROOT,check=True)
    from PIL import Image
    for path in out.glob('*.ppm'):
        with Image.open(path) as image:image.save(path.with_suffix('.png'))


if __name__=='__main__':main()
