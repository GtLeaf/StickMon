#!/usr/bin/env python3
"""Validate complete market scripts and emit a bounded, CRC checked resource."""
import argparse
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'origin_asset/text/market_dialogue.json'
OUTPUT = ROOT / 'data/packs/dev/text/market.smondialogue'
HEADER = struct.Struct('<4sHHHHII')
PROFILE = struct.Struct('<HHI')
SCRIPT = struct.Struct('<HBBHHHH' + 'BI' * 4)
CONTEXT = {'any': 0, 'resident': 1, 'solo': 2, 'pair': 3}
SPEAKER = {'self': 0, 'target': 1, 'other': 2, 'partner': 3}


def compile_dialogue(data):
    if data.get('schema') != 1:
        raise ValueError('Unsupported dialogue schema')
    traits = data['traits']
    if set(traits.values()) != {1, 2, 4, 8, 16, 32}:
        raise ValueError('Trait bits must match the runtime schema')
    strings, offsets = bytearray(), {}

    def string(value, limit):
        if not isinstance(value, str) or not value or len(value) > limit:
            raise ValueError('Empty or oversized dialogue text')
        if any(not ch.isprintable() for ch in value):
            raise ValueError('Control characters are not dialogue text')
        if value not in offsets:
            offsets[value] = len(strings)
            strings.extend(value.encode('utf-8') + b'\0')
        return offsets[value]

    def mask(names):
        if len(set(names)) != len(names) or any(n not in traits for n in names):
            raise ValueError('Invalid trait list')
        return sum(traits[n] for n in names)

    profiles, seen = [], set()
    for p in sorted(data['profiles'], key=lambda p: p['species']):
        sid = p['species']
        if not 1 <= sid <= 65535 or sid in seen:
            raise ValueError('Invalid or duplicate species profile')
        seen.add(sid)
        profiles.append(PROFILE.pack(sid, mask(p['traits']), string(p['name'], 12)))
    scripts, seen = [], set()
    contexts = set()
    for s in data['scripts']:
        sid, context, lines = s['id'], CONTEXT[s['context']], s['lines']
        if sid in seen or not 1 <= sid <= 65535 or not 1 <= len(lines) <= 4:
            raise ValueError('Invalid script id or line count')
        if not 0 <= s['species'] <= 65535 or not 1 <= s['weight'] <= 100:
            raise ValueError('Invalid species filter or script weight')
        seen.add(sid)
        if not s['species'] and not s['traits']:
            contexts.add(context)
        fields = [sid, context, len(lines), mask(s['traits']), s['species'], s['weight'], 0]
        for line in lines:
            role = SPEAKER[line['speaker']]
            if role == 2 and context != 3:
                raise ValueError('Only real pairs may use the other visitor')
            fields.extend((role, string(line['text'], 48)))
        fields.extend([0, 0] * (4 - len(lines)))
        scripts.append(SCRIPT.pack(*fields))
    if not {1, 2, 3} <= contexts:
        raise ValueError('Resident, solo and pair must each have an unfiltered fallback')
    if not 1 <= len(scripts) <= 64 or len(profiles) > 128 or len(strings) > 16384:
        raise ValueError('Dialogue resource exceeds runtime bounds')
    payload = b''.join(profiles + scripts) + strings
    return HEADER.pack(b'SMDL', 1, len(profiles), len(scripts), 0,
                       len(strings), zlib.crc32(payload)) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--output', type=Path, default=OUTPUT)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    blob = compile_dialogue(json.loads(args.source.read_text()))
    if args.check:
        if not args.output.exists() or args.output.read_bytes() != blob:
            raise SystemExit('Generated dialogue resource is stale')
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(blob)
    print(f'Market dialogue: {len(blob)} bytes')


if __name__ == '__main__':
    main()
