#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate built LittleFS, bundle signatures, flash ranges and USB framing."""
import argparse
import base64
import binascii
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import sdgoods_release as release


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True)
    p.add_argument('--output',type=Path)
    args=p.parse_args()
    out=args.output or release.ROOT/'.stick-local/artifacts/sdgoods-release-tests'/args.build.name
    out.mkdir(parents=True,exist_ok=True)
    bundle=out/'bundle'
    key=release.ROOT/'.ota/release-key.pem'
    if key.exists(): release.package(args.build,bundle,private_key=key)
    else: release.package(args.build,bundle,development=True)
    meta,envelope=release.validate_bundle(bundle)
    table=release.partitions((bundle/'partition-table.bin').read_bytes())
    layout_end=max(at+size for _,_,at,size in table.values())
    factory=out/f'factory-{layout_end//(1024*1024)}m.bin'
    release.factory_image(bundle,factory)
    image=factory.read_bytes()
    assert len(image)==layout_end
    if meta['layout_id']==release.LAYOUT:assert len(image)==16*1024*1024
    for at,name in release.write_plan(bundle):
        segment=(bundle/name).read_bytes()
        assert image[at:at+len(segment)]==segment
    for name in ('nvs','phy_init','userdata','coredump'):
        _,_,low,size=table[name]
        assert image[low:low+size]==b'\xff'*size
    release.install(bundle,None,None,False)
    release.validate_install_table(b'\xff'*0x1000)
    release.validate_install_table((bundle/'partition-table.bin').read_bytes())
    for unknown in (b'',b'\xff'*0x800,b'\x00'*0x1000):
        try:release.validate_install_table(unknown)
        except ValueError:pass
        else:raise AssertionError('accepted unknown/short initial partition table')
    # Cross-check initial boot selection with the algorithm in IDF's otatool.
    ota=(bundle/'otadata.bin').read_bytes();seq=struct.unpack_from('<I',ota)[0]
    assert seq==1 and struct.unpack_from('<I',ota,24)[0]==2
    assert struct.unpack_from('<I',ota,28)[0]==binascii.crc32(struct.pack('<I',seq),0xffffffff)&0xffffffff
    with tempfile.TemporaryDirectory(dir=out) as temporary:
        tmp=Path(temporary);private=tmp/'private.pem';public=tmp/'public.pem';signature=tmp/'signature'
        subprocess.run(['openssl','ecparam','-name','prime256v1','-genkey','-noout','-out',str(private)],check=True,capture_output=True)
        subprocess.run(['openssl','pkey','-in',str(private),'-pubout','-out',str(public)],check=True,capture_output=True)
        raw=base64.b64decode(envelope['payload'])
        signature.write_bytes(subprocess.run(['openssl','dgst','-sha256','-sign',str(private)],input=raw,check=True,capture_output=True).stdout)
        signed={'payload':envelope['payload'],'signature':base64.b64encode(signature.read_bytes()).decode()}
        def replace_envelope(value):
            (bundle/'envelope.json').write_bytes(release.canonical(value)+b'\n')
            inventory=json.loads((bundle/'bundle.json').read_text());inventory['files']['envelope.json']=release.sha((bundle/'envelope.json').read_bytes())
            (bundle/'bundle.json').write_bytes(release.canonical(inventory))
        replace_envelope(signed);release.validate_bundle(bundle,public)
        for case in ('signature','payload','unsigned'):
            bad=dict(signed)
            if case=='signature':bad['signature']=base64.b64encode(b'wrong').decode()
            elif case=='payload':bad['payload']=base64.b64encode(raw+b' ').decode()
            else:bad['signature']=''
            replace_envelope(bad)
            try:release.validate_bundle(bundle,public)
            except (ValueError,subprocess.CalledProcessError):pass
            else:raise AssertionError('accepted '+case)
        replace_envelope(envelope)
    app=bundle/'app.bin';original=app.read_bytes();app.write_bytes(original[:-1])
    try:release.validate_bundle(bundle)
    except ValueError:pass
    else:raise AssertionError('accepted truncated app')
    factory.unlink()
    try:release.factory_image(bundle,factory)
    except ValueError:pass
    else:raise AssertionError('exported tampered factory image')
    assert not factory.exists()
    app.write_bytes(original)
    # A self-consistent filesystem/index can still contain invalid references.
    # Rebuild such an image and require package() to stop before signing/output.
    with tempfile.TemporaryDirectory(dir=out) as temporary:
        badbuild=Path(temporary)/'build';badbuild.mkdir()
        for name in ('config/sdkconfig.h','compile_commands.json','partition_table/partition-table.bin',
                     'stickmon_sdgoods.bin','bootloader/bootloader.bin','esp-idf/main/UpdateKey.h',
                     'littlefs_py_venv'):
            target=badbuild/name;target.parent.mkdir(parents=True,exist_ok=True)
            target.symlink_to((args.build/name).resolve())
        stage=badbuild/'resource-stage';shutil.copytree(args.build/'resource-stage',stage)
        tutor=stage/'packs/dev/catalog/54.smondata'
        bad=bytearray(tutor.read_bytes());struct.pack_into('<H',bad,16,168);tutor.write_bytes(bad)
        idx=bytearray((stage/'files.idx').read_bytes())
        for at in range(16,len(idx),196):
            if idx[at:at+160].split(b'\0')[0]==b'packs/dev/catalog/54.smondata':
                idx[at+164:at+196]=hashlib.sha256(bad).digest()
        (stage/'files.idx').write_bytes(idx)
        manifest=json.loads((stage/'bank-manifest.json').read_text())
        manifest['index_sha256']=release.sha(idx)
        (stage/'bank-manifest.json').write_text(json.dumps(manifest)+'\n')
        subprocess.run([str(badbuild/'littlefs_py_venv/bin/python'),'-m','littlefs','create',
                        str(stage),str(badbuild/'resources.bin'),'--block-size','4096',
                        '--name-max','64','--fs-size',str(meta['resource_size'])],check=True,capture_output=True)
        refused=Path(temporary)/'must-not-publish'
        try:release.package(badbuild,refused,private_key=key if key.exists() else None,development=not key.exists())
        except subprocess.CalledProcessError as error:assert error.returncode==1
        else:raise AssertionError('packaged a dangling tutor reference with consistent hashes')
        assert not refused.exists()
    class Connection:
        def __init__(self,fail=False):self.lines=[];self.files=[bytearray()];self.file=0;self.done=False;self.fail=fail
        def __enter__(self):return self
        def __exit__(self,*args):return False
        def write(self,data):
            if data==b'SMON-HELLO\n':self.lines.append(f'SMON STATUS slot={int(self.done)} layout=1 ready=1\n'.encode());return
            if data.startswith(b'SMON-UPDATE '):self.lines.append(b'SMON ERROR shared-resource-update-disabled\n');return
            if data==b'DONE':
                assert False, 'shared resource layout must not accept in-app update'
                self.done=True;self.lines.append(b'SMON COMMITTED\n');return
            at,n,crc=struct.unpack_from('<III',data);payload=data[12:]
            assert at==len(self.files[self.file]) and n==len(payload) and n<=4096 and binascii.crc32(payload)&0xffffffff==crc
            self.files[self.file]+=payload;offset=at+n
            self.lines.append(f'SMON ACK {offset+int(self.fail)}\n'.encode())
            if self.file==0 and offset==release.BANK_SIZE:self.file=1
        def readline(self):assert self.lines;return self.lines.pop(0)
    real=release.serial_connection
    try:
        for connection in (Connection(), Connection(True)):
            try:release.update(bundle,'mock')
            except ValueError as error:
                assert 'shared resource layout' in str(error)
            else:raise AssertionError('accepted disabled in-app update')
            assert not connection.done
    finally:release.serial_connection=real
    print(f'PASS actual LittleFS/stage equivalence, signatures/tampering, {layout_end//(1024*1024)} MiB factory image/segments/gaps, flash preservation, OTA seed and shared-resource update guard')

if __name__=='__main__':main()
