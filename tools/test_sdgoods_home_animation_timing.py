#!/usr/bin/env python3
"""Verify real round home frame selection against the pinned SpriteCollab XML."""
import argparse
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from test_sdgoods_market import ROOT, build
from pmd_expansion_sheets import MANIFEST, source_dir


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.stick-local/artifacts/legacy55-animation-20261122/callers/sd-home')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    species_ids = MANIFEST['speciesIds'] + MANIFEST.get('retimedSpeciesIds', [])
    assert len(species_ids) == len(set(species_ids))
    xmls = [source_dir(species) / 'AnimData.xml' for species in species_ids]
    for xml in sorted(xmls):
        species = int(xml.parent.name)
        animations = {a.findtext('Name'): a for a in ET.parse(xml).findall('./Anims/Anim')}
        for index, name in enumerate(('Idle', 'Walk', 'Sleep')):
            animation = animations[name]
            seen = set()
            while animation.findtext('CopyOf'):
                target = animation.findtext('CopyOf')
                assert target not in seen
                seen.add(target)
                animation = animations[target]
            durations = [int(d.text) for d in animation.findall('./Durations/Duration')]
            assert durations and all(d > 0 for d in durations)
            rows.append(' '.join(map(str, (species, index, len(durations), *durations))))
    assert len(rows) == len(species_ids) * 3
    fixture = output / 'source-durations.txt'
    fixture.write_text('\n'.join(rows) + '\n')
    binary = build(output, debug=False, sanitize=args.sanitize,
                   host_source=ROOT / 'tools/sdgoods_home_animation_timing_host.cpp')
    run = subprocess.run([str(binary), str(ROOT / 'data'), str(fixture)], cwd=ROOT,
                         text=True, capture_output=True, env={**os.environ,
                         'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                         'UBSAN_OPTIONS': 'halt_on_error=1'})
    (output / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end='')
    run.check_returncode()


if __name__ == '__main__':
    main()
