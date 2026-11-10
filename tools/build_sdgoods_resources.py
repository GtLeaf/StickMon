#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Stage one complete, deterministic SDGOODS LittleFS bank (no serial I/O)."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
from generate_sdgoods_catalog import ROOT, generate

LAYOUT='sdgoods-16m-single-v1'
ABI=1
BANK_SIZE=9*1024*1024
INDEX_ENTRY=struct.Struct('<160sI32s')
EXTENSIONS={'.json','.smonsp','.smonroom','.smonfurn','.smonfx','.smonfont',
            '.smondialogue','.smonaudio','.smoncry'}

def stage(output,layout='16m'):
    if layout not in ('16m','32m'):raise ValueError('Unknown SDGOODS resource layout')
    layout_id='sdgoods-32m-single-v1' if layout=='32m' else LAYOUT
    bank_size=24*1024*1024 if layout=='32m' else BANK_SIZE
    output=Path(output).resolve()
    if output==ROOT or ROOT not in output.parents:
        raise ValueError('Resource stage must be a dedicated directory inside this checkout')
    output.mkdir(parents=True,exist_ok=True)
    # Remove only files owned by the previous generated stage, preserving callers' directories.
    owned=output.parent/(output.name+'-generated-files.json')
    if owned.exists():
        for name in json.loads(owned.read_text()):
            p=output/name
            if not p.is_relative_to(output) or '..' in Path(name).parts: raise ValueError('Invalid prior stage path')
            if p.is_file():p.unlink()
    for p in sorted((ROOT/'data').rglob('*')):
        if not p.is_file() or p.suffix not in EXTENSIONS or p.name=='ui_amoled.smonfx':continue
        relative=p.relative_to(ROOT/'data')
        dest=output/relative;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,dest)
    generate(output/'packs/dev/catalog')
    files=sorted(p for p in output.rglob('*') if p.is_file() and p.name not in
                 ('.generated-files.json','bank-manifest.json','files.idx'))
    records=[]
    for p in files:
        name=p.relative_to(output).as_posix().encode('ascii')
        if len(name)>=160:raise ValueError(f'Path too long: {p}')
        data=p.read_bytes();records.append(INDEX_ENTRY.pack(name,len(data),hashlib.sha256(data).digest()))
    index=struct.pack('<4sHHII',b'SMFI',1,INDEX_ENTRY.size,len(records),0)+b''.join(records)
    (output/'files.idx').write_bytes(index)
    manifest={'format':'stickmon-resource-bank-v1','layout_id':layout_id,'target':'sdgoods',
              'schema':1,'content_abi':ABI,'bank_size':bank_size,'file_count':len(records),
              'index_sha256':hashlib.sha256(index).hexdigest(),'root':'/packs/dev'}
    (output/'bank-manifest.json').write_text(json.dumps(manifest,sort_keys=True,separators=(',',':'))+'\n')
    names=[p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file() and p!=owned]
    # Keep ownership list outside the image: companion file, not a resource.
    if owned.exists():owned.unlink()
    owned.write_text(json.dumps(names,indent=2)+'\n')
    payload=sum(p.stat().st_size for p in output.rglob('*') if p.is_file())
    if payload>bank_size*0.8:raise ValueError('Resource payload exceeds bank capacity budget')
    print(f'SDGOODS resource bank: {len(records)} files, {payload} logical bytes, ABI {ABI}')
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--layout',choices=('16m','32m'),default='16m')
    args=parser.parse_args()
    stage(args.output,args.layout)
