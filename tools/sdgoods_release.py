#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build, inspect and install SDGOODS firmware with one shared resource image.

All scratch files belong under .stick-local. Install preserves the original NVS
and PHY sectors and never performs a chip erase. The old paired USB update
protocol is disabled for the shared resource layout.
"""
import argparse
import base64
import binascii
import csv
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = 'sdgoods-16m-single-v1'
APP_SIZE, BANK_SIZE = 0x300000, 0x900000
LAYOUT_END = 0x1000000
RETAINED_RANGES = ((0x9000,0xf000), (0x11000,0x20000),
                   (0xf20000,0xfe0000), (0x1e20000,0x1fe0000))
PUBLIC_KEY = ROOT / 'firmware/amoled_1_8_v2/ota/public-key.pem'
EXPECTED = {
    'nvs': (1,2,0x9000,0x6000), 'otadata': (1,0,0xf000,0x2000),
    'phy_init': (1,1,0x11000,0x1000), 'ota_0': (0,16,0x20000,APP_SIZE),
    'ota_1': (0,17,0x320000,APP_SIZE), 'resources': (1,0x83,0x620000,BANK_SIZE),
    'userdata': (1,0x83,0xf20000,0xc0000),
    'resmeta': (1,2,0xfe0000,0x10000), 'coredump': (1,3,0xff0000,0x10000),
}
# Accepted only as an installed source layout; no new 15 MiB packages.
PREVIOUS_15M = dict(EXPECTED, resources=(1,0x83,0x620000,0x800000),
    userdata=(1,0x83,0xe20000,0xc0000), resmeta=(1,2,0xee0000,0x10000),
    coredump=(1,3,0xef0000,0x10000))
PREVIOUS_SINGLE_24M = dict(EXPECTED, resources=(1,0x83,0x620000,0x1800000),
    userdata=(1,0x83,0x1e20000,0x1c0000), resmeta=(1,2,0x1fe0000,0x10000),
    coredump=(1,3,0x1ff0000,0x10000))
LAYOUTS = {LAYOUT: EXPECTED, 'sdgoods-32m-single-v1': PREVIOUS_SINGLE_24M}
PREVIOUS_AB = {
    'nvs': (1,2,0x9000,0x6000), 'otadata': (1,0,0xf000,0x2000),
    'phy_init': (1,1,0x11000,0x1000), 'ota_0': (0,16,0x20000,APP_SIZE),
    'ota_1': (0,17,0x320000,APP_SIZE), 'res_a': (1,0x83,0x620000,0xc00000),
    'res_b': (1,0x83,0x1220000,0xc00000), 'userdata': (1,0x83,0x1e20000,0x1c0000),
    'resmeta': (1,2,0x1fe0000,0x10000), 'coredump': (1,3,0x1ff0000,0x10000),
}
LEGACY = {k: v for k,v in EXPECTED.items() if k in ('nvs','otadata','phy_init','ota_0','ota_1')}
LEGACY.update(resources=(1,0x82,0x620000,0x400000), coredump=(1,3,0xa20000,0x10000),
              brainfs=(1,0x81,0xa30000,0x5d0000))

def table_layout(table):
    for layout,expected in LAYOUTS.items():
        if table==expected:return layout
    raise ValueError('unexpected partition table')

def save_schema():
    """Release metadata tracks SaveCodec::SCHEMA_VERSION checked by the device."""
    text=(Path(__file__).resolve().parents[1]/'src/core/SaveCodec.h').read_text()
    return int(re.search(r'SCHEMA_VERSION\s*=\s*(\d+)',text).group(1))

def sha(data):
    return hashlib.sha256(data).hexdigest()

def canonical(value):
    return json.dumps(value,sort_keys=True,separators=(',',':')).encode()

def partitions(data):
    result = {}
    for at in range(0,min(len(data),0xc00),32):
        record=data[at:at+32]
        if len(record)!=32: raise ValueError('short partition table')
        magic=struct.unpack_from('<H',record)[0]
        if magic==0xebeb:
            if record[16:]!=hashlib.md5(data[:at]).digest(): raise ValueError('partition table MD5')
            return result
        if magic!=0x50aa: raise ValueError('missing partition table digest or bad record')
        _,typ,sub,offset,size,label,flags=struct.unpack('<HBBII16sI',record)
        name=label.split(b'\0')[0].decode('ascii')
        if name in result or flags or not size or offset+size>0x2000000: raise ValueError('partition geometry/flags')
        result[name]=(typ,sub,offset,size)
    raise ValueError('partition table too large')

def binding(slot, payload, sequence=1):
    data=struct.pack('<IHBBIII',0x42504d53,1,slot,1,sequence,payload['app_size'],payload['resource_size'])
    data+=b''.join(bytes.fromhex(payload[k]) for k in ('app_sha256','resource_sha256','index_sha256'))
    return data+struct.pack('<I',binascii.crc32(data)&0xffffffff)

def ota_data(slot=0):
    seq=slot+1
    # ESP-IDF otatool.py uses crc32(sequence, 0xffffffff), with no final XOR.
    record=struct.pack('<I20sII',seq,b'\xff'*20,2,binascii.crc32(struct.pack('<I',seq),0xffffffff)&0xffffffff)
    return record+b'\xff'*(0x2000-len(record))

def idf_paths():
    path=Path(os.environ.get('IDF_PATH',Path.home()/'.espressif/v5.5.4/esp-idf'))
    python=Path(os.environ.get('IDF_PYTHON_ENV_PATH',Path.home()/'.espressif/python_env/idf5.5_py3.11_env'))/'bin/python'
    if not path.is_dir() or not python.exists(): raise ValueError('Set IDF_PATH and IDF_PYTHON_ENV_PATH to ESP-IDF 5.5')
    return path,python

def package(build,output,private_key=None,development=False,public_key=PUBLIC_KEY):
    build,output=build.resolve(),output.resolve()
    if development==bool(private_key): raise ValueError('choose --development or --private-key')
    config=(build/'config/sdkconfig.h').read_text()
    for required in ('#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1','#define CONFIG_ESPTOOLPY_FLASHSIZE_32MB 1'):
        if required not in config: raise ValueError('incompatible build configuration')
    debug='#define STICKMON_ENABLE_DEBUG_FEATURES 1' in config
    # This project option is a CMake compile definition, not an sdkconfig symbol.
    compile_db=(build/'compile_commands.json').read_text()
    debug=bool(re.search(r'STICKMON_ENABLE_DEBUG_FEATURES=1\b',compile_db))
    if development and not debug: raise ValueError('unsigned development bundle requires a Debug application')
    if 'STICKMON_EXTERNAL_CONTENT=1' not in compile_db: raise ValueError('application does not use external content')
    table=(build/'partition_table/partition-table.bin').read_bytes()
    layout=table_layout(partitions(table))
    bank_size=LAYOUTS[layout]['resources'][3]
    app=(build/'stickmon_sdgoods.bin').read_bytes(); resources=(build/'resources.bin').read_bytes()
    if len(app)>APP_SIZE or not app or app[0]!=0xe9 or len(resources)!=bank_size: raise ValueError('image size/header')
    stage=build/'resource-stage'
    bank=json.loads((stage/'bank-manifest.json').read_text())
    if bank['layout_id']!=layout or bank['bank_size']!=bank_size or bank['content_abi']!=1: raise ValueError('resource bank ABI')
    index=(stage/'files.idx').read_bytes()
    if sha(index)!=bank['index_sha256']: raise ValueError('resource index digest')
    # Inspect the real filesystem image, not just the staging directory.
    fs_python=build/'littlefs_py_venv/bin/python'
    with tempfile.TemporaryDirectory(dir=output.parent if output.parent.exists() else build) as tmp:
        extracted=Path(tmp)/'unpacked'
        subprocess.run([str(fs_python),'-m','littlefs','extract','--block-size','4096',str(build/'resources.bin'),str(extracted)],check=True,capture_output=True)
        staged={p.relative_to(stage).as_posix():sha(p.read_bytes()) for p in stage.rglob('*') if p.is_file()}
        actual={p.relative_to(extracted).as_posix():sha(p.read_bytes()) for p in extracted.rglob('*') if p.is_file()}
        if actual!=staged:raise ValueError('LittleFS image differs from staged resources; rebuild before packaging')
        from validate_sdgoods_catalog import validate
        validate(extracted)
    trusted_header=(build/'esp-idf/main/UpdateKey.h').read_text()
    if public_key.read_text().strip() not in trusted_header:raise ValueError('packaging key differs from application trust key')
    payload={'target':'sdgoods','layout_id':layout,'content_abi':1,'save_schema':save_schema(),
             'app_size':len(app),'resource_size':len(resources),'app_sha256':sha(app),
             'resource_sha256':sha(resources),'index_sha256':sha(index),
             'bootloader_sha256':sha((build/'bootloader/bootloader.bin').read_bytes()),
             'partition_table_sha256':sha(table)}
    output.mkdir(parents=True,exist_ok=True)
    (output/'app.bin').write_bytes(app); (output/'resources.bin').write_bytes(resources)
    shutil.copyfile(build/'bootloader/bootloader.bin',output/'bootloader.bin')
    (output/'partition-table.bin').write_bytes(table); (output/'otadata.bin').write_bytes(ota_data())
    (output/'release.json').write_bytes(canonical(payload)+b'\n')
    raw=canonical(payload); signature=b''
    if private_key:
        signed=subprocess.run(['openssl','dgst','-sha256','-sign',str(private_key)],input=raw,check=True,capture_output=True)
        signature=signed.stdout
        with tempfile.TemporaryDirectory(dir=output) as tmp:
            sig=Path(tmp)/'sig'; sig.write_bytes(signature)
            subprocess.run(['openssl','dgst','-sha256','-verify',str(public_key),'-signature',str(sig)],input=raw,check=True,capture_output=True)
    envelope={'payload':base64.b64encode(raw).decode(),'signature':base64.b64encode(signature).decode()}
    if development:envelope['development']=True
    (output/'envelope.json').write_bytes(canonical(envelope)+b'\n')
    rows=[['key','type','encoding','value'],['pairs','namespace','','']]
    for slot in range(2):
        for copy in range(2): rows.append([('b' if slot else 'a')+str(copy),'data','hex2bin',binding(slot,payload).hex()])
    with (output/'resmeta.csv').open('w',newline='') as f:csv.writer(f).writerows(rows)
    idf,python=idf_paths()
    subprocess.run([str(python),str(idf/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'),
                    'generate',str(output/'resmeta.csv'),str(output/'resmeta.bin'),'0x10000'],check=True)
    inventory={name:sha((output/name).read_bytes()) for name in
               ('app.bin','resources.bin','bootloader.bin','partition-table.bin','otadata.bin','resmeta.bin','envelope.json','release.json')}
    (output/'bundle.json').write_bytes(canonical({'format':1,'files':inventory})+b'\n')
    print(json.dumps({'bundle':str(output),'app_bytes':len(app),'resource_logical_bytes':sum(p.stat().st_size for p in stage.rglob('*') if p.is_file()),
                      'resource_bank_bytes':len(resources),'development':development},indent=2))
    return payload

def validate_bundle(folder,public_key=PUBLIC_KEY):
    folder=folder.resolve(); inventory=json.loads((folder/'bundle.json').read_text())
    names={'app.bin','resources.bin','bootloader.bin','partition-table.bin','otadata.bin','resmeta.bin','envelope.json','release.json'}
    if inventory['format']!=1 or set(inventory['files'])!=names: raise ValueError('bundle inventory')
    for name,digest in inventory['files'].items():
        if sha((folder/name).read_bytes())!=digest:raise ValueError('bundle changed: '+name)
    envelope=json.loads((folder/'envelope.json').read_text());raw=base64.b64decode(envelope['payload'],validate=True)
    payload=json.loads(raw)
    if payload!=json.loads((folder/'release.json').read_text()):raise ValueError('release mismatch')
    if payload['target']!='sdgoods' or payload['layout_id'] not in LAYOUTS or payload['content_abi']!=1 or payload['save_schema']!=save_schema():raise ValueError('release compatibility')
    expected=LAYOUTS[payload['layout_id']]
    bank_size=expected['resources'][3]
    if sha((folder/'app.bin').read_bytes())!=payload['app_sha256'] or (folder/'app.bin').stat().st_size!=payload['app_size'] or not 0<payload['app_size']<=APP_SIZE:raise ValueError('application digest/size')
    if sha((folder/'resources.bin').read_bytes())!=payload['resource_sha256'] or (folder/'resources.bin').stat().st_size!=bank_size or payload['resource_size']!=bank_size:raise ValueError('resource digest/size')
    if partitions((folder/'partition-table.bin').read_bytes())!=expected:raise ValueError('bundle partition table')
    if sha((folder/'bootloader.bin').read_bytes())!=payload['bootloader_sha256'] or sha((folder/'partition-table.bin').read_bytes())!=payload['partition_table_sha256']:raise ValueError('bootloader/table digest')
    if (folder/'otadata.bin').read_bytes()!=ota_data() or (folder/'resmeta.bin').stat().st_size!=0x10000:raise ValueError('initial metadata')
    sig=base64.b64decode(envelope['signature'],validate=True)
    if sig:
        with tempfile.TemporaryDirectory(dir=folder) as tmp:
            p=Path(tmp)/'sig';p.write_bytes(sig)
            subprocess.run(['openssl','dgst','-sha256','-verify',str(public_key),'-signature',str(p)],input=raw,check=True,capture_output=True)
    elif envelope.get('development') is not True:raise ValueError('unsigned release')
    return payload,envelope

def write_plan(folder):
    table=partitions((folder/'partition-table.bin').read_bytes())
    table_layout(table)
    return [(0,'bootloader.bin'),(0x8000,'partition-table.bin'),(0xf000,'otadata.bin'),
            (0x20000,'app.bin'),(0x320000,'app.bin'),(0x620000,'resources.bin'),
            (table['resmeta'][2],'resmeta.bin')]

def validate_write_plan(folder, plan):
    """Check full erase sectors before connecting, reading or writing flash."""
    table=partitions((folder/'partition-table.bin').read_bytes())
    table_layout(table)
    layout_end=max(at+size for _,_,at,size in table.values())
    userdata=table['userdata']
    retained=((0x9000,0xf000),(0x11000,0x20000),
              (userdata[2],userdata[2]+userdata[3]),(0x1e20000,0x1fe0000))
    occupied=[]
    for at,name in plan:
        size=(folder/name).stat().st_size
        low=at & ~4095
        high=(at+size+4095) & ~4095
        if at<0 or at%4096 or size<=0 or high>layout_end:
            raise ValueError(f'write exceeds {layout_end//(1024*1024)} MiB layout or is not sector aligned')
        if any(low<b and high>a for a,b in retained):
            raise ValueError('write overlaps retained data')
        if any(low<b and high>a for a,b in occupied):
            raise ValueError('write regions overlap')
        occupied.append((low,high))

def factory_image(folder, output, public_key=PUBLIC_KEY):
    """Export a contiguous image for new devices; gaps include erased NVS."""
    validate_bundle(folder,public_key)
    plan=write_plan(folder)
    validate_write_plan(folder,plan)
    table=partitions((folder/'partition-table.bin').read_bytes())
    layout_end=max(at+size for _,_,at,size in table.values())
    # Keep this separate from the sparse installer which preserves NVS.
    if output.resolve().parent==folder.resolve():
        raise ValueError('factory output must be outside the signed bundle')
    data=bytearray(b'\xff'*layout_end)
    for at,name in plan:
        segment=(folder/name).read_bytes()
        data[at:at+len(segment)]=segment
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(data)
    print(json.dumps({'factory_image':str(output.resolve()),'bytes':len(data),
                      'sha256':sha(data),'preserves_nvs':False},indent=2))

def validate_install_table(data):
    # Fresh devices have no table yet. Existing layouts must be known so a
    # first installation cannot silently overwrite another product's data.
    if data==b'\xff'*0x1000:return
    current=partitions(data)
    if current not in (EXPECTED,PREVIOUS_15M,PREVIOUS_SINGLE_24M,PREVIOUS_AB,LEGACY):raise ValueError('unknown device layout; installation refused: '+repr(current))

def run_install_command(command, label, heartbeat_seconds=5):
    """Stream esptool output, including its long compressed-block flush waits."""
    print('[install] '+label,flush=True)
    process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                             text=True,bufsize=1,env={**os.environ,'PYTHONUNBUFFERED':'1'})
    lines=queue.Queue()
    def read_output():
        try:
            for line in process.stdout:lines.put(line)
        finally:lines.put(None)
    reader=threading.Thread(target=read_output,daemon=True)
    reader.start()
    output=[]
    started=last_output=time.monotonic()
    try:
        while True:
            try:line=lines.get(timeout=heartbeat_seconds)
            except queue.Empty:
                now=time.monotonic()
                print(f'[install] {label}: waiting for esptool '
                      f'({now-started:.0f}s elapsed, {now-last_output:.0f}s without new output)',flush=True)
                continue
            if line is None:break
            output.append(line)
            last_output=time.monotonic()
            print(line,end='',flush=True)
            if 'Writing at ' in line and '(100 %)' in line:
                print('[install] 100% marks the last compressed block; '
                      'flash completion and digest verification are still pending.',flush=True)
        code=process.wait()
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        reader.join(timeout=1)
        process.stdout.close()
    combined=''.join(output)
    if code:raise subprocess.CalledProcessError(code,command,combined)
    return combined

def install(folder,port,mac,execute=False,public_key=PUBLIC_KEY):
    validate_bundle(folder,public_key)
    plan=write_plan(folder)
    validate_write_plan(folder,plan)
    print(json.dumps({'writes':[{'offset':hex(at),'file':name,'size':(folder/name).stat().st_size} for at,name in plan],
                      'preserve':['nvs','phy_init','userdata'],'execute':execute},indent=2))
    if not execute:return
    if not port or not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}',mac or ''):raise ValueError('--port and explicit --mac required')
    _,python=idf_paths()
    run_install_command([str(python),str(ROOT/'tools/sdgoods_flash_install.py'),
                         '--bundle',str(folder.resolve()),'--port',port,'--mac',mac,
                         '--public-key',str(public_key.resolve())],
                        'Checking and installing changed regions')

def serial_connection(port):
    import serial
    connection=serial.Serial(port=None,baudrate=921600,timeout=0.2,write_timeout=15)
    connection.dtr=False;connection.rts=False;connection.port=port;connection.open()
    return connection

def response(connection,prefix,timeout=90):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        line=connection.readline().decode('utf-8',errors='replace').strip()
        if line.startswith('SMON ERROR'):raise ValueError(line)
        if line.startswith(prefix):return line
        if line and not line.startswith('SMON'):print(line,flush=True)
    raise TimeoutError('device did not respond: '+prefix)

def hello(connection,timeout=45):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        connection.write(b'SMON-HELLO\n')
        try:return response(connection,'SMON STATUS',timeout=min(3,max(0,deadline-time.monotonic())))
        except TimeoutError:continue
    raise TimeoutError('application did not start')

def update(folder,port,public_key=PUBLIC_KEY):
    raise ValueError('USB in-app update is disabled for the shared resource layout; use install with the complete ROM bundle')

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    pack=sub.add_parser('package');pack.add_argument('--build',type=Path,required=True);pack.add_argument('--output',type=Path,required=True)
    sign=pack.add_mutually_exclusive_group(required=True);sign.add_argument('--development',action='store_true');sign.add_argument('--private-key',type=Path)
    for command in ('inspect','install','update','factory'):
        c=sub.add_parser(command);c.add_argument('--bundle',type=Path,required=True);c.add_argument('--public-key',type=Path,default=PUBLIC_KEY)
        if command in ('install','update'):c.add_argument('--port',required=command=='update')
        if command=='install':c.add_argument('--mac');c.add_argument('--execute',action='store_true')
        if command=='factory':c.add_argument('--output',type=Path,required=True)
    pack.add_argument('--public-key',type=Path,default=PUBLIC_KEY)
    a=p.parse_args()
    if a.action=='package':package(a.build,a.output,a.private_key,a.development,a.public_key)
    elif a.action=='install':install(a.bundle,a.port,a.mac,a.execute,a.public_key)
    elif a.action=='update':update(a.bundle,a.port,a.public_key)
    elif a.action=='factory':factory_image(a.bundle,a.output,a.public_key)
    else:print(json.dumps(validate_bundle(a.bundle,a.public_key)[0],indent=2))

if __name__=='__main__':main()
