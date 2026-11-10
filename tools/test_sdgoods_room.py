#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the round room/menu/food UI and persistence in separate processes."""
import argparse
import hashlib
import shutil
import struct
import subprocess
import sys
import os
from pathlib import Path

from build_web import ROOT, sources_for, ui_flags_for
from pack_sdgoods_resources import pack


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=ROOT / '.stick-local/artifacts/sdgoods-room-proof-20261025')
    parser.add_argument('--journey', choices=('onboard-fresh', 'learn-move', 'bag-flow', 'shop-tabs', 'battery-hud', 'audio-resources', 'settings-audio', 'locked-standby'), help='Run one focused interaction journey')
    parser.add_argument('--debug', action='store_true',
                        help='Check the debug menu and exploration with debug/trace enabled')
    parser.add_argument('--external-content',action='store_true',
                        help='Exercise the actual external catalog and directory resource backend')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if args.external_content:
        from build_sdgoods_resources import stage
        content_root=output/'resource-stage'
        stage(content_root)
        os.environ['STICKMON_CONTENT_ROOT']=str(content_root)
    archive = output / 'resources.bin'
    pack(ROOT / 'data', archive)
    pack(ROOT / 'data', output / 'audio-resources.bin', audio_only=True)
    replaced = {'HomeUi.cpp', 'MainMenuUi.cpp', 'FoodUi.cpp', 'MenuOverlayUi.cpp', 'SocialUi.cpp', 'HatchUi.cpp', 'ShopUi.cpp', 'BagUi.cpp', 'ComputerUi.cpp', 'TeamUi.cpp', 'RoomUi.cpp', 'ContactsUi.cpp', 'SettingsUi.cpp', 'StatusUi.cpp', 'MovesUi.cpp', 'DebugUi.cpp', 'ProgressionUi.cpp'}
    sources = [p for p in sources_for('ostb') if '/platform/web/' not in str(p)
               and not (p.parent.name == 'compact_touch' and p.name in replaced)]
    sources += sorted((ROOT / 'src/ui/round_touch').glob('*.cpp'))
    sources += [ROOT / 'firmware/sdgoods/main/RoomSession.cpp',
                ROOT / 'src/platform/desktop/DesktopPlatform.cpp',
                ROOT / 'tools/sdgoods_room_host.cpp']
    flags = [f for f in ui_flags_for('ostb')
             if not any(k in f for k in ('DISPLAY_', 'HEADER_'))]
    flags += ['-DSTICKMON_DISPLAY_WIDTH=360', '-DSTICKMON_DISPLAY_HEIGHT=360',
              '-DSTICKMON_UI_ROUND_TOUCH=1', '-DSTICKMON_UI_HEADER_INSET_X=60',
              '-DSTICKMON_UI_HEADER_TOP=32', '-DSTICKMON_ROOM_PIXEL_SCALE=1',
              '-DSTICKMON_DEDICATED_POWER_BUTTON=1',
              '-DSTICKMON_DEFAULT_AUDIO_VOLUME=5', '-DSTICKMON_LOCKED_UPDATE_MS=1000',
              f'-DSTICKMON_ENABLE_DEBUG_FEATURES={int(args.debug)}',
              f'-DSTICKMON_ENABLE_TRACE_LOGS={int(args.debug)}', '-DSTICKMON_HAS_CLAW=0']
    flags += [f'-DSTICKMON_EXTERNAL_CONTENT={int(args.external_content)}']
    objects = []
    for source in sources:
        if source.suffix == '.c':
            obj = output / (source.stem + '.o')
            subprocess.run(['cc', '-O1', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined', '-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
    binary = output / 'sdgoods-room-host'
    subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-Isrc', '-Ifirmware/sdgoods/main', *flags,
                    *[str(p) for p in sources if p.suffix == '.cpp'], *objects,
                    '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
    blobs = output / 'blobs'
    # Keep all previous runs; select a fresh store instead of deleting evidence.
    attempt = 0
    while blobs.exists() or any(output.glob(f'{blobs.name}-*')):
        attempt += 1
        blobs = output / f'blobs-{attempt}'
    run = lambda mode, store: subprocess.run(
        [str(binary), mode, str(output), str(archive), str(store)], check=True)
    if args.external_content:
        # A valid baseline must pass before any corruption cases: otherwise
        # unrelated failures can make all negative fixtures falsely pass.
        run('catalog-valid', output/f'{blobs.name}-catalog-valid')
        for case,ident in (('missing-coins',46),('short-coins',46),('coin-count',46),
                           ('species-type',1),('boss-level',36)):
            path=content_root/f'packs/dev/catalog/{ident}.smondata'
            original=path.read_bytes();bad=bytearray(original)
            try:
                if case=='missing-coins':path.unlink()
                else:
                    if case=='short-coins':bad=bad[:-1]
                    elif case=='coin-count':struct.pack_into('<I',bad,8,5)
                    elif case=='species-type':bad[20+struct.unpack_from('<H',bad,18)[0]]=99
                    elif case=='boss-level':bad[24]=255
                    path.write_bytes(bad)
                run('catalog-invalid',output/f'{blobs.name}-{case}')
            finally:path.write_bytes(original)
        print('PASS missing/truncated/count-invalid coin ranges and semantic species/boss rejection')
    if args.journey:
        run(args.journey, output / f'{blobs.name}-{args.journey}')
        return
    if args.debug:
        run('debug-menu', output / f'{blobs.name}-debug-menu')
        run('explore-round', output / f'{blobs.name}-explore-round')
        print('PASS debug menu navigation and native exploration with debug/trace enabled')
        return
    if args.external_content:
        for mode in ('fonts','surface','menu-fixed','shop-tabs','bag-flow','status-round','team-moves',
                     'settings-header','settings-audio','contacts-header','explore-select','explore-round','battle-round',
                     'home-notice','hud-pair','battery-hud'):
            run(mode,output/f'{blobs.name}-{mode}')
        for mode in ('onboard-fresh','onboard-resume','onboard-restored','onboard-explore'):
            run(mode,output/f'{blobs.name}-onboarding')
        run('fresh',blobs);run('restore',blobs)
        print('PASS external directory resources/catalog, complete round journeys and persisted save')
        return
    run('fonts', output / f'{blobs.name}-fonts')
    # Each malformed optional subset is loaded in a fresh process. Existing
    # 1bpp fonts must remain available for the visible fallback.
    original = archive.read_bytes()
    entries = []
    for i in range(struct.unpack_from('<I', original, 4)[0]):
        pos = 8 + i * 200
        name, offset, length = struct.unpack_from('<192sII', original, pos)
        if b'/round-zh' in name:
            entries.append((pos, offset, length))
    assert len(entries) == 2
    for case in ('version', 'flags', 'count', 'short', 'duplicate', 'codepoint'):
        content = bytearray(original)
        for pos, offset, length in entries:
            if case == 'version':
                struct.pack_into('<H', content, offset + 4, 9)
            elif case == 'flags':
                struct.pack_into('<I', content, offset + 12, 1)
            elif case == 'count':
                struct.pack_into('<H', content, offset + 6, 65535)
            elif case == 'short':
                struct.pack_into('<I', content, pos + 196, 8)
            elif case == 'duplicate':
                stride = 4 + content[offset + 10]
                content[offset + 16 + stride:offset + 20 + stride] = content[offset + 16:offset + 20]
            else:
                struct.pack_into('<I', content, offset + 16, 0xD800)
        invalid = output / f'fonts-invalid-{case}.bin'
        invalid.write_bytes(content)
        subprocess.run([str(binary), 'fonts-invalid', str(output), str(invalid),
                        str(output / f'{blobs.name}-fonts-{case}')], check=True)
    run('surface', output / f'{blobs.name}-surface')
    run('menu-fixed', output / f'{blobs.name}-menu-fixed')
    run('shop-tabs', output / f'{blobs.name}-shop-tabs')
    run('bag-flow', output / f'{blobs.name}-bag-flow')
    run('status-round', output / f'{blobs.name}-status-round')
    run('team-moves', output / f'{blobs.name}-team-moves')
    run('settings-header', output / f'{blobs.name}-settings-header')
    run('settings-audio', output / f'{blobs.name}-settings-audio')
    run('contacts-header', output / f'{blobs.name}-contacts-header')
    run('explore-select', output / f'{blobs.name}-explore-select')
    run('explore-round', output / f'{blobs.name}-explore-round')
    run('battle-round', output / f'{blobs.name}-battle-round')
    run('learn-move', output / f'{blobs.name}-learn-move')
    run('home-notice', output / f'{blobs.name}-home-notice')
    run('hud-pair', output / f'{blobs.name}-hud-pair')
    run('battery-hud', output / f'{blobs.name}-battery-hud')
    run('audio-resources', output / f'{blobs.name}-audio-resources')
    onboarding = output / f'{blobs.name}-onboarding'
    for mode in ('onboard-fresh', 'onboard-resume', 'onboard-restored'):
        run(mode, onboarding)
    run('onboard-explore', onboarding)
    run('fresh', blobs)
    run('restore', blobs)
    for case in ('corrupt', 'newer', 'slots-only'):
        store = output / f'{blobs.name}-{case}'
        shutil.copytree(blobs, store)
        for path in store.glob('stickmon--state*'):
            if case == 'slots-only' and path.name == 'stickmon--state':
                path.unlink()
                continue
            content = path.read_bytes()
            if case in ('corrupt', 'slots-only'):
                content = b'corrupt'
            else:
                content = content[:4] + struct.pack('<H', 99) + content[6:]
            path.write_bytes(content)
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in store.iterdir()}
        run('reject', store)
        after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in store.iterdir()}
        assert before == after, f'{case} save was modified'
    hatch = output / f'{blobs.name}-unfinished-hatch'
    run('seed-hatch', hatch)
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in hatch.iterdir()}
    run('resume-hatch', hatch)
    run('reset', output / f'{blobs.name}-reset')
    print('PASS separate-process restore, corrupt/newer save protection, hatch resume and reset; evidence:', output)


if __name__ == '__main__':
    main()
