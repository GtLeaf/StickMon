#!/usr/bin/env python3
"""Native round bag/shop text, controls and real service effects."""
import argparse,os,subprocess
from pathlib import Path
from PIL import Image
from test_sdgoods_market import ROOT,build
from build_sdgoods_resources import stage

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--external',action='store_true')
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--capture-only',action='store_true')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    resource=ROOT/'data'
    if a.external:resource=out/'resource-stage';stage(resource)
    binary=build(out,a.external,a.sanitize,host_source=ROOT/'tools/sdgoods_item_detail_host.cpp')
    subprocess.run([str(binary),str(resource),str(out),'capture' if a.capture_only else 'check'],
                   cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    for ppm in out.glob('*.ppm'):
        Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()

if __name__=='__main__':main()
