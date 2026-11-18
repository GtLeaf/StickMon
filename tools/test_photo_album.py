#!/usr/bin/env python3
"""Actual AMOLED photo workflow, using the production application and files."""
import argparse,subprocess
from pathlib import Path
from amoled_host import ROOT,build_amoled_host
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
binary=build_amoled_host(out/'photo-amoled',[ROOT/'tools/photo_amoled_host.cpp',ROOT/'tools/amoled_checksum_guard.cpp'],debug=False)
run=out/'run';attempt=0
while run.exists():attempt+=1;run=out/f'run-{attempt}'
run.mkdir()
subprocess.run([str(binary),str(ROOT/'data'),str(run)],cwd=ROOT,check=True)
from PIL import Image
for ppm in run.glob('*.ppm'):
    Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
