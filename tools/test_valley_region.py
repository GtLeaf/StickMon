"""Check compiled regions, seed replay, native art and complete journeys."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from amoled_host import ROOT, build_amoled_host
from valley_region import ASSETS, FORESTS, OLD_CAVES, BRIDGES, cpp_table, room_for
from generate_runtime_tile_map import generate_valley_region, fingerprint


class ValleyRegionTests(unittest.TestCase):
    def test_generated_table_and_style_counts(self):
        self.assertEqual((ROOT / 'src/game/ExploreValleyData.h').read_text(), cpp_table())
        self.assertEqual((len(FORESTS), len(FORESTS[0]), len(OLD_CAVES), len(BRIDGES)),
                         (2, 6, 4, 16))
        self.assertEqual(len({tuple(tuple(layer) for room in group for layer in room['layers'])
                              for group in BRIDGES}), 16)

    def test_reciprocal_portals_and_reachable_forests(self):
        for seed in (38, 91, 262):
            seen, pending = set(), [0]
            while pending:
                node = pending.pop()
                if node in seen:
                    continue
                seen.add(node)
                for index, port in enumerate(room_for(seed, node)['portals']):
                    target = port['target']
                    if target == 255:
                        continue
                    other = room_for(seed, target)['portals'][port['entrance']]
                    # A's two elevations share a physical room and portal list.
                    self.assertEqual(other['target'], 10 if node == 13 else node)
                    self.assertEqual(other['entrance'], index)
                    if target < 6:
                        lane = 0 if port['edge'] % 2 == 0 else 1
                        self.assertEqual(port['point'][lane], other['point'][lane])
                        self.assertEqual((port['edge'] + 2) % 4, other['edge'])
                        pending.append(target)
            self.assertEqual(seen, set(range(6)))
            mouths = [(n, p) for n in range(6) for p in room_for(seed, n)['portals']
                      if 6 <= p['target'] < 14]
            self.assertEqual([n for n, _ in mouths], [4, 5])

    def test_runtime_routes_replay_and_elevation(self):
        for seed in (*range(32), *range(256, 288), 0, 0xFFFFFFFF):
            for node in range(14):
                room = room_for(seed, node)
                entries = range(2, 4) if node == 13 else range(2) if node == 10 else range(len(room['portals']))
                for entry in entries:
                    m = generate_valley_region(seed, node, entry)
                    self.assertEqual(m, generate_valley_region(seed, node, entry))
                    self.assertTrue(1 <= len(m.paths) <= 3)
                    for path in m.paths:
                        self.assertEqual(path.points[0], m.entry.point)
                        self.assertTrue(2 <= len(path.points) <= 48)
                        for a, b in zip(path.points, path.points[1:]):
                            self.assertEqual(abs(a[0]-b[0]) + abs(a[1]-b[1]), 1)
                            if any(room['stairs'][p[1]] & (1 << p[0]) for p in (a,b)):
                                self.assertEqual(a[0], b[0])
                            for k in range(17):
                                x, y = ((a[i]+.5+(b[i]-a[i])*k/16)*26 for i in range(2))
                                for dx, dy in ((0, 0), (-6, 9), (0, 9), (6, 9)):
                                    cx, cy = int((x+dx)//26), int((y+dy)//26)
                                    self.assertTrue(room['roads'][node == 13][cy] & (1 << cx))
                    final = generate_valley_region(seed, node, entry, True)
                    self.assertTrue(all(p.terminal for p in final.paths) if node < 10 else
                                    all(p.terminal == (p.next_valley_node == 255) for p in final.paths))
            self.assertEqual(generate_valley_region(seed, 10, 0).layers,
                             generate_valley_region(seed, 13, 2).layers)
        with self.assertRaises(ValueError):
            generate_valley_region(38, 10, 2)

    def test_native_fingerprints_art_depth_and_real_journey(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = build_amoled_host(Path(directory) / 'region',
                                       [ROOT / 'tools/valley_region_host.cpp'])
            rows = subprocess.check_output([str(binary), '--fingerprints'], text=True)
            for row in rows.splitlines():
                seed, node, entry, final, expected = row.split()
                m = generate_valley_region(int(seed), int(node), int(entry), bool(int(final)))
                self.assertEqual(f'{fingerprint(m):08x}', expected, row)
            run = subprocess.run([str(binary), str(ROOT / 'data')], check=True,
                                 capture_output=True, text=True)
            self.assertIn('journey=ok quota=ok', run.stdout)

    def test_stick_and_touch_scene_journeys(self):
        from test_explore_ui_parity import compile_and_run
        with tempfile.TemporaryDirectory() as directory:
            for target in ('sticks3', 'ostb'):
                result, _ = compile_and_run(target, Path(directory) / target,
                    host=ROOT / 'tools/valley_scene_host.cpp')
                self.assertIn('journeys=2 quota=ok', result)


if __name__ == '__main__':
    unittest.main()
