#!/usr/bin/env python3
"""Source round trips, native compatibility and export rejection contracts."""
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
from catalog import Catalog, runtime, ROAD_TILES
from project import base, snapshot, validate, flatten, check_project, compile_project, header, to_runtime, pave, route_world, render_project
from map_editor_assets import cpp_table, TILES, FRAMES
from slateport_market import OBJECTS as MARKET_OBJECTS, SOURCE as MARKET_SOURCE, tile_image as market_tile
import lake_autotile as lake
import map_editor_rotations as rotations

class EditorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.catalog=Catalog()

    def simple(self):
        p=base(self.catalog)
        p['map']['paths']=[dict(points=[[7,y] for y in range(11,-1,-1)],exit=dict(point=[7,0],edge='top'),
                    cellCentered=False,fallsToNextLevel=False,nextValleyNode=255,nextValleyEntrance=0,terminal=False,interiorExit=False)]
        return pave(p,self.catalog)

    def test_road_network_source_roundtrip_and_validation(self):
        p=base(self.catalog)
        p['map']['roadNetwork']=dict(version=1,source='[]',nodes=[[7,10],[7,11],[7,12]],
            edges=[[0,1],[1,2]],components=1,junctions=0,uncovered=[])
        self.assertEqual(check_project(json.loads(json.dumps(p))),p)
        for field,value in [('nodes',[[True,10]]),('edges',[[0,4]]),('edges',[[0,2]]),
                            ('edges',[[0,1],[1,0]]),('uncovered',[[16,0]])]:
            bad=copy.deepcopy(p);bad['map']['roadNetwork'][field]=value
            with self.assertRaises(ValueError): check_project(bad)

    def test_road_route_saved_and_reopened_through_http(self):
        import threading
        import urllib.request
        from http.server import ThreadingHTTPServer
        import importlib.util
        spec=importlib.util.spec_from_file_location('map_editor_http',ROOT/'tools/map_editor/serve.py')
        server_module=importlib.util.module_from_spec(spec);spec.loader.exec_module(server_module)
        Handler=server_module.Handler
        p=self.simple()
        script="""import fs from 'node:fs';
import {recognizeRoads,storeRoadRoute} from './tools/map_editor/road-network.js';
const {p,catalog}=JSON.parse(fs.readFileSync(0,'utf8'));
p.map.paths=[];p.map.roadNetwork=recognizeRoads(p,catalog);storeRoadRoute(p);
process.stdout.write(JSON.stringify(p));"""
        p=json.loads(subprocess.run(['node','--input-type=module','-e',script],cwd=ROOT,
            input=json.dumps(dict(p=p,catalog=dict(assets=self.catalog.assets))),text=True,capture_output=True,check=True).stdout)
        check_project(p)
        report=validate(p,self.catalog)
        self.assertEqual(report['errors'],0,report['issues'])
        self.assertNotIn('loop',[v['code'] for v in report['issues']])
        self.assertEqual(route_world(p['map']['paths'][0]),[[x*26,y*26] for x,y in p['map']['paths'][0]['points']])
        with self.assertRaisesRegex(ValueError,'道路往返'): to_runtime(p,self.catalog)
        bad=copy.deepcopy(p);bad['map']['layers'][0][0]='outside:386'
        self.assertIn('road-route-stale',[v['code'] for v in validate(bad,self.catalog)['issues']])
        bad=copy.deepcopy(p);bad['map']['paths'][0]['points'][1]=[0,0]
        self.assertIn('road-route-links',[v['code'] for v in validate(bad,self.catalog)['issues']])
        with tempfile.TemporaryDirectory() as temp:
            server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
            server.drafts=Path(temp);server.catalog=self.catalog;server.token='test-only';server.work_lock=threading.Lock()
            worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
            url=f'http://127.0.0.1:{server.server_port}'
            def save(value):
                request=urllib.request.Request(url+'/api/save',json.dumps(value).encode(),headers={'X-Editor-Token':server.token,'Content-Type':'application/json'})
                with urllib.request.urlopen(request) as response:return json.load(response)
            try:
                saved=save(p)
                with urllib.request.urlopen(url+'/api/draft/'+saved['file']) as response: reopened=json.load(response)
                p['draftId']=saved['draftId'];self.assertEqual(reopened,p)
                self.assertEqual(save(reopened),saved)
                self.assertEqual(len(list(Path(temp).glob('*.stickmap.json'))),1)
            finally:server.shutdown();server.server_close();worker.join()

    def test_curated_asset_tables_and_capacity(self):
        import hashlib
        self.assertEqual(hashlib.sha256(json.dumps(rotations.TILES[:99]).encode()).hexdigest(),
                         '5696d3e742bdd563fe57dbd104f068c784a26314528e602a76cfbf7813030ee5')
        self.assertEqual((ROOT/'src/game/ExploreEditorTiles.h').read_text(),cpp_table())
        self.assertEqual(len(TILES),156)
        self.assertEqual(len(FRAMES),159)
        self.assertEqual(hashlib.sha256(json.dumps(TILES[:71]).encode()).hexdigest(),
                         '5bfdec1099bfda3be451fec42a5b809a7eafea5d508a80a3718031f472de5af1')
        for k,v in self.catalog.pack_stats['limits'].items(): self.assertLessEqual(self.catalog.pack_stats[k],v)
        for pref in self.catalog.prefabs:
            for cell in pref['cells']: self.assertIn(cell['asset'],self.catalog.assets)

    def test_library_deletion_preserves_map_and_export(self):
        p=self.simple()
        result=subprocess.run(['node','tools/map_editor/test_library.mjs'],cwd=ROOT,
            input=json.dumps(dict(project=p,catalog=self.catalog.payload())),text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)
        deleted=json.loads(result.stdout)
        self.assertEqual(check_project(json.loads(json.dumps(deleted)),self.catalog),deleted)
        self.assertEqual(validate(deleted,self.catalog)['errors'],0)
        data,report=compile_project(deleted,self.catalog)
        self.assertIsNotNone(data,report['issues'])

    def test_road_brushes_use_exportable_ground_and_preserve_route(self):
        ground=[p for p in self.catalog.prefabs if p.get('brushAsset')]
        self.assertEqual(len(ground),18)
        self.assertEqual({p['category'] for p in self.catalog.prefabs if not p.get('hidden')},
                         {'道路地面','湖泊','自然景物','集市','围栏设施'})
        p=self.simple(); before=copy.deepcopy(p['map']['paths'])
        for i,pref in enumerate(ground):
            key=pref['brushAsset']; a=self.catalog.assets[key]
            self.assertIsNotNone(a['runtimeId']); self.assertFalse(a['water'])
            self.assertEqual(a['depth'][0],0)
            self.assertEqual(pref['cells'],[dict(x=0,y=0,layer=0,asset=key)])
            p['map']['layers'][0][(i//6+2)*16+i%6]=key
        data,report=compile_project(p,self.catalog)
        self.assertIsNotNone(data,report['issues']); self.assertTrue(report['nativeVerified'])
        self.assertEqual(p['map']['paths'],before)
        road={(x,y) for x in range(2,5) for y in range(2,5)}
        r=to_runtime(p,self.catalog)
        self.assertEqual(runtime.road_tile(r,road,3,3),546)
        for missing,tile in [((2,2),558),((4,2),556),((2,4),542),((4,4),540)]:
            self.assertEqual(runtime.road_tile(r,road-{missing},3,3),tile)
        self.assertEqual(len(ROAD_TILES),13)

    def test_market_rotations_preserve_pixels_and_native_semantics(self):
        from PIL import Image
        self.assertEqual(len(rotations.TILES),107)
        import hashlib
        self.assertEqual(hashlib.sha256(json.dumps(rotations.TILES[:83]).encode()).hexdigest(),
                         'f5a97bba9c200cd35b80a765bf0d9519ff0e10758e667dadb96e67a7781ae6b3')
        self.assertEqual((ROOT/'src/game/ExploreRotatedTiles.h').read_text(),rotations.cpp_table())
        prefs={p['id']:p for p in self.catalog.prefabs}
        def image(pref):
            out=Image.new('RGBA',(pref['width']*32,pref['height']*32))
            for c in pref['cells']: out.alpha_composite(self.catalog.tile(c['asset']),(c['x']*32,c['y']*32))
            return out
        for family,ids in rotations.families().items():
            source=image(prefs[family])
            for turn,key in enumerate(ids[1:],1):
                pref=prefs[key]
                self.assertEqual(image(pref).tobytes(),source.rotate(-turn*90,expand=True).tobytes())
                self.assertTrue(pref['hidden'])
                for c in pref['cells']:
                    a=self.catalog.assets[c['asset']]
                    self.assertEqual(a['depth'],[0,pref['height']-c['y'],0])
                    self.assertFalse(a['water'])
                    self.assertEqual(a['requiredLayer'],1)
        for i,spec in enumerate(rotations.TILES):
            self.assertEqual(spec['id'],13000+i)
            self.assertEqual(spec['kind'],f'EXPLORE_ROTATED_{i}' if i<75 else
                             f'EXPLORE_FLOWER_BOX_{i-75}' if i<83 else
                             f'EXPLORE_DECKCHAIR_{i-83}' if i<99 else f'EXPLORE_CRATE_STACK_{i-99}')

    def test_crate_stacks_preserve_source_pixels_and_ground_collision(self):
        from PIL import Image
        import npcs
        for family,top,body in [('market-crate',814,822),('market-label-crate',815,823)]:
            for count in (2,3):
                key=f'{family}-stack{count}'
                pref=next(p for p in self.catalog.prefabs if p['id']==key)
                actual=Image.new('RGBA',(32,64))
                for c in pref['cells']:
                    actual.alpha_composite(self.catalog.tile(c['asset']),(0,c['y']*32))
                    self.assertEqual(self.catalog.assets[c['asset']]['depth'],[0,2-c['y'],2-c['y']])
                    self.assertEqual(c['layer'],2 if c['y']==0 else 1)
                    self.assertEqual(self.catalog.assets[c['asset']]['requiredLayer'],c['layer'])
                expected=Image.new('RGBA',(32,64))
                expected.alpha_composite(market_tile(top),(0,16 if count==2 else 0))
                expected.alpha_composite(market_tile(body).crop((0,16,32,32)) if count==2 else market_tile(body),
                                         (0,48 if count==2 else 32))
                self.assertEqual(actual.tobytes(),expected.tobytes())
                self.assertEqual(pref['footprint'],[[0,1]])
                self.assertTrue(pref['hidden'])
                p=self.simple();p['map']['objects']=[dict(id='stack',prefab=key,x=2,y=5)]
                grid=npcs.walkable(p,self.catalog)
                self.assertTrue(grid[5*16+2]);self.assertFalse(grid[6*16+2])
                data,report=compile_project(p,self.catalog)
                self.assertIsNotNone(data,report['issues'])
                self.assertTrue(report['nativeVerified'])

    def test_source_sampling_preserves_alpha_and_ground(self):
        result=subprocess.run(['node','tools/map_editor/test_sampling.mjs'],cwd=ROOT,
            input=json.dumps(self.catalog.payload()),text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)
        for tile in (1381,1411,1419,1427,859):
            key=f'outside:{tile}'
            self.assertTrue(self.catalog.assets[key]['hasTransparency'])
            image=self.catalog.tile(key)
            p=self.simple()
            pref=dict(id=f'sample-{tile}',name='原图取样组合',category='自定义',width=1,height=1,
                      cells=[dict(x=0,y=0,layer=0,asset=key)],anchor=[.5,1],anchorMode='runtime',footprint=[])
            p['prefabs'].append(pref);p['map']['objects']=[dict(id='sample',prefab=pref['id'],x=2,y=3)]
            original=image.tobytes();check_project(p,self.catalog)
            self.assertEqual(pref['cells'][0]['layer'],1)
            self.assertEqual(flatten(p)[0][0][50],'outside:385')
            shown=render_project(p,self.catalog).crop((64,96,96,128)).convert('RGBA')
            expected=self.catalog.tile('outside:385').copy();expected.alpha_composite(image)
            self.assertEqual(shown.tobytes(),expected.tobytes())
            self.assertEqual(self.catalog.tile(key).tobytes(),original)
            canonical=copy.deepcopy(p);check_project(p,self.catalog);self.assertEqual(p,canonical)
            if tile==859:
                self.assertEqual(pref['footprint'],[[0,0]])
                data,report=compile_project(p,self.catalog)
                self.assertIsNotNone(data,report['issues']);self.assertTrue(report['nativeVerified'])
        # Explicitly customized geometry and ordinary ground tiles stay intact.
        for asset,anchor in [('outside:1381',[.5,.5]),('outside:546',[.5,1])]:
            p=self.simple();pref=dict(id='sample-custom',name='custom',category='自定义',width=1,height=1,
                cells=[dict(x=0,y=0,layer=0,asset=asset)],anchor=anchor,anchorMode='runtime',footprint=[])
            p['prefabs'].append(pref);before=copy.deepcopy(pref);check_project(p,self.catalog)
            self.assertEqual(pref,before)

    def test_lamp_shadow_keeps_road_open_and_pole_blocked(self):
        import npcs
        self.assertEqual(self.catalog.assets['outside:1425']['runtimeId'],11027)
        self.assertEqual(self.catalog.assets['outside:1425']['depth'],[0,0,0])
        self.assertTrue(self.catalog.assets['outside:1425']['behind'][1])
        self.assertEqual(self.catalog.assets['outside:1424']['depth'],[0,1,0])
        p=self.simple()
        pref=next(v for v in p['prefabs'] if v['id']=='lamp')
        self.assertEqual(pref['footprint'],[[0,2]])
        p['map']['objects']=[dict(id='lamp',prefab='lamp',x=6,y=3)]
        grid=npcs.walkable(p,self.catalog)
        self.assertFalse(grid[5*16+6]);self.assertTrue(grid[5*16+7])
        result=subprocess.run(['node','tools/map_editor/test_lamp_shadow.mjs'],cwd=ROOT,
            input=json.dumps(dict(p=p,catalog=self.catalog.payload())),text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        data,report=compile_project(p,self.catalog)
        self.assertIsNotNone(data,report['issues']);self.assertTrue(report['nativeVerified'])
        p['map']['objects']=[]
        p['map']['layers'][1][5*16+6]='outside:1424'
        p['map']['layers'][1][5*16+7]='outside:1425'
        grid=npcs.walkable(p,self.catalog)
        self.assertFalse(grid[5*16+6]);self.assertTrue(grid[5*16+7])

    def test_legacy_lamp_footprints_migrate_without_changing_custom_geometry(self):
        p=self.simple();pref=next(v for v in p['prefabs'] if v['id']=='lamp')
        pref['footprint']=[[0,2],[1,2]]
        sample=copy.deepcopy(pref);sample.update(id='sample-lamp',anchorMode='runtime')
        p['prefabs'].append(sample)
        custom=copy.deepcopy(sample);custom.update(id='instance-lamp',footprint=[[1,2]])
        p['prefabs'].append(custom)
        layered=copy.deepcopy(sample);layered['id']='sample-layered'
        layered['cells'].append(dict(x=1,y=2,layer=2,asset='rotated:13100'))
        p['prefabs'].append(layered)
        p['map']['objects']=[dict(id='lamp',prefab=sample['id'],x=6,y=3)]
        before=copy.deepcopy(p);check_project(p,self.catalog)
        self.assertEqual(pref['footprint'],[[0,2]])
        self.assertEqual(sample['footprint'],[[0,2]])
        self.assertEqual(custom['footprint'],[[1,2]])
        self.assertEqual(layered['footprint'],[[0,2],[1,2]])
        self.assertEqual(p['map'],before['map'])
        for current,old in zip(p['prefabs'],before['prefabs']):
            self.assertEqual({k:v for k,v in current.items() if k!='footprint'},
                             {k:v for k,v in old.items() if k!='footprint'})
        once=copy.deepcopy(p);check_project(p,self.catalog);self.assertEqual(p,once)

    def test_rectangle_group_selection(self):
        result=subprocess.run(['node','tools/map_editor/test_selection.mjs'],cwd=ROOT,text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_stack_alpha_overlay_and_legacy_upgrade(self):
        from PIL import Image
        for family in ('market-crate','market-label-crate'):
            p=self.simple()
            p['map']['objects']=[dict(id='truck',prefab='truck-left',x=1,y=4),
                                dict(id='crate',prefab=family+'-stack2',x=2,y=5)]
            pref=next(v for v in p['prefabs'] if v['id']==family+'-stack2')
            truck=next(v for v in p['prefabs'] if v['id']=='truck-left')
            truck_cell=next(c for c in truck['cells'] if c['x']==1 and c['y']==1)
            layers,owners=flatten(p)
            self.assertEqual(layers[1][5*16+2],truck_cell['asset'])
            self.assertEqual(layers[2][5*16+2],pref['cells'][0]['asset'])
            expected=Image.new('RGBA',(32,32))
            for z in range(3): expected.alpha_composite(self.catalog.tile(layers[z][5*16+2]))
            image=render_project(p,self.catalog)
            self.assertEqual(image.crop((64,160,96,192)).convert('RGB').tobytes(),expected.convert('RGB').tobytes())
            upper=self.catalog.tile(pref['cells'][0]['asset'])
            self.assertIsNone(upper.crop((0,0,32,16)).getbbox())
            legacy=copy.deepcopy(p)
            for v in legacy['prefabs']:
                if '-stack' in v['id']:
                    for cell in v['cells']: cell['layer']=1
            check_project(legacy)
            self.assertEqual(legacy,p)
            check_project(legacy);self.assertEqual(legacy,p)
            custom=copy.deepcopy(p)
            custom_pref=next(v for v in custom['prefabs'] if v['id']==pref['id'])
            custom_pref['anchor']=[.5,1.5]
            for cell in custom_pref['cells']:cell['layer']=1
            before=copy.deepcopy(custom_pref);check_project(custom)
            self.assertEqual(custom_pref,before)
            data,report=compile_project(p,self.catalog)
            self.assertIsNotNone(data,report['issues']);self.assertTrue(report['nativeVerified'])
            # A raw upper fragment cannot export without its lower half.
            broken=self.simple();broken['map']['layers'][2][5*16+2]=pref['cells'][0]['asset']
            self.assertIn('assembly',[v['code'] for v in validate(broken,self.catalog)['issues']])

    def test_stacking_model_and_native_export(self):
        proc=subprocess.run(['node','tools/map_editor/test_stacking.mjs'],cwd=ROOT,
            input=json.dumps(dict(project=self.simple(),catalog=self.catalog.payload())),
            text=True,capture_output=True)
        self.assertEqual(proc.returncode,0,proc.stderr)
        cases=json.loads(proc.stdout)
        self.assertEqual(len(cases),12)
        for p in cases:
            data,report=compile_project(p,self.catalog)
            self.assertIsNotNone(data,report['issues'])
            self.assertTrue(report['nativeVerified'])

    def test_compact_deckchairs_preserve_all_visible_pixels(self):
        from PIL import Image
        import npcs
        for color,tiles in [('green',[736,744,752]),('blue',[737,745,753])]:
            key=f'market-deckchair-{color}-compact'
            pref=next(p for p in self.catalog.prefabs if p['id']==key)
            self.assertEqual((pref['width'],pref['height']),(1,2))
            self.assertEqual(pref['footprint'],[[0,1]])
            source=Image.new('RGBA',(32,96))
            for y,tile in enumerate(tiles):source.alpha_composite(market_tile(tile),(0,y*32))
            compact=Image.new('RGBA',(32,64))
            for c in pref['cells']:compact.alpha_composite(self.catalog.tile(c['asset']),(0,c['y']*32))
            self.assertEqual(compact.tobytes(),source.crop((0,16,32,80)).tobytes())
            self.assertEqual(sum(a>0 for a in source.getchannel('A').tobytes()),
                             sum(a>0 for a in compact.getchannel('A').tobytes()))
            p=self.simple();p['map']['objects']=[dict(id='chair',prefab=key,x=2,y=2)]
            grid=npcs.walkable(p,self.catalog)
            self.assertFalse(grid[3*16+2]);self.assertTrue(grid[4*16+2])
            self.assertFalse(pref['hidden'])
            self.assertTrue(next(p for p in self.catalog.prefabs if p['id']==f'market-deckchair-{color}')['hidden'])

    def test_compact_flower_box_crops_margins_without_resizing(self):
        from PIL import Image
        pref=next(p for p in self.catalog.prefabs if p['id']=='flower-box-compact')
        self.assertEqual((pref['width'],pref['height']),(2,1))
        self.assertEqual(pref['footprint'],[[0,0],[1,0]])
        image=Image.new('RGBA',(64,32))
        for c in pref['cells']:image.alpha_composite(self.catalog.tile(c['asset']),(c['x']*32,0))
        original=rotations.source_sheet('Outside.png').crop((16,3952,80,3984))
        # Transparent pixel RGB values do not affect the rendered result.
        expected=Image.new('RGBA',(64,32));expected.alpha_composite(original)
        self.assertEqual(image.tobytes(),expected.tobytes())
        self.assertFalse(pref['hidden'])
        self.assertTrue(next(p for p in self.catalog.prefabs if p['id']=='flower-box')['hidden'])

    def test_rotation_ui_model_roundtrip_and_native_export(self):
        proc=subprocess.run(['node','tools/map_editor/test_rotation.mjs'],cwd=ROOT,
            input=json.dumps(dict(project=self.simple(),catalog=dict(prefabs=self.catalog.prefabs,rotations=rotations.families()))),
            text=True,capture_output=True,check=True)
        cases=json.loads(proc.stdout)
        self.assertEqual(len(cases),37)
        for p in cases:
            data,report=compile_project(p,self.catalog)
            self.assertIsNotNone(data,report['issues'])
            self.assertTrue(report['nativeVerified'])

    def test_rotated_assembly_still_requires_complete_object(self):
        p=self.simple();pref=next(v for v in p['prefabs'] if v['id']=='table-check-r90')
        p['map']['layers'][1][17]=pref['cells'][0]['asset']
        self.assertIn('assembly',[v['code'] for v in validate(p,self.catalog)['issues']])

    def test_original_market_sources_and_animation(self):
        import hashlib
        provenance=json.loads((MARKET_SOURCE/'sources.json').read_text())
        for f in provenance['files']:
            self.assertEqual(hashlib.sha256((MARKET_SOURCE/f['path']).read_bytes()).hexdigest(),f['sha256'])
        frames=[self.catalog.tile('slateport:836',f) for f in range(8)]
        # Emerald repeats the neutral pose in slots 1 and 3: four timed
        # frames, three distinct silhouettes. Preserve that source sequence.
        self.assertEqual(len(set(im.tobytes() for im in frames)),3)
        self.assertEqual(frames[1].tobytes(),frames[3].tobytes())
        for i,im in enumerate(frames):
            self.assertEqual(im.tobytes(),market_tile(836,i%4).tobytes())
            self.assertEqual(im.getpixel((0,0))[3],0)
        self.assertEqual(self.catalog.assets['slateport:836']['frames'],4)

    def test_red_flowers_sway_without_moving_roots_or_losing_pixels(self):
        from collections import Counter
        from generate_explore_map import regular_tile
        from catalog import sheet
        original=regular_tile(sheet('outside'),1340)
        frames=[self.catalog.tile('outside:1340',f) for f in range(8)]
        self.assertEqual(self.catalog.assets['outside:1340']['runtimeId'],14000)
        self.assertEqual(self.catalog.assets['outside:1340']['frames'],8)
        self.assertEqual(frames[0].tobytes(),original.tobytes())
        self.assertEqual(len({im.tobytes() for im in frames}),3)
        roots=original.crop((0,22,32,32)).tobytes()
        opaque=lambda im:Counter(p for p in im.getdata() if p[3])
        for f,im in enumerate(frames):
            self.assertEqual(im.crop((0,22,32,32)).tobytes(),roots)
            self.assertEqual(opaque(im),opaque(original))
            self.assertEqual(self.catalog.tile('outside:1340',f+8).tobytes(),im.tobytes())

    def test_all_market_objects_compile_and_keep_original_ids(self):
        for i,(kind,tid,_,_,_) in enumerate(TILES[:40]):
            self.assertEqual((kind,tid),(f'EXPLORE_EDITOR_{i}',11000+i))
        for key,_,_ in MARKET_OBJECTS:
            p=self.simple()
            p['map']['objects']=[dict(id='market',prefab=key,x=1,y=2)]
            data,report=compile_project(p,self.catalog)
            self.assertIsNotNone(data,(key,report['issues']))
            self.assertTrue(report['nativeVerified'])

    def test_all_new_objects_compile_with_native_depth_and_water(self):
        for pref in self.catalog.prefabs:
            p=self.simple()
            p['map']['objects']=[dict(id='one',prefab=pref['id'],x=1,y=1)]
            data,report=compile_project(p,self.catalog)
            self.assertIsNotNone(data,(pref['id'],report['issues']))
            self.assertTrue(report['nativeVerified'])
        flowers=next(p for p in self.catalog.prefabs if p['id']=='flowers-single')
        self.assertEqual((flowers['width'],flowers['height'],len(flowers['cells'])),(1,1,1))
        self.assertFalse(flowers['footprint'])
        p=self.simple()
        p['map']['objects']=[dict(id='lake',prefab='lake-small',x=7,y=2)]
        self.assertIn('water',[i['code'] for i in validate(p,self.catalog)['issues']])

    def test_umbrellas_are_complete_and_preserve_legacy_depths(self):
        for color,first in [('blue',738),('orange',740),('green',742)]:
            pref=next(p for p in self.catalog.prefabs if p['id']==f'market-umbrella-{color}-full')
            self.assertEqual((pref['width'],pref['height'],len(pref['cells'])),(2,3,5))
            self.assertEqual([self.catalog.assets[c['asset']]['tile'] for c in pref['cells']],
                             [first,first+1,first+8,first+9,first+16])
            for cell in pref['cells']:
                self.assertEqual(self.catalog.assets[cell['asset']]['depth'][1]+cell['y'],3)
            self.assertEqual(self.catalog.assets[f'slateport:{first}']['depth'][1],2)
            self.assertEqual(self.catalog.assets[f'slateport:{first}:d3']['depth'][1],3)

    def test_snapshot_roundtrip_all_regions(self):
        for area in range(6):
            p=snapshot(self.catalog,dict(area=area,seed=38))
            check_project(json.loads(json.dumps(p)))
            r=to_runtime(p,self.catalog)
            self.assertEqual(f'{runtime.fingerprint(r):08x}',p['origin']['fingerprint'])
            self.assertEqual(r.layers,to_runtime(json.loads(json.dumps(p)),self.catalog).layers)

    def test_new_objects_preserve_ground_and_compile_native(self):
        p=self.simple()
        p['map']['objects']=[dict(id='stall',prefab='stall-blue',x=1,y=2),dict(id='table',prefab='table-check',x=11,y=5)]
        saved=json.loads(json.dumps(p));check_project(saved)
        layers,_=flatten(saved)
        self.assertEqual(layers[0][2*16+1],'outside:385')
        self.assertIsNone(layers[1][4*16+2])  # Real empty center of the canopy.
        data,report=compile_project(saved,self.catalog)
        self.assertEqual(report['errors'],0,report['issues']);self.assertTrue(report['nativeVerified'])
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            self.assertEqual(json.loads(archive.read('project.stickmap.json')),saved)
            cpp=archive.read('AuthoredMap.h')
        with tempfile.TemporaryDirectory() as temp:
            target=Path(temp)
            (target/'AuthoredMap.h').write_bytes(cpp)
            (target/'main.cpp').write_text('#include "AuthoredMap.h"\n#include <cstdio>\nint main(){auto m=AuthoredMap::load();printf("%08x",ExploreMapGenerator::fingerprint(m));}\n')
            subprocess.run(['c++','-std=c++17','-I',str(ROOT/'src'),str(target/'main.cpp'),str(ROOT/'src/game/ExploreMapGenerator.cpp'),'-o',str(target/'test')],check=True,capture_output=True)
            self.assertEqual(subprocess.check_output([str(target/'test')]).decode(),report['fingerprint'])
        saved['map']['objects']=[]
        self.assertIsNone(flatten(saved)[0][1][2*16+1])
        self.assertEqual(flatten(saved)[0][0][2*16+1],'outside:385')

    def test_invalid_schema_and_future_version_rejected(self):
        for patch in [lambda p:p.update(schemaVersion=2),lambda p:p['map']['layers'][0].pop(),
                      lambda p:p['map'].update(seed='38'),lambda p:p['prefabs'].append(copy.deepcopy(p['prefabs'][0]))]:
            p=base(self.catalog);patch(p)
            with self.assertRaises(ValueError): check_project(p)

    def test_illegal_paths_and_obstruction_block_export(self):
        for patch,code in [(lambda p:p['map']['paths'][0]['points'].pop(3),'continuity'),
                (lambda p:p['map']['paths'][0]['exit'].update(edge='right'),'exit-edge'),
                (lambda p:p['map']['objects'].append(dict(id='tree',prefab='tree',x=7,y=3)),'footprint'),
                (lambda p:p['map']['paths'][0].update(interiorExit=True),'transition'),
                (lambda p:p['map']['layers'][1].__setitem__(0,'outside:4000'),'unsupported')]:
            p=self.simple();patch(p);out,report=compile_project(p,self.catalog)
            self.assertIsNone(out);self.assertIn(code,[i['code'] for i in report['issues']])

    def test_alias_identity_does_not_collapse_source(self):
        self.assertNotEqual(self.catalog.assets['outside:515'].get('runtimeId'),self.catalog.assets['runtime:4700']['runtimeId'])
        self.assertEqual(self.catalog.assets['runtime:4700']['provenance']['sheet'],'Caves.png')

    def test_prefab_bounds_overlap_and_anchor(self):
        p=self.simple();p['map']['objects']=[dict(id='a',prefab='stall-blue',x=15,y=10)]
        self.assertIn('bounds',[v['code'] for v in validate(p,self.catalog)['issues']])
        p['map']['objects']=[dict(id='a',prefab='stall-blue',x=1,y=1),dict(id='b',prefab='stall-blue',x=1,y=1)]
        self.assertNotIn('overlap',[v['code'] for v in validate(p,self.catalog)['issues']])
        layers,owners=flatten(p)
        self.assertEqual(owners[(1,1*16+1)],'b')
        self.assertEqual(layers[1][1*16+1],'outside:1384')
        p['map']['objects'].pop();next(v for v in p['prefabs'] if v['id']=='stall-blue')['anchor'][1]=2
        self.assertIn('anchor',[v['code'] for v in validate(p,self.catalog)['issues']])

    def test_unchanged_mist_snapshot_exports(self):
        for edge in ['top','right','bottom','left']:
            p=snapshot(self.catalog,dict(area=4,edge=edge,seed=38))
            out,report=compile_project(p,self.catalog)
            self.assertIsNotNone(out,report['issues'])
            self.assertEqual(report['fingerprint'],p['origin']['fingerprint'])
    def test_overlapping_complete_assemblies_export_visible_cells(self):
        p=self.simple();p['map']['objects']=[dict(id='table',prefab='table-check',x=1,y=2),
            dict(id='crate',prefab='market-crate',x=1,y=2)]
        layers,owners=flatten(p)
        self.assertEqual(owners[(1,2*16+1)],'crate')
        expected=next(pref for pref in p['prefabs'] if pref['id']=='market-crate')['cells'][0]['asset']
        self.assertEqual(layers[1][2*16+1],expected)
        data,report=compile_project(p,self.catalog)
        self.assertIsNotNone(data,report['issues'])
        self.assertNotIn('assembly',[v['code'] for v in report['issues']])
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            runtime=json.loads(archive.read('runtime-map.json'))
            self.assertEqual(runtime['layers'][1][2*16+1],self.catalog.assets[expected]['runtimeId'])

    def test_imported_trees_are_editable_without_changing_snapshot(self):
        p=snapshot(self.catalog,dict(area=4,seed=38))
        self.assertTrue(p['map']['objects'])
        before=flatten(p)[0]
        obj=p['map']['objects'].pop()
        after=flatten(p)[0]
        self.assertNotEqual(before[1],after[1])
        self.assertEqual(before[0],after[0])
        p['map']['objects'].append(obj)
        self.assertEqual(flatten(p)[0],before)

    def test_exploded_fragments_and_false_footprints_cannot_export(self):
        p=self.simple()
        p['map']['layers'][1][0]='outside:1384'
        self.assertIn('assembly',[v['code'] for v in validate(p,self.catalog)['issues']])
        p=self.simple();p['map']['objects']=[dict(id='tree',prefab='tree',x=1,y=2)]
        next(v for v in p['prefabs'] if v['id']=='tree')['footprint']=[]
        self.assertIn('footprint-semantics',[v['code'] for v in validate(p,self.catalog)['issues']])

    def test_source_changes_block_export(self):
        p=self.simple();p['sources']['runtime-art']['sha256']='stale'
        self.assertIn('source-change',[v['code'] for v in validate(p,self.catalog)['issues']])

    def test_manual_lake_prefab_keeps_authored_shoreline(self):
        p=self.simple();p['prefabs'].append(dict(id='manual-shore',name='manual shore',category='自定义',
            width=1,height=1,cells=[dict(x=0,y=0,layer=0,asset='lake:191')],
            anchor=[.5,1],anchorMode='runtime',footprint=[]))
        p['map']['objects']=[dict(id='shore',prefab='manual-shore',x=0,y=0)]
        p['map']['layers'][0][0]='runtime:5007'
        before=copy.deepcopy(p)
        self.assertFalse([i for i in validate(p,self.catalog)['issues'] if i['code'].startswith('lake-')])
        self.assertEqual(p,before)
        self.assertEqual(flatten(p)[0][0][0],'lake:191')

    def test_auto_lake_js_python_masks_and_native_export(self):
        script = '''import fs from 'node:fs';
import {editLake} from './tools/map_editor/lake.js';
const {project,spec}=JSON.parse(fs.readFileSync(0,'utf8'));const maps=[];
const offsets=[[0,-1],[1,0],[0,1],[-1,0],[-1,-1],[1,-1],[1,1],[-1,1]];
for(let mask=0;mask<256;mask++){
 const p=structuredClone(project),cells=[[3,4]];
 offsets.forEach(([dx,dy],bit)=>{if(mask&(1<<bit))cells.push([3+dx,4+dy]);});
 editLake(p,cells,false,spec);maps.push(p);
}
process.stdout.write(JSON.stringify(maps));'''
        result=subprocess.run(['node','--input-type=module','-e',script],cwd=ROOT,
            input=json.dumps(dict(project=self.simple(),spec=lake.descriptor())),text=True,capture_output=True,check=True)
        cases=json.loads(result.stdout)
        for mask,p in enumerate(cases):
            self.assertEqual(p['map']['layers'][0][4*16+3],f'lake:{lake.canonical(mask)}')
            self.assertEqual(validate(p,self.catalog)['errors'],0,(mask,validate(p,self.catalog)['issues']))
        out,report=compile_project(cases[-1],self.catalog)
        self.assertIsNotNone(out);self.assertTrue(report['nativeVerified'])
        corrupted=copy.deepcopy(cases[-1]);corrupted['map']['layers'][0][4*16+3]='lake:0'
        self.assertIn('lake-edge',[i['code'] for i in validate(corrupted,self.catalog)['issues']])
        for mask in lake.MASKS:
            a=self.catalog.assets[f'lake:{mask}']
            self.assertTrue(a['water']);self.assertEqual(a['frames'],8);self.assertEqual(a['depth'],[0,0,0])
        self.assertEqual((ROOT/'src/game/ExploreLakeTiles.h').read_text(),lake.cpp_table())

    def test_auto_lake_restoration_schema_and_road_retiling(self):
        p=self.simple()
        for value in [[],{'192':'outside:385'},{'-1':'outside:385'},{'01':'outside:385'},{'0':None}]:
            p['map']['lakeGround']=value
            with self.assertRaises(ValueError): check_project(p)
        p=self.simple();p['map']['layers'][0][7+3*16]='lake:0';p['map']['lakeGround']={'55':'outside:385'}
        self.assertIn('water',[i['code'] for i in validate(p,self.catalog)['issues']])
        p=pave(p,self.catalog)
        self.assertEqual(validate(p,self.catalog)['errors'],0)

if __name__=='__main__': unittest.main()
