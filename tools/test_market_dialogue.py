#!/usr/bin/env python3
"""Dialogue content coverage, malformed resources and the real market lock."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from generate_market_dialogue import ROOT, SOURCE, OUTPUT, compile_dialogue
sys.path.insert(0,str(ROOT/'tools/map_editor'))


class DialogueTests(unittest.TestCase):
    def setUp(self):
        self.content = json.loads(SOURCE.read_text())

    def test_generated_resource_and_npc_profiles(self):
        self.assertEqual(compile_dialogue(self.content), OUTPUT.read_bytes())
        market = json.loads((ROOT/'origin_asset/maps/market/market.stickmap.json').read_text())
        # Every NPC in the maintained map is named; unknown future species use
        # the unfiltered fallback scripts checked by the compiled host.
        profiles = {p['species'] for p in self.content['profiles']}
        from catalog import Catalog
        from market_export import compile_market
        runtime = compile_market(market, Catalog())
        species = {p['species'] for p in runtime['residents']}
        species.update(s for p in runtime['families'] for s in (p['adult'],p['juvenile']) if s)
        self.assertTrue(species <= profiles)

    def test_invalid_roles_content_and_fallbacks_rejected(self):
        bad=copy.deepcopy(self.content);bad['scripts'][0]['lines'][0]['speaker']='other'
        with self.assertRaises(ValueError): compile_dialogue(bad)
        bad=copy.deepcopy(self.content);bad['scripts'][0]['lines'][0]['text']='x'*49
        with self.assertRaises(ValueError): compile_dialogue(bad)
        bad=copy.deepcopy(self.content);bad['scripts']=[s for s in bad['scripts'] if s['context']!='pair']
        with self.assertRaises(ValueError): compile_dialogue(bad)

    def test_all_dialogue_glyphs_exist_in_both_fonts(self):
        chars={c for p in self.content['profiles'] for c in p['name'] if ord(c)>=128}
        chars.update(c for s in self.content['scripts'] for line in s['lines'] for c in line['text'] if ord(c)>=128)
        chars.add('：')
        for name,glyph_bytes in [('zh16',32),('zh32',128)]:
            data=(ROOT/f'data/packs/dev/fonts/{name}.smonfont').read_bytes()
            count=struct.unpack_from('<H',data,6)[0]
            glyphs={chr(struct.unpack_from('<I',data,16+i*(4+glyph_bytes))[0]) for i in range(count)}
            self.assertFalse(chars-glyphs, (name,chars-glyphs))

    def test_catalog_and_navigation_under_sanitizers(self):
        with tempfile.TemporaryDirectory() as directory:
            executable=Path(directory)/'dialogue'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(ROOT/'src'),
                *[str(ROOT/p) for p in ['tools/market_dialogue_host.cpp',
                    'src/platform/desktop/DesktopPlatform.cpp','src/platform/api/PlatformServices.cpp',
                    'src/core/ResourceFS.cpp','src/core/ResourcePack.cpp','src/game/Species.cpp']],
                '-o',str(executable)],check=True)
            subprocess.run([str(executable),str(OUTPUT),str(ROOT/'data')],check=True)


if __name__=='__main__':
    unittest.main()
