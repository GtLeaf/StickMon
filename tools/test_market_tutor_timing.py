#!/usr/bin/env python3
"""Original XML timing through all six real tutor scripts; no SDK required."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pmd_expansion_sheets import MANIFEST, source_dir

ROOT = Path(__file__).resolve().parents[1]
SPECIES = MANIFEST['speciesIds'] + MANIFEST.get('retimedSpeciesIds', [])
# Production choreography contract in milliseconds. Only these existing loops
# run Walk; pauses, strikes and partner celebration hops hold their selected poses.
WINDOWS = (
    ((700, 1500), (2650, 3450)),
    ((600, 1150), (1350, 1900), (2600, 3150), (3400, 3950)),
    ((600, 1400), (2600, 3300)),
    ((1400, 2300), (3500, 4600)),
    ((1400, 1800), (2000, 2400), (3500, 3950), (4150, 4550)),
    ((1400, 2500), (3500, 4600)),
)
INSTRUCTOR_WINDOWS = (
    ((4100, 4801),), (), (),
    ((400, 1400), (2500, 3500)),
    ((400, 750), (900, 1250), (2500, 2850), (3000, 3350)),
    ((400, 1400), (2500, 3500)),
)


def walk_durations(species):
    xml = source_dir(species) / 'AnimData.xml'
    anims = {a.findtext('Name'): a for a in ET.parse(xml).findall('./Anims/Anim')}
    walk = anims['Walk']
    while walk.find('CopyOf') is not None:
        walk = anims[walk.findtext('CopyOf')]
    return [int(v.text) for v in walk.findall('./Durations/Duration')]


def samples():
    instructor = walk_durations(94)
    def frame_at(durations, elapsed):
        phase = (elapsed * 60) % (sum(durations) * 1000)
        total = 0
        for index, duration in enumerate(durations):
            total += duration * 1000
            if phase < total:
                return index

    def row(species, style, ms, durations, frame):
        teacher = int(style == 4 and (750 <= ms < 900 or 2850 <= ms < 3000))
        for start, stop in INSTRUCTOR_WINDOWS[style]:
            if start <= ms < stop and not (style == 0 and ms == start):
                teacher = frame_at(instructor, ms - start)
        return f'{species} {style} {ms} {len(durations)} {frame} {len(instructor)} {teacher}\n'

    for species in SPECIES:
        durations = walk_durations(species)
        for style, windows in enumerate(WINDOWS):
            end = 4800 if style < 3 else 5600
            times = set(range(0, end + 1, 50))
            for start, stop in windows + INSTRUCTOR_WINDOWS[style]:
                times.update((start - 1, start, start + 1, stop - 1, stop, stop + 1))
                # Each source frame boundary and wrap in each short script loop.
                ticks = 0
                while ticks * 1000 < (stop - start) * 60:
                    for duration in (instructor if (start, stop) in INSTRUCTOR_WINDOWS[style] else durations):
                        ticks += duration
                        boundary = start + (ticks * 1000 + 59) // 60
                        if boundary < stop:
                            times.update((boundary - 1, boundary, boundary + 1))
            for ms in sorted(time for time in times if time <= end):
                frame = int((style == 1 and (1150 <= ms < 1350 or 3150 <= ms < 3400))
                            or (style == 4 and 3950 <= ms < 4150))
                for start, stop in windows:
                    if start <= ms < stop and not (style == 0 and ms == start):
                        frame = frame_at(durations, ms - start)
                yield row(species, style, ms, durations, frame)
    # Legacy Eevee keeps script-selected modulo-3 cycling and impact holds.
    for style, ms, frame in ((0, 800, 2), (0, 2700, 0), (1, 800, 2),
                             (1, 1200, 1), (1, 1500, 0), (2, 800, 1),
                             (3, 1600, 1), (4, 1600, 1), (4, 4000, 1),
                             (5, 1600, 0)):
        yield row(133, style, ms, [1, 1, 1], frame)


def main():
    with tempfile.TemporaryDirectory(prefix='market-tutor-timing-') as directory:
        binary = Path(directory) / 'tutor-timing'
        subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                        '-Isrc', 'tools/market_tutor_timing_host.cpp',
                        'src/presentation/Canvas565.cpp', '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], input=''.join(samples()), text=True, check=True)


if __name__ == '__main__':
    main()
