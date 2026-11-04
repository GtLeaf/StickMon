"""Check random cave layouts against movement and original-material rules."""

import copy
import json
import unittest

from valley_bridge_layouts import generate, fingerprint, validate


class ValleyBridgeLayoutsTest(unittest.TestCase):
    def test_seed_sweep_and_regeneration(self):
        families, fingerprints = set(), set()
        for seed in range(128):
            data = generate(seed)
            families.add(data['family'])
            fingerprints.add(fingerprint(data))
            self.assertEqual(data, generate(seed))
            self.assertEqual(data['checks']['result'], 'pass')
        self.assertEqual(families, set(range(4)))
        self.assertEqual(len(fingerprints), 128)

    def test_extreme_seeds_and_serialized_reentry(self):
        for seed in (0, 38, 91, 6144, 0x7FFFFFFF, 0xFFFFFFFF):
            original = generate(seed)
            loaded = json.loads(json.dumps(original))
            self.assertEqual(validate(loaded)['result'], 'pass')
            self.assertEqual(fingerprint(original), fingerprint(generate(seed)))

    def test_four_distinct_terrain_families(self):
        data = [generate(38, family=i) for i in range(4)]
        self.assertEqual(len({fingerprint(d, True) for d in data}), 4)
        self.assertGreater(len({tuple(d['rooms']['A']['bridge']) for d in data}), 2)

    def test_reject_stair_bypass(self):
        for key in 'BC':
            data = generate(38)
            # Clear a dry vertical opening beside the stairs. The validator
            # must detect that the required elevation change is bypassable.
            room, recipe = data['rooms'][key], data['recipe']
            x = recipe[key.lower() + '_stair'] + 1
            room['cliffs'] = [p for p in room['cliffs'] if p[0] != x]
            with self.assertRaises(AssertionError):
                validate(data)

    def test_reject_swapped_banks_and_mismatched_portals(self):
        data = generate(38)
        x = data['recipe']['river'] - 1
        data['rooms']['A']['layers'][1][2 * 16 + x] = 6588
        with self.assertRaises(AssertionError):
            validate(data)
        data = generate(38)
        data['links'][0]['b'][-1] -= 1
        with self.assertRaises(AssertionError):
            validate(data)

    def test_reject_dry_bank_below_waterfall(self):
        data = generate(38)
        for key in 'ABC':
            broken = copy.deepcopy(data)
            room = broken['rooms'][key]
            fall = room['falls'][0]
            tile = room['layers'][1][(fall['splash'] + 1) * 16 + fall['left']]
            room['water_quarters'][tile][0] = (12, False)
            with self.assertRaises(AssertionError):
                validate(broken)

    def test_reject_detached_branch(self):
        data = generate(38)
        room = data['rooms']['C']
        room['paths'].append(dict(level=0, name='detached', cells=[(14, 10)]))
        room['decor'] = [p for p in room['decor'] if tuple(p) != (14, 10)]
        with self.assertRaisesRegex(AssertionError, 'disconnected route branch'):
            validate(data)


if __name__ == '__main__':
    unittest.main()
