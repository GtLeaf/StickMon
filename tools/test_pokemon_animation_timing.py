#!/usr/bin/env python3
"""Original XML durations, rational boundaries, and per-actor clock lifecycle."""
import bisect
import subprocess
import tempfile
import unittest
from unittest import mock
from pathlib import Path

from pmd_expansion_sheets import MANIFEST, sheet_metadata

ROOT = Path(__file__).resolve().parents[1]


class AnimationTimingTests(unittest.TestCase):
    def test_generated_timelines_are_current(self):
        import generate_pokemon_sprites as generator
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / 'durations.inc'
            with mock.patch.object(generator, 'OUT_ANIMATION_DURATIONS', generated), \
                    mock.patch.object(generator, 'OUT_EXPANSION_PROFILES', Path(directory) / 'profiles.h'):
                generator.write_expansion_profiles()
            self.assertEqual((ROOT / 'src/game/PokemonAnimationDurations.inc').read_bytes(),
                             generated.read_bytes())

    def test_market_actor_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'market-timing'
            subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=undefined', '-Isrc',
                            'tools/market_animation_timing_host.cpp', '-o', str(binary)],
                           cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)

    def test_original_timelines_and_clock(self):
        header = ROOT / 'src/game/PokemonAnimationTiming.h'
        self.assertTrue(header.exists(), 'Original per-frame timing sampler is missing')
        source = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include "game/PokemonAnimationTiming.h"
using namespace PokemonAnimationTiming;
int main() {
    Clock c;
    assert(c.update(214, Action::Idle, 100) == 0);
    assert(c.update(214, Action::Idle, 599) == 0);
    assert(c.update(214, Action::Idle, 600) == 1);
    c.update(214, Action::Idle, 650, false);
    assert(c.elapsedMs() == 500);
    c.update(214, Action::Idle, 90000, false);
    assert(c.update(214, Action::Idle, 100000) == 1);
    assert(c.elapsedMs() == 500);
    assert(c.update(214, Action::Idle, 100134) == 2);
    assert(c.update(214, Action::Walk, 100200) == 0);
    assert(c.update(214, Action::Walk, 100334) == 1);
    assert(c.update(264, Action::Walk, 100335) == 0);
    c.reset();
    c.update(214, Action::Idle, UINT32_MAX - 250);
    assert(c.update(214, Action::Idle, 249) == 1);
    assert(c.elapsedMs() == 500);
    assert(!hasTiming(133));
    assert(frameAt(133, Action::Walk, 1000, 7) == 7);
    assert(frameAt(214, static_cast<Action>(255), 1000, 7) == 7);
    unsigned id, action;
    uint64_t ms;
    while (std::cin >> id >> action >> ms)
        std::cout << unsigned(frameAt(id, static_cast<Action>(action), ms, 255)) << '\n';
}
'''
        queries, expected = [], []
        for species in MANIFEST['speciesIds'] + MANIFEST.get('retimedSpeciesIds', []):
            sheets, _ = sheet_metadata(species)
            for action, name in enumerate(('idle', 'walking', 'sleeping')):
                ends, total = [], 0
                for duration in sheets[name].durations:
                    total += duration
                    ends.append(total * 1000)
                times = set(range((total * 2000 + 59) // 60))
                times.update((10**9, 2**32 - 1, 2**64 - 1))
                for tick in [0] + [x // 1000 for x in ends]:
                    boundary = (tick * 1000 + 59) // 60
                    times.update((max(0, boundary - 1), boundary, boundary + 1))
                for ms in sorted(times):
                    queries.append(f'{species} {action} {ms}')
                    expected.append(bisect.bisect_right(ends, (ms * 60) % (total * 1000)))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'timing.cpp').write_text(source)
            subprocess.run(['c++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=undefined', '-Isrc', str(path / 'timing.cpp'),
                            '-o', str(path / 'timing')], cwd=ROOT, check=True)
            result = subprocess.run([str(path / 'timing')], input='\n'.join(queries),
                                    text=True, capture_output=True, check=True)
        self.assertEqual(expected, [int(x) for x in result.stdout.split()])


if __name__ == '__main__':
    unittest.main()
