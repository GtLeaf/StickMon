#!/usr/bin/env python3
"""Native SDGOODS standby policy/care and engine integration proof."""
import argparse,os,subprocess
from pathlib import Path
from test_sdgoods_market import ROOT,build
def check_wiring():
    main=(ROOT/'firmware/sdgoods/main/main.cpp').read_text()
    assert main.index('resumeStandbyBoot(board,catalog)')<main.index('board.begin()')
    platform=(ROOT/'firmware/sdgoods/main/SdgoodsPlatform.cpp').read_text()
    minimal=platform.split('bool SdgoodsPlatform::beginStandbyStorage(){',1)[1].split('bool SdgoodsPlatform::begin()',1)[0]
    assert all(name not in minimal for name in ('beginDisplay','beginTouch','audio_.begin','imu_.begin'))
    assert 'vTaskNotifyGiveFromISR' in platform and 'ulTaskNotifyTake' in platform
    assert 'CONFIG_FREERTOS_USE_TICKLESS_IDLE=y' in (ROOT/'firmware/sdgoods/sdkconfig.defaults').read_text()
    audio=(ROOT/'firmware/sdgoods/main/Audio.cpp').read_text()
    assert 'else ulTaskNotifyTake(pdTRUE,portMAX_DELAY)' in audio
    boot=(ROOT/'firmware/sdgoods/main/AdaptiveStandby.cpp').read_text()
    assert 'RTC_NOINIT_ATTR uint8_t retainedBytes' in boot
    assert 'ESP_RST_DEEPSLEEP' in boot
    print('standby wiring: early dark boot, RTC reset guard, tickless/GPIO wake, blocking audio OK')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--external',action='store_true');a=p.parse_args()
    check_wiring()
    out=a.output.resolve();out.mkdir(exist_ok=True,parents=True)
    resource=ROOT/'data'
    if a.external:
        from build_sdgoods_resources import stage
        resource=out/'resource-stage';stage(resource)
    binary=build(out,external=a.external,sanitize=a.sanitize,extra_flags=['-DSTICKMON_SDGOODS_ADAPTIVE_STANDBY=1'],host_source=ROOT/'tools/sdgoods_standby_host.cpp')
    subprocess.run([str(binary),str(resource)],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()
