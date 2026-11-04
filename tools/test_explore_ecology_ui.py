#!/usr/bin/env python3
"""Verify OSTB ecology through the real scene and production rendering."""
import json
from pathlib import Path
import tempfile
import unittest

from build_web import ROOT
from test_explore_ui_parity import compile_and_run


class ExploreEcologyUiTests(unittest.TestCase):
    def test_habitats_pause_reveal_cover_grass_and_resident_artwork(self):
        expected = json.loads((ROOT / 'tools/fixtures/explore_ecology_ui.json').read_text())
        expected.update(json.loads((ROOT / 'tools/fixtures/explore_ecology_ui_hud.json').read_text()))
        expected.update(json.loads((ROOT / 'tools/fixtures/explore_ecology_ui_route_anchor.json').read_text()))
        expected.update(json.loads((ROOT / 'tools/fixtures/explore_ecology_ui_lower_third.json').read_text()))
        with tempfile.TemporaryDirectory() as directory:
            for debug in (False, True):
                with self.subTest(debug=debug):
                    result, frames = compile_and_run(
                        'ostb', Path(directory) / str(int(debug)), debug=debug,
                        host=ROOT / 'tools/explore_ecology_ui_host.cpp')
                    self.assertEqual(result, 'ecology-ui=ok areas=6 actors=12\n')
                    self.assertEqual(expected, frames)


if __name__ == '__main__':
    unittest.main()
