#!/usr/bin/env python3
"""Sign a V2 application-only release. Never distributes the private key."""
import argparse
import base64
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--key', required=True, type=Path)
args = parser.parse_args()
build = args.build_dir.resolve()
meta = json.loads((build / 'ota-generated/ota-build.json').read_text())
if meta.get('debug'):
    raise SystemExit('Debug images cannot be signed for OTA')
config = (build / 'config/sdkconfig.h').read_text()
if '#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1' not in config:
    raise SystemExit('Refusing OTA release without rollback-enabled bootloader')
binary = (build / 'stickmon_amoled_1_8_v2.bin').read_bytes()
if not 0 < len(binary) <= 0x300000 or binary[0] != 0xe9:
    raise SystemExit('Invalid or oversized ESP image')
meta.update(size=len(binary), sha256=hashlib.sha256(binary).hexdigest())
payload = json.dumps(meta, separators=(',', ':'), ensure_ascii=True).encode()
with tempfile.TemporaryDirectory() as tmp:
    source, signature = Path(tmp) / 'payload', Path(tmp) / 'signature'
    source.write_bytes(payload)
    subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(args.key),
                    '-out', str(signature), str(source)], check=True)
    subprocess.run(['openssl', 'dgst', '-sha256', '-verify',
                    str(Path(__file__).resolve().parents[1] /
                        'firmware/amoled_1_8_v2/ota/public-key.pem'),
                    '-signature', str(signature), str(source)], check=True)
    envelope = dict(payload=base64.b64encode(payload).decode(),
                    signature=base64.b64encode(signature.read_bytes()).decode())
dest = build / 'ota-release.json'
dest.write_text(json.dumps(envelope, indent=2) + '\n')
if (build / 'out').is_dir():
    (build / 'out/ota-release.json').write_text(dest.read_text())
print(f'Signed {meta["flavor"]} {meta["version"]} build {meta["build"]}: {dest}')
