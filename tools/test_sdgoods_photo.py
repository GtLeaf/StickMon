#!/usr/bin/env python3
"""Native real SDGOODS photograph and album workflow, no device access."""
import argparse,os,subprocess
from pathlib import Path
from test_sdgoods_market import ROOT,build

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
binary=build(out,sanitize=a.sanitize,debug=False,host_source=ROOT/'tools/sdgoods_photo_host.cpp')
run=out/'run';index=0
while run.exists():index+=1;run=out/f'run-{index}'
run.mkdir()
subprocess.run([str(binary),str(ROOT/'data'),str(run)],check=True,cwd=ROOT,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
from PIL import Image
for ppm in run.glob('*.ppm'):
    Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
