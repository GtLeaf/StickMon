"""Validate approved art, plant patches, playable routes and native integration."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from PIL import Image

from amoled_host import ROOT, build_amoled_host
from mist_forest import rooms,transitions,room_for,ASSETS,IMAGES,asset_image,cpp_table,stage
from mist_forest_layouts import design
from preview_mist_forest_progression import render as approved_render
from generate_runtime_tile_map import generate_map,Edge,fingerprint
from generate_explore_map import regular_tile
from asset_paths import essentials_dir


class MistForestTests(unittest.TestCase):
    def test_portable_tables_and_original_preview_art(self):
        self.assertEqual((ROOT/'src/game/ExploreMistForestData.h').read_text(),cpp_table())
        sheet=Image.open(essentials_dir()/'Graphics/Tilesets/Outside.png').convert('RGBA')
        for i in range(7):
            room=rooms()[0][i][2]
            image=Image.new('RGBA',(512,384))
            for layer in room['layers']:
                for cell,tile in enumerate(layer):
                    if tile:
                        art=asset_image(tile) if tile>=10000 else regular_tile(sheet,tile)
                        image.alpha_composite(art,(cell%16*32,cell//16*32))
            self.assertEqual(image.convert('RGB').tobytes(),
                             approved_render(design(i,38),sheet).tobytes(),i)

    def test_connected_patches_and_full_long_grass(self):
        # validate() checks group connectivity, size, gaps and road/root clearance.
        for seed in range(64):
            for i in range(7):
                for edge in range(4):
                    data=design(i,seed,edge)
                    self.assertTrue(data['plant_patches'])
        for group in (*rooms(),*transitions()):
            for oriented in group:
                for room in oriented:
                    for x,y in room['data']['long_grass']:
                        self.assertEqual(room['layers'][1][y*16+x],391)
                        self.assertEqual(room['layers'][0][(y+1)*16+x],399)

    def test_progression_ports_and_feet_stay_on_roads(self):
        for count in (6,7,8):
            sequence=[stage(i,count) for i in range(count)]
            self.assertEqual(sequence[:4],[0,1,2,3])
            self.assertEqual(sequence[-1],6)
            self.assertEqual(sequence,sorted(sequence))
            for seed in (38,294):
                for edge in range(4):
                    journey=[room_for(seed,edge,i,count) for i in range(count)]
                    for a,b in zip(journey,journey[1:]):
                        axis=0 if edge%2==0 else 1
                        self.assertEqual(a['exit'][axis],b['entry'][axis])
        for group in (*rooms(),*transitions()):
            for oriented in group:
                for edge,room in enumerate(oriented):
                    road=set(room['data']['roads'])
                    self.assertEqual(len(room['paths']),
                                     3 if room['data']['index']==6 else
                                     2 if room['data']['index']==2 else 1)
                    for points in room['paths']:
                        self.assertEqual(len(points), len(set(points)))
                        self.assertTrue(all(abs(a[0]-b[0])+abs(a[1]-b[1])==1
                                            for a,b in zip(points,points[1:])))
                        self.assertEqual(points[0],room['entry'])
                        self.assertEqual(points[-1],room['exit'])
                        for i,(x,y) in enumerate(points):
                            adjacent=points[max(0,i-1):i]+points[i+1:i+2]
                            wx=(x+.5+.5*any(p[0]==x for p in adjacent))*26
                            wy=(y+.5+.5*any(p[1]==y for p in adjacent))*26
                            for dx,dy in ((-6,9),(0,9),(6,9)):
                                self.assertIn((int((wx+dx)//26),int((wy+dy)//26)),road)
                    self.assertTrue(all(p in road for route in room['paths'] for p in route))

    def test_native_fingerprints_render_depth_feedback_and_journey(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(os.environ.get('STICKMON_MIST_ARTIFACT_DIR',directory)).resolve()
            output.mkdir(parents=True,exist_ok=True)
            binary=build_amoled_host(output/'mist-host',[ROOT/'tools/mist_forest_host.cpp'])
            fingerprints=subprocess.check_output([str(binary),'--fingerprints'],text=True)
            for row in fingerprints.splitlines():
                seed,count,level,edge,expected=row.split()
                actual=generate_map(int(seed),Edge(int(edge)),4,
                                    frost_level=int(level),frost_level_count=int(count))
                self.assertEqual(f'{fingerprint(actual):08x}',expected,row)
            run=subprocess.run([str(binary),str(ROOT/'data'),str(output)],text=True,capture_output=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            self.assertIn('mist-native=ok maps=56',run.stdout)
            self.assertIn('journeys=12',run.stdout)

    def test_wide_roads_do_not_join_distant_route_legs(self):
        for group in (*rooms(), *transitions()):
            for oriented in group:
                for room in oriented:
                    graph = {}
                    for route in room['paths']:
                        for a, b in zip(route, route[1:]):
                            graph.setdefault(a, set()).add(b)
                            graph.setdefault(b, set()).add(a)
                    road = set(room['data']['roads'])
                    owners = {cell: {p for p in graph
                                     if cell[0] in (p[0], p[0]+1)
                                     and cell[1] in (p[1], p[1]+1)} for cell in road}
                    self.assertTrue(all(owners.values()))
                    nearby = {}
                    for point in graph:
                        seen = front = {point}
                        for _ in range(3):
                            front = {n for p in front for n in graph[p]} - seen
                            seen = seen | front
                        nearby[point] = seen
                    for x, y in road:
                        for cell in {(x-1,y), (x+1,y), (x,y-1), (x,y+1)} & road:
                            self.assertTrue(any(nearby[p] & owners[cell]
                                                for p in owners[(x,y)]))

    def test_stick_scene_journeys(self):
        from test_explore_ui_parity import compile_and_run
        with tempfile.TemporaryDirectory() as directory:
            for target in ('sticks3',):
                result,_=compile_and_run(target,Path(directory)/target,
                                        host=ROOT/'tools/mist_forest_scene_host.cpp')
                self.assertIn('mist-scene=ok journeys=12',result)


if __name__=='__main__':
    unittest.main()
