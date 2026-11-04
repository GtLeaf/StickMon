"""Large map integration: actual frontend operations, storage, rendering and native AI."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.request
from http.server import ThreadingHTTPServer
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/map_editor'))
from catalog import Catalog
from project import base,check_project,flatten,validate,render_project,to_runtime,compile_project
import npcs
import population
from market_export import compile_market

class SizeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.catalog=Catalog()
    def frontend(self,p,operations='',size=(32,24)):
        script='''import fs from 'node:fs';
import {resizeMap} from './tools/map_editor/resize.js';
import {recognizeRoads,storeRoadRoute} from './tools/map_editor/road-network.js';
import {editLake,lakeSeed} from './tools/map_editor/lake.js';
const {p,catalog,size}=JSON.parse(fs.readFileSync(0,'utf8'));
resizeMap(p,size);
'''+operations+"\nprocess.stdout.write(JSON.stringify(p));"
        import lake_autotile
        catalog=dict(assets=self.catalog.assets,lake=lake_autotile.descriptor())
        result=subprocess.run(['node','--input-type=module','-e',script],cwd=ROOT,input=json.dumps(dict(p=p,catalog=catalog,size=size)),text=True,capture_output=True,check=True)
        return json.loads(result.stdout)
    def actor(self,**extra):
        return dict(dict(id='large-npc',name='远处精灵',sprite='pokemon-025',spawn=[30,22],direction=0,
                    behavior='patrol',scope='map',zoneIds=[],stepMs=200,waitMinMs=100,waitMaxMs=100,patrol=[[30,22],[31,23]]),**extra)
    def large(self):return self.frontend(base(self.catalog))
    def road(self,dense=False):
        return self.frontend(base(self.catalog),'''
p.map.entry={edge:'right',point:[31,20]};p.map.junction=[31,20];
for(let y='''+('0' if dense else '20')+''';y<'''+('24' if dense else '22')+''';y++)for(let x=0;x<32;x++)p.map.layers[0][y*32+x]='outside:546';
p.map.roadNetwork=recognizeRoads(p,catalog);storeRoadRoute(p);
''')
    def test_frontend_regressions(self):
        subprocess.run(['node','tools/map_editor/test_size.mjs'],cwd=ROOT,check=True)
    def test_schema_rejects_wrong_lengths_and_coordinates(self):
        p=self.large();self.assertEqual(check_project(p),p)
        for field,value in [('size',[65,48]),('size',[32.0,24]),('size',[15,12]),('size',[16,11]),('size',[16,65]),('size',[32]),('size',None),('layers',[[None]*192]*3),('junction',[32,23]),('entry',dict(edge='bottom',point=[31,24]))]:
            q=copy.deepcopy(p);q['map'][field]=value
            with self.assertRaises(ValueError,msg=field):check_project(q)
    def test_large_lake_object_flatten_and_png(self):
        p=self.frontend(base(self.catalog),"editLake(p,lakeSeed(p,31,23,catalog.lake),false,catalog.lake);")
        p['map']['objects']=[dict(id='tree-at-edge',prefab='tree',x=26,y=18)]
        check_project(p)
        layers,owners=flatten(p)
        self.assertTrue(any(cell>255 for z,cell in owners))
        self.assertEqual(len(layers[0]),768)
        self.assertFalse([i for i in validate(p,self.catalog)['issues'] if i['code'].startswith('lake-')])
        image=render_project(p,self.catalog);self.assertEqual(image.size,(1024,768))
        tile=self.catalog.tile(layers[0][767]);expected=Image.new('RGBA',(32,32),(0,0,0,255));expected.alpha_composite(tile)
        self.assertEqual(image.crop((992,736,1024,768)).tobytes(),expected.tobytes())
        self.assertEqual(render_project(p,self.catalog,26).size,(832,624))
    def test_dense_road_roundtrip_exceeds_old_cap(self):
        p=self.road(True);check_project(p)
        self.assertGreater(len(p['map']['paths'][0]['points']),1025)
        self.assertEqual(check_project(json.loads(json.dumps(p))),p)
        self.assertEqual(validate(p,self.catalog)['errors'],0)
        for export in (to_runtime,compile_project,compile_market):
            with self.assertRaisesRegex(ValueError,'大地图'):export(p,self.catalog)
    def test_generic_native_activity_above_255_and_no_row_wrap(self):
        p=self.large();p['map']['npcs']=[self.actor()];before=copy.deepcopy(p)
        frames=npcs.preview(p,self.catalog,120)['frames'];self.assertEqual(frames[0][0][:2],[30.5,22.5])
        self.assertTrue(any(f[0][:2]==[31.5,23.5] for f in frames))
        self.assertTrue(all(30.5<=f[0][0]<=31.5 and 22.5<=f[0][1]<=23.5 for f in frames))
        self.assertEqual(p,before)
        mask=[False]*768;mask[31]=mask[32]=True
        self.assertEqual(npcs.reachable(31,mask,32),{31})
    def test_large_zone_and_random_visitors(self):
        p=self.large();m=p['map'];m['entry']=dict(point=[30,23],edge='bottom');m['junction']=[30,23]
        m['activityZones']=[dict(id='far-zone',name='远处摊位',color='#a9ce90',cells=[[x,y] for y in range(19,22) for x in range(28,32)],
            visitors=dict(enabled=True,families=['025-172'],intervalMinMs=1000,intervalMaxMs=1000,stayMinMs=1000,stayMaxMs=1000))]
        issues,grid=npcs.diagnostics(p,self.catalog);self.assertFalse(issues)
        rules=population.definitions(p,grid);self.assertEqual(rules[0]['ports'][0]['cell'],766)
        frames=npcs.preview(p,self.catalog,1200,38)['frames']
        self.assertTrue(any(f[0][5] and f[1][5] and 19<=f[0][1]<=22 and f[0][0]>27 for f in frames))
        self.assertTrue(any(not f[0][5] and not f[1][5] for f in frames[300:]))
        m['activityZones'][0].pop('visitors');m['npcs']=[self.actor(scope='zones',zoneIds=['far-zone'],behavior='wander',spawn=[30,20],patrol=[])]
        frames=npcs.preview(p,self.catalog,240)['frames'];self.assertTrue(all(28.5<=f[0][0]<=31.5 and 19.5<=f[0][1]<=21.5 for f in frames))
    def test_large_market_uses_shared_navigation_core(self):
        p=self.road();m=p['map'];m['npcs']=[self.actor(spawn=[25,20],patrol=[[25,20],[28,20]])]
        m['activityZones']=[dict(id='stalls',name='道路摊位',color='#a9ce90',cells=[[24,21],[25,21]],
            visitors=dict(enabled=True,families=['025-172'],intervalMinMs=1000,intervalMaxMs=1000,stayMinMs=1000,stayMaxMs=1000))]
        before=copy.deepcopy(p);a=npcs.preview(p,self.catalog,1200,38)
        self.assertEqual(a['navigationVersion'],2);self.assertEqual(a['frames'][0][0][:2],[25.5,20.5]);self.assertEqual(p,before)
        self.assertTrue(any(f[0][0]>27 for f in a['frames']))
        self.assertTrue(any(f[1][5] and f[1][0]>16 and 19<=f[1][1]<=22 for f in a['frames']))
        self.assertEqual(a['frames'],npcs.preview(p,self.catalog,1200,38)['frames'])
    def test_custom_size_native_and_render(self):
        for width,height in [(17,13),(25,19),(64,64)]:
            with self.subTest(size=(width,height)):
                p=self.frontend(base(self.catalog),size=(width,height))
                p['map']['npcs']=[self.actor(spawn=[width-2,height-2],patrol=[[width-2,height-2],[width-1,height-1]])]
                check_project(p)
                self.assertEqual(render_project(p,self.catalog).size,(width*32,height*32))
                frames=npcs.preview(p,self.catalog,80)['frames']
                self.assertTrue(any(f[0][:2]==[width-.5,height-.5] for f in frames))

    def test_custom_market_navigation_and_source_dimensions(self):
        from project import road_source_matches
        for width,height in [(25,19),(64,64)]:
            p=self.frontend(base(self.catalog),"""
const [w,h]=p.map.size;
p.map.entry={edge:'right',point:[w-1,h-2]};p.map.junction=[w-1,h-2];
for(let y=h-2;y<h;y++)for(let x=0;x<w;x++)p.map.layers[0][y*w+x]='outside:546';
p.map.roadNetwork=recognizeRoads(p,catalog);storeRoadRoute(p);
""",size=(width,height))
            p['map']['npcs']=[self.actor(spawn=[width-4,height-2],patrol=[[width-4,height-2],[width-2,height-2]])]
            check_project(p);self.assertEqual(validate(p,self.catalog)['errors'],0)
            frames=npcs.preview(p,self.catalog,80,38)['frames']
            self.assertTrue(any(f[0][0]>=width-1.5 for f in frames))
            self.assertTrue(road_source_matches(p,p['map']['roadNetwork']['source']))
            q=copy.deepcopy(p);q['map']['size']=[height,width]
            if width!=height:self.assertFalse(road_source_matches(q,p['map']['roadNetwork']['source']))

    def test_http_save_reopen_and_png(self):
        spec=importlib.util.spec_from_file_location('size_editor_http',ROOT/'tools/map_editor/serve.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        p=self.frontend(base(self.catalog),size=(29,17));p['map']['npcs']=[self.actor(spawn=[27,15],patrol=[[27,15],[28,16]])]
        with tempfile.TemporaryDirectory() as temp:
            server=ThreadingHTTPServer(('127.0.0.1',0),module.Handler);server.drafts=Path(temp);server.catalog=self.catalog;server.token='size-test';server.work_lock=threading.Lock()
            worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
            url=f'http://127.0.0.1:{server.server_port}'
            def request(path,value=None):
                req=urllib.request.Request(url+path,json.dumps(value).encode() if value else None,headers={'X-Editor-Token':server.token,'Content-Type':'application/json'})
                with urllib.request.urlopen(req) as response:return response.read()
            try:
                self.assertIn(b'export function resizeMap',request('/resize.js'))
                saved=json.loads(request('/api/save',p));reopened=json.loads(request('/api/draft/'+saved['file']))
                p['draftId']=saved['draftId'];self.assertEqual(reopened,p)
                self.assertEqual(Image.open(io.BytesIO(request('/api/png',p))).size,(928,544))
                self.assertEqual(len(list(Path(temp).glob('*.stickmap.json'))),1)
                with self.assertRaises(urllib.error.HTTPError) as error:request('/api/compile',p)
                self.assertEqual(error.exception.code,422)
                error.exception.close()
            finally:server.shutdown();server.server_close();worker.join()

if __name__=='__main__':unittest.main()
