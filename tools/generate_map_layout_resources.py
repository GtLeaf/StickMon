#!/usr/bin/env python3
"""Export only existing map tables 10..20 as CRC-checked compressed SMCD v2."""
import argparse
from pathlib import Path
import struct
import tempfile
import zlib
from generate_sdgoods_catalog import ROOT, generate as export_catalog

def encode_table(raw):
    if len(raw)<16:raise ValueError('Truncated content table')
    magic,schema,key,count,size=struct.unpack_from('<IHHII',raw)
    if magic!=0x44434d53 or schema!=1 or not 10<=key<=20 or not count or size!=len(raw)-16:
        raise ValueError('Invalid source map table')
    c=zlib.compressobj(9,wbits=-15);payload=c.compress(raw[16:])+c.flush()
    return struct.pack('<IHHIIII',magic,2,key,count,size,len(payload),zlib.crc32(raw[16:]))+payload

def generate(output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        source=Path(temporary);export_catalog(source)
        total=0
        for key in range(10,21):
            encoded=encode_table((source/f'{key}.smondata').read_bytes())
            path=output/f'{key}.smondata'
            if not path.exists() or path.read_bytes()!=encoded:path.write_bytes(encoded)
            total+=len(encoded)
    print(f'Map layouts: 11 tables, {total} bytes -> {output}')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'data/packs/dev/catalog')
    generate(p.parse_args().output)
