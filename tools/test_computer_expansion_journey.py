#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,os
from PIL import Image
from test_sdgoods_market import build,ROOT
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/computer-expansion-20261123/journey');p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 cache=out/'native';cache.mkdir(exist_ok=True)
 binary=build(cache,sanitize=a.sanitize,host_source=ROOT/'tools/computer_expansion_journey_host.cpp')
 subprocess.run([str(binary),str(ROOT/'data'),str(out)],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 for ppm in out.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
if __name__=='__main__':main()
