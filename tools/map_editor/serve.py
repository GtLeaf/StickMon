#!/usr/bin/env python3
"""Serve the local map editor. Writes only user drafts in .stick-local."""
import argparse
import json
import secrets
import subprocess
import sys
import threading
import webbrowser
from functools import partial
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

try:
    from catalog import Catalog, ROOT, LOCAL, SHEETS, sheet, png
    from project import base, snapshot, validate, check_project, compile_project, render_project
    import npcs
except ModuleNotFoundError as error:
    if error.name == 'PIL': raise SystemExit('Pillow is required: python3 -m pip install Pillow') from error
    raise

TOOL = Path(__file__).resolve().parent
MAX_BODY = 4*1024*1024
STATIC = {'/':'index.html','/styles.css':'styles.css','/app.js':'app.js','/model.js':'model.js','/library.js':'library.js','/resize.js':'resize.js','/lake.js':'lake.js','/rotation.js':'rotation.js','/stacking.js':'stacking.js','/panels.js':'panels.js','/npc-editor.js':'npc-editor.js','/road-network.js':'road-network.js'}


def sprite():
    sys.path.insert(0,str(ROOT/'tools/room_editor'))
    from generate_sprite_previews import load_sprite_pack,parse_kind_names,choose_preview_frame,decode_frame
    names=parse_kind_names((ROOT/'src/assets/PokemonSprites.h').read_text())
    _,frames,rle,palettes=load_sprite_pack(ROOT/'data/packs/dev/sprites/025.smonsp',names)
    frame=choose_preview_frame(frames)
    return png(decode_frame(frame,rle,palettes)),dict(width=frame['width'],height=frame['height'],groundOffsetY=frame['groundOffsetY'])


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(30)

    def respond(self,body,content='application/json; charset=utf-8',status=200):
        if isinstance(body,(dict,list)): body=json.dumps(body,ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header('Content-Type',content)
        self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Security-Policy',"default-src 'self'; img-src 'self' blob: data:; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'")
        self.end_headers()
        self.wfile.write(body)

    def safe_host(self):
        return self.headers.get('Host') in (f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}')

    def do_GET(self):
        if not self.safe_host(): return self.respond({'error':'Invalid host'},status=403)
        path=urlsplit(self.path).path
        try:
            if path in STATIC:
                file=TOOL/STATIC[path]
                content={'html':'text/html','css':'text/css','js':'text/javascript'}[file.suffix[1:]]
                return self.respond(file.read_bytes(),content+'; charset=utf-8')
            if path=='/api/catalog':
                return self.respond({**self.server.catalog.payload(),'token':self.server.token,'sprite':self.server.sprite_meta,'npcSprites':npcs.sprite_catalog(include_legacy=True),'npcFamilies':npcs.population.families()})
            if path=='/api/blank': return self.respond(base(self.server.catalog))
            if path=='/api/drafts':
                items=[]
                for file in self.server.drafts.glob('*.stickmap.json'):
                    items.append(dict(file=file.name,name=json.loads(file.read_text())['name'],modified=file.stat().st_mtime))
                return self.respond(sorted(items,key=lambda x:x['modified'],reverse=True))
            if path.startswith('/api/draft/'):
                name=path.removeprefix('/api/draft/')
                if '/' in name or not name.endswith('.stickmap.json') or name.startswith('.'): raise ValueError('Invalid draft name')
                return self.respond((self.server.drafts/name).read_bytes())
            if path=='/art/runtime.png': return self.respond(self.server.catalog.atlas_png,'image/png')
            if path=='/art/market-chat.png': return self.respond((ROOT/'origin_asset/icon/market/chat.png').read_bytes(),'image/png')
            if path=='/art/sprite.png': return self.respond(self.server.sprite_png,'image/png')
            if path.startswith('/art/npc-') and path.endswith('.png'):
                return self.respond(npcs.sprite_png(path[9:-4]),'image/png')
            if path.startswith('/art/') and path[5:-4] in SHEETS and path.endswith('.png'):
                return self.respond(png(sheet(path[5:-4])),'image/png')
            self.respond({'error':'Not found'},status=404)
        except (ValueError,OSError,KeyError) as error: self.respond({'error':str(error)},status=422)

    def do_POST(self):
        origin=self.headers.get('Origin')
        if not self.safe_host() or self.headers.get('X-Editor-Token')!=self.server.token or (origin and origin not in
                (f'http://127.0.0.1:{self.server.server_port}',f'http://localhost:{self.server.server_port}')):
            return self.respond({'error':'Invalid local editor request'},status=403)
        try:
            length=int(self.headers.get('Content-Length','0'))
            if not 0<length<=MAX_BODY: raise ValueError('工程文件过大或请求为空')
            data=json.loads(self.rfile.read(length))
            path=urlsplit(self.path).path
            with self.server.work_lock:
                if path=='/api/snapshot': return self.respond(snapshot(self.server.catalog,data))
                if path=='/api/check': return self.respond(check_project(data,self.server.catalog))
                if path=='/api/npc-check':
                    check_project(data,self.server.catalog)
                    issues,grid=npcs.diagnostics(data,self.server.catalog)
                    return self.respond(dict(issues=issues,walkable=grid))
                if path=='/api/npc-preview':
                    check_project(data,self.server.catalog)
                    return self.respond(npcs.preview(data,self.server.catalog,seed=secrets.randbits(32)))
                if path=='/api/validate': return self.respond(validate(data,self.server.catalog))
                if path=='/api/save':
                    check_project(data,self.server.catalog)
                    # Stable opaque IDs avoid trusting file paths supplied by a project.
                    name=data.get('draftId','')
                    if not isinstance(name,str) or len(name)!=24 or any(c not in '0123456789abcdef' for c in name): name=secrets.token_hex(12)
                    data['draftId']=name
                    target=self.server.drafts/(name+'.stickmap.json')
                    temp=target.with_suffix('.tmp')
                    temp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
                    temp.replace(target)
                    return self.respond(dict(draftId=name,file=target.name))
                if path=='/api/png': return self.respond(png(render_project(data,self.server.catalog)),'image/png')
                if path=='/api/native-preview':
                    from native_preview import render_zip
                    return self.respond(render_zip(data,self.server.catalog),'application/zip')
                if path=='/api/compile':
                    result,report=compile_project(data,self.server.catalog)
                    if result is None: return self.respond(report,status=422)
                    return self.respond(result,'application/zip')
                self.respond({'error':'Not found'},status=404)
        except (ValueError,TypeError,KeyError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as error:
            self.respond({'error':str(error)},status=422)


def create_server(port=8767):
    catalog=Catalog()
    server=ThreadingHTTPServer(('127.0.0.1',port),Handler)
    server.catalog=catalog;server.token=secrets.token_urlsafe(32)
    server.drafts=ROOT/'.stick-local/map-editor/projects'
    server.drafts.mkdir(parents=True,exist_ok=True)
    server.work_lock=threading.Lock()
    server.sprite_png,server.sprite_meta=sprite()
    return server


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=8767)
    parser.add_argument('--no-open',action='store_true')
    args=parser.parse_args()
    print('Loading original artwork and runtime semantics…',flush=True)
    server=create_server(args.port)
    url=f'http://127.0.0.1:{server.server_port}'
    print(f'StickMon Map Editor: {url}',flush=True)
    if not args.no_open: webbrowser.open(url)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close()


if __name__=='__main__': main()
