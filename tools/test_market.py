#!/usr/bin/env python3
"""Authored town export, stable destinations and shared simulation contracts."""
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/map_editor'))
from catalog import Catalog
from market_export import compile_market, header, size_header, package
import npcs

class MarketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog=Catalog()
        cls.project=json.loads((ROOT/'origin_asset/maps/market/market.stickmap.json').read_text())

    def test_generated_data_matches_maintained_source(self):
        d=compile_market(self.project,self.catalog)
        self.assertEqual(header(d),(ROOT/'src/game/MarketMapData.h').read_text())
        self.assertEqual((len(d['residents']),len(d['families'])),(4,7))
        self.assertGreater(len(d['stops']),2)

    def test_expanded_map_preserves_layers_and_actor_positions(self):
        from project import flatten
        d=compile_market(self.project,self.catalog)
        self.assertEqual(d['size'],[18,17])
        self.assertEqual(size_header(d),(ROOT/'src/game/MarketMapSize.h').read_text())
        layers,_=flatten(self.project)
        for z in range(3):
            self.assertEqual(len(d['layers'][z]),306)
            self.assertEqual(d['layers'][z],[self.catalog.assets[k]['runtimeId'] if k else 0 for k in layers[z]])
        self.assertEqual([(r['species'],r['activity']['spawn']) for r in d['residents']],
                         [(352,201),(94,210),(115,141),(113,63)])
        self.assertNotIn(172,[r['species'] for r in d['residents']])

    def test_moved_actor_reexports_without_hardcoded_spawn(self):
        p=copy.deepcopy(self.project);p['map']['npcs'][0]['spawn']=[3,10]
        d=compile_market(p,self.catalog)
        self.assertEqual(d['residents'][0]['activity']['spawn'],10*18+3)
        self.assertIn('activity.spawn=183',header(d))

    def test_stale_geometry_or_source_blocks_export(self):
        p=copy.deepcopy(self.project);p['map']['objects'][0]['x']+=1
        with self.assertRaises(ValueError):compile_market(p,self.catalog)
        p=copy.deepcopy(self.project);p['sources']['runtime-art']['sha256']='changed'
        with self.assertRaises(ValueError):compile_market(p,self.catalog)

    def test_market_package_contains_versioned_nav_and_original_sprites(self):
        blob,report=package(self.project,self.catalog)
        self.assertEqual(report['errors'],0)
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            self.assertIn('MarketMapData.h',z.namelist())
            self.assertEqual(json.loads(z.read('market-runtime.json'))['version'],2)
            self.assertEqual(z.read('npc-packs/162.smonsp'),(ROOT/'data/packs/dev/sprites/162.smonsp').read_bytes())

    def test_editor_uses_same_natural_activity_core(self):
        a=npcs.preview(self.project,self.catalog,1200,38)
        b=npcs.preview(self.project,self.catalog,1200,38)
        self.assertEqual(a['frames'],b['frames'])
        self.assertEqual(a['navigationVersion'],2)
        index=len(self.project['map']['npcs'])
        visible={frame[index][6] for frame in a['frames'] if frame[index][5]}
        self.assertTrue(visible)

    def test_large_runtime_indices_camera_and_explore_isolation(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'size'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                '-I',str(ROOT/'src'),str(ROOT/'tools/market_size_host.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_long_runs_have_no_collisions_and_can_leave(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'host'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ROOT/'src'),str(ROOT/'tools/market_host.cpp'),'-o',str(exe)],check=True)
            for seed in (1,2,3,4,5,17,38,99,1024,65535):
                result=subprocess.run([str(exe),str(seed)],text=True,capture_output=True)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_manual_walk_collision_chat_and_departure(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'walk'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ROOT/'src'),str(ROOT/'tools/market_walk_host.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_solo_visitors_complete_round_trips_without_hidden_actors(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'visitors'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ROOT/'src'),str(ROOT/'tools/market_visitors_host.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_lively_groups_receivers_shows_and_pause(self):
        with tempfile.TemporaryDirectory() as temp:
            exe=Path(temp)/'lively'
            subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                            '-I',str(ROOT/'src'),str(ROOT/'tools/market_liveliness_host.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

    def test_market_settings_round_trip_and_invalid_capacity(self):
        from market_export import wire, preview_activity
        d=compile_market(self.project,self.catalog)
        self.assertEqual((d['maxGroups'],d['maxVisitors'],d['soloPercent']),(3,6,60))
        self.assertEqual(len(d['stalls']),4)
        self.assertEqual([r['reception'] for r in d['residents']],[3,0,0,0])
        p=copy.deepcopy(self.project);p['map']['market']['maxGroups']=4
        with self.assertRaises(ValueError):compile_market(p,self.catalog)
        p=copy.deepcopy(self.project);p['map']['npcs'][0]['reception']['customerPoints']=[[0,0]]
        with self.assertRaises(ValueError):compile_market(p,self.catalog)
        p=copy.deepcopy(self.project);p['map'].pop('market')
        for n in p['map']['npcs']:n.pop('reception')
        legacy=compile_market(p,self.catalog)
        self.assertEqual((legacy['maxGroups'],legacy['stalls']),(1,[]))
        timeline=preview_activity(self.project,self.catalog,1200,38)
        self.assertEqual(len(timeline['actors']),10)
        self.assertTrue(any(p[7] for f in timeline['frames'] for p in f))
        self.assertTrue(any(p[8]>0 for f in timeline['frames'] for p in f))

    def test_market_pool_includes_solos_and_expanded_pairs(self):
        d=compile_market(self.project,self.catalog)
        self.assertEqual({r['species'] for r in d['residents']},{94,113,115,352})
        pairs={(f['adult'],f['juvenile']) for f in d['families']}
        self.assertTrue({(25,172),(162,161),(286,285),(279,278),(225,0),(184,0),(212,0)}==pairs)
        blob,_=package(self.project,self.catalog)
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            for sid in (94,113,115,352):self.assertIn(f'npc-packs/{sid:03d}.smonsp',z.namelist())
            self.assertNotIn('npc-packs/000.smonsp',z.namelist())

if __name__=='__main__':unittest.main()
