"""Authored activity validation, portable simulation and export contracts."""
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/map_editor'))
from catalog import Catalog
from project import base,check_project,compile_project,pave
import npcs
import population
from npc_sprites import artwork, species
from generate_sprite_previews import parse_kind_names, load_sprite_pack, decode_frame

class NpcTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.catalog=Catalog()
    def project(self):
        return base(self.catalog)
    def actor(self,id='one',spawn=None,scope='map',behavior='wander'):
        return dict(id=id,name=id,sprite='pokemon-025',spawn=spawn or [3,3],scope=scope,zoneIds=[],behavior=behavior,
                    direction=0,stepMs=400,waitMinMs=100,waitMaxMs=300,patrol=[])
    def test_old_project_unchanged(self):
        p=self.project();before=copy.deepcopy(p);check_project(p);self.assertEqual(npcs.diagnostics(p,self.catalog)[0],[])
        self.assertEqual(npcs.preview(p,self.catalog,2)['frames'],[[],[],[]]);self.assertEqual(p,before)
    def test_species_catalog_and_original_pixels(self):
        entries=npcs.sprite_catalog()
        self.assertEqual({s['id'] for s in entries},set(species()))
        self.assertTrue(all('speciesId' in s and not s.get('legacy') for s in entries))
        names=parse_kind_names((ROOT/'src/assets/PokemonSprites.h').read_text())
        for meta in entries:
            key=meta['id'];payload=npcs.sprite_path(key).read_bytes()
            self.assertEqual(hashlib.sha256(payload).hexdigest(),meta['sha256'])
            self.assertEqual(hashlib.sha256(npcs.sprite_png(key)).hexdigest(),meta['atlasSha256'])
            _,frames,rle,palettes=load_sprite_pack(npcs.sprite_path(key),names)
            originals={f['kindName']:f for f in frames}
            with Image.open(io.BytesIO(npcs.sprite_png(key))) as atlas:
                for action,directions in meta['actions'].items():
                    self.assertEqual(len(directions),4)
                    for group in directions:
                        self.assertTrue(group)
                        for f in group:
                            original=originals[f['kind']]
                            self.assertEqual(f['groundOffsetY'],original['groundOffsetY'])
                            expected=decode_frame(original,rle,palettes)
                            if f['mirrored']:expected=expected.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
                            crop=atlas.crop((f['x'],f['y'],f['x']+f['width'],f['y']+f['height']))
                            self.assertEqual(crop.tobytes(),expected.tobytes(),(key,action,f['kind']))
        self.assertEqual(len(artwork('pokemon-133')[0]['actions']['idle'][0]),2)
        self.assertEqual(len(artwork('pokemon-134')[0]['actions']['walking'][0]),4)
    def test_legacy_actor_is_preserved(self):
        p=self.project();n=self.actor();p['map']['npcs']=[n]
        modern=npcs.preview(p,self.catalog,20)['frames']
        n['sprite']='youngster';before=copy.deepcopy(p)
        check_project(p)
        self.assertEqual(npcs.preview(p,self.catalog,20)['frames'],modern)
        self.assertEqual(p,before)
        self.assertTrue(artwork('youngster')[0]['legacy'])
    def test_schema_rejects_bad_references_and_bools(self):
        p=self.project();p['map']['npcs']=[self.actor()]
        for key,value in [('direction',True),('stepMs',0),('waitMinMs',350),('zoneIds',['missing']),('patrol',[[16,0]]),('sprite','../../secret')]:
            q=copy.deepcopy(p);q['map']['npcs'][0][key]=value
            with self.assertRaises(ValueError):check_project(q)
        q=copy.deepcopy(p);q['map']['npcs']*=2
        with self.assertRaises(ValueError):check_project(q)
    def test_zone_membership_and_snapshot_immutability(self):
        p=self.project();m=p['map'];m['activityZones']=[dict(id='a',name='area',color='#a9ce90',cells=[[x,y] for x in range(2,6) for y in range(2,6)])]
        n=self.actor(scope='zones');n['zoneIds']=['a'];m['npcs']=[n];before=copy.deepcopy(p)
        frames=npcs.preview(p,self.catalog,1200)['frames'];self.assertEqual(p,before)
        positions={(row[0][0],row[0][1]) for row in frames};self.assertGreater(len(positions),10)
        self.assertTrue(all(2.5<=x<=5.5 and 2.5<=y<=5.5 for x,y in positions))
        self.assertEqual(frames,npcs.preview(p,self.catalog,1200)['frames'])
    def test_fixed_free_patrol_and_obstacle_detour(self):
        p=self.project();m=p['map'];m['layers'][0][3*16+4]='runtime:48'
        n=self.actor(behavior='patrol');n['patrol']=[[3,3],[5,3]];m['npcs']=[n,self.actor('idle',[8,8],behavior='idle')]
        frames=npcs.preview(p,self.catalog,400)['frames']
        self.assertTrue(any(row[0][:2]==[5.5,3.5] for row in frames))
        self.assertTrue(all(row[1][:2]==[8.5,8.5] and not row[1][4] for row in frames))
        self.assertFalse(any(int(row[0][0])==4 and int(row[0][1])==3 for row in frames))
    def test_footprints_water_and_blocked_destinations(self):
        p=self.project();m=p['map'];m['objects']=[dict(id='tree',prefab='tree',x=3,y=1)]
        grid=npcs.walkable(p,self.catalog);self.assertTrue(grid[1*16+3]);self.assertFalse(grid[3*16+3])
        m['npcs']=[self.actor()]
        self.assertIn('npc-spawn',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])
        with self.assertRaises(ValueError):npcs.preview(p,self.catalog,1)
        m['objects']=[];m['npcs'][0].update(behavior='patrol',patrol=[[3,3],[4,3]])
        m['layers'][0][3*16+4]='runtime:48'
        self.assertIn('npc-unreachable',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])
    def test_covered_footprints_follow_the_visible_object(self):
        p=self.project();m=p['map']
        m['objects']=[dict(id='under',prefab='tree',x=3,y=1),dict(id='over',prefab='tree',x=3,y=3)]
        grid=npcs.walkable(p,self.catalog)
        self.assertTrue(grid[3*16+3]);self.assertFalse(grid[5*16+3])
        m['npcs']=[self.actor(spawn=[3,3],behavior='idle')]
        self.assertFalse(any(i['severity']=='error' for i in npcs.diagnostics(p,self.catalog)[0]))
        m['objects'].reverse()
        self.assertFalse(npcs.walkable(p,self.catalog)[3*16+3])
    def test_disconnected_multi_zone_and_overlap_validation(self):
        p=self.project();m=p['map'];m['activityZones']=[dict(id=id,name=id,color='#aabbcc',cells=cells) for id,cells in [('a',[[3,3],[3,4]]),('b',[[8,8],[8,9]])]]
        n=self.actor(scope='zones');n['zoneIds']=['a','b'];m['npcs']=[n]
        self.assertIn('npc-zone-unreachable',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])
        m['npcs']=[self.actor('a'),self.actor('b')]
        self.assertIn('npc-overlap',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])
    def test_capacity_and_no_actor_collisions(self):
        p=self.project();m=p['map'];m['npcs']=[self.actor(str(i),[2+i%4,2+i//4]) for i in range(16)]
        frames=npcs.preview(p,self.catalog,1200)['frames']
        for frame in frames:
            positions=[(int(a[0]),int(a[1])) for a in frame]
            self.assertEqual(len(set(positions)),16)
            self.assertTrue(all(0<=a[0]<16 and 0<=a[1]<12 for a in frame))
    def visitor_zone(self,id='visitors',cells=None):
        return dict(id=id,name=id,color='#aabbcc',cells=cells or [[3,9],[3,10]],visitors=dict(enabled=True,
            families=['025-172','184-298'],intervalMinMs=1000,intervalMaxMs=2000,stayMinMs=1000,stayMaxMs=2000))
    def test_visitor_evolution_pool_schema_and_capacity(self):
        pairs={f['id']:f for f in population.families()}
        for key in ['025-172','026-172','184-298','212-123','282-280','135-133','197-133']:
            self.assertIn(key,pairs)
        self.assertFalse(any({f['adult'],f['juvenile']}&{151,380,381} for f in pairs.values()))
        p=self.project();p['map']['activityZones']=[self.visitor_zone()]
        for key,value in [('enabled',1),('families',['151-172']),('families',['025-172']*2),('families',[]),
                          ('stayMinMs',True),('intervalMaxMs',999),('stayMaxMs',500)]:
            q=copy.deepcopy(p);q['map']['activityZones'][0]['visitors'][key]=value
            with self.assertRaises(ValueError):check_project(q)
        p['map']['npcs']=[self.actor(str(i),[i,3]) for i in range(15)]
        with self.assertRaises(ValueError):check_project(p)
    def test_visitor_round_trips_following_and_random_seed(self):
        p=self.project();m=p['map'];m['activityZones']=[self.visitor_zone()]
        # Force a detour around a pond between the port and the stall.
        for y in range(7,11):m['layers'][0][y*16+5]='runtime:48'
        before=copy.deepcopy(p);data=npcs.preview(p,self.catalog);frames=data['frames']
        self.assertEqual(p,before);self.assertEqual(len(data['actors']),2)
        self.assertTrue(all(a.get('visitor') for a in data['actors']))
        arrivals=[i for i in range(1,len(frames)) if frames[i][0][5] and not frames[i-1][0][5]]
        self.assertGreaterEqual(len(arrivals),3)
        self.assertTrue(any(not row[0][5] and not row[1][5] for row in frames[100:]))
        self.assertTrue(any(all(a[5] and 3<=a[0]<4 and 9<=a[1]<11 and not a[4] for a in row) for row in frames))
        grid=npcs.walkable(p,self.catalog)
        selected={(25,172),(184,298)}
        for row in frames:
            active=[a for a in row if a[5]]
            if len(active)==2:
                self.assertIn(tuple(a[6] for a in row),selected)
                self.assertNotEqual(row[0][:2],row[1][:2])
                inside=all(0<=a[0]<16 and 0<=a[1]<12 for a in row)
                self.assertLessEqual(abs(row[0][0]-row[1][0])+abs(row[0][1]-row[1][1]),2.01 if inside else 4.01)
            for a in active:
                if 0<=a[0]<16 and 0<=a[1]<12:self.assertTrue(grid[int(a[1])*16+int(a[0])])
                else:self.assertEqual(a[0],7.5) # Only the authored bottom port.
        self.assertEqual(frames,npcs.preview(p,self.catalog)['frames'])
        self.assertNotEqual(frames,npcs.preview(p,self.catalog,seed=1234)['frames'])
        for i in range(len(frames)-1):
            for a,b in zip(frames[i],frames[i+1]):
                if a[5] and b[5]:self.assertLessEqual(abs(a[0]-b[0])+abs(a[1]-b[1]),.167)
    def test_visitor_unreachable_disconnected_and_ports(self):
        p=self.project();m=p['map'];m['activityZones']=[self.visitor_zone(cells=[[3,3],[9,9]])]
        self.assertIn('visitors-disconnected',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])
        m['activityZones'][0]['cells']=[[3,3],[3,4]]
        for x in range(16):m['layers'][0][8*16+x]='runtime:48'
        with self.assertRaisesRegex(ValueError,'无法从地图出入口到达'):npcs.preview(p,self.catalog)
        m['entry']['point']=[7,9]
        self.assertIn('visitors-port',[i['code'] for i in npcs.diagnostics(p,self.catalog)[0]])

    def test_solo_visitor_uses_one_cell_and_exports_no_zero_species(self):
        p=self.project();z=self.visitor_zone(cells=[[3,9]])
        z['visitors']['families']=['solo-225'];p['map']['activityZones']=[z]
        check_project(p)
        for seed in (1,17,38,99):
            frames=npcs.preview(p,self.catalog,1200,seed)['frames']
            arrivals=[i for i in range(1,len(frames)) if frames[i][0][5] and not frames[i-1][0][5]]
            self.assertGreaterEqual(len(arrivals),2)
            self.assertTrue(any(row[0][5] and not row[0][4] and 3<=row[0][0]<4 and 9<=row[0][1]<10 for row in frames))
            self.assertTrue(all(not row[1][5] and row[1][6]==0 for row in frames))
        p['map']['paths']=[dict(points=[[7,y] for y in range(11,-1,-1)],exit=dict(point=[7,0],edge='top'),cellCentered=False,
            fallsToNextLevel=False,nextValleyNode=255,nextValleyEntrance=0,terminal=False,interiorExit=False)]
        p=pave(p,self.catalog)
        blob,report=compile_project(p,self.catalog)
        self.assertIsNotNone(blob,report['issues'])
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            self.assertIn('npc-packs/225.smonsp',z.namelist())
            self.assertNotIn('npc-packs/000.smonsp',z.namelist())
        candidates=population.families()
        self.assertLessEqual(len(candidates),64)
        self.assertTrue({'solo-113','solo-115','solo-225','solo-352'}<={f['id'] for f in candidates})
    def test_multiple_visitor_zones_share_port_and_manual_obstacles(self):
        p=self.project();m=p['map'];m['activityZones']=[self.visitor_zone('left'),self.visitor_zone('right',[[11,9],[11,10]])]
        m['npcs']=[self.actor('stationary',[7,10],behavior='idle')]
        frames=npcs.preview(p,self.catalog)['frames']
        for member in (1,3):
            self.assertGreaterEqual(sum(row[member][5] and not frames[i-1][member][5] for i,row in enumerate(frames) if i),2)
        for row in frames:
            cells=[(int(a[0]),int(a[1])) for a in row if a[5] and 0<=a[0]<16 and 0<=a[1]<12]
            self.assertEqual(len(cells),len(set(cells)))
            self.assertEqual(row[0][:2],[7.5,10.5])
    def test_export_roundtrip_and_compiled_header(self):
        p=self.project();m=p['map'];m['paths']=[dict(points=[[7,y] for y in range(11,-1,-1)],exit=dict(point=[7,0],edge='top'),cellCentered=False,
            fallsToNextLevel=False,nextValleyNode=255,nextValleyEntrance=0,terminal=False,interiorExit=False)]
        p=pave(p,self.catalog);legacy=self.actor('legacy',[9,9]);legacy['sprite']='youngster'
        p['map']['npcs']=[self.actor(),legacy]
        p['map']['activityZones']=[self.visitor_zone()]
        check_project(json.loads(json.dumps(p)));data,report=compile_project(p,self.catalog)
        self.assertIsNotNone(data,report['issues'])
        with zipfile.ZipFile(io.BytesIO(data)) as z, tempfile.TemporaryDirectory() as temp:
            self.assertEqual(json.loads(z.read('project.stickmap.json')),p)
            self.assertEqual(json.loads(z.read('activity.json'))['npcs'],p['map']['npcs'])
            for filename in ('AuthoredNpc.h','AuthoredNpcs.h','AuthoredNpcPopulation.h','AuthoredPopulations.h'):Path(temp,filename).write_bytes(z.read(filename))
            self.assertEqual(json.loads(z.read('activity.json'))['zones'],p['map']['activityZones'])
            self.assertEqual({f['id'] for f in json.loads(z.read('npc-families.json'))},{'025-172','184-298'})
            self.assertEqual(z.read('npc-art/youngster.png'),npcs.sprite_path('youngster').read_bytes())
            self.assertEqual(z.read('npc-art/pokemon-025.png'),npcs.sprite_png('pokemon-025'))
            self.assertEqual(z.read('npc-packs/025.smonsp'),npcs.sprite_path('pokemon-025').read_bytes())
            sources={s['id']:s for s in json.loads(z.read('npc-sources.json'))}
            self.assertEqual(set(sources),{'pokemon-025','pokemon-172','pokemon-184','pokemon-298','youngster'})
            self.assertEqual(z.read('npc-packs/298.smonsp'),npcs.sprite_path('pokemon-298').read_bytes())
            self.assertEqual(sources['pokemon-025']['speciesId'],25)
            self.assertEqual(hashlib.sha256(z.read('npc-art/pokemon-025.png')).hexdigest(),sources['pokemon-025']['atlasSha256'])
            src=Path(temp,'test.cpp');src.write_text('#include "AuthoredNpcs.h"\nint main(){AuthoredNpc::Definition d[AuthoredNpc::CAPACITY];AuthoredNpcs::load(d);AuthoredNpc::Simulation s;s.reset(d,AuthoredNpcs::COUNT);for(int i=0;i<1200;i++)s.tick();return s.count()!=2;}')
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(src),'-o',str(Path(temp,'test'))],check=True,capture_output=True)
            subprocess.run([str(Path(temp,'test'))],check=True)
            src.write_text('#include "AuthoredNpcs.h"\n#include "AuthoredPopulations.h"\nint main(){AuthoredNpc::Definition d[AuthoredNpc::CAPACITY];AuthoredNpcPopulation::Rule r[AuthoredNpcPopulation::RULE_CAPACITY];AuthoredNpcs::load(d);AuthoredPopulations::load(r);AuthoredNpcPopulation::Simulation s;s.reset(d,AuthoredNpcs::COUNT,r,AuthoredPopulations::COUNT,38);bool seen=false,departed=false;for(int i=0;i<1200;i++){s.tick();if(s.pose(2).visible)seen=true;else if(seen)departed=true;}return !departed;}')
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(src),'-o',str(Path(temp,'visitors'))],check=True,capture_output=True)
            subprocess.run([str(Path(temp,'visitors'))],check=True)

if __name__=='__main__':unittest.main()
