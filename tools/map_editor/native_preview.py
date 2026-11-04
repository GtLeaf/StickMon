"""Render an edited map using production assets and the AMOLED route renderer."""
import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
from PIL import Image
from catalog import ROOT,LOCAL,Catalog
from project import validate,to_runtime,wire,require_device_size
sys.path.insert(0,str(ROOT/'tools'))
from amoled_host import build_amoled_host
from amoled_source import amoled_runtime_sources
from map_editor_assets import TILES as EDITOR_TILES
from lake_autotile import MASKS as LAKE_MASKS
from map_editor_rotations import TILES as ROTATED_TILES


def binary():
    target=LOCAL/'map-editor-render'
    dependencies=[Path(__file__).with_name('render_host.cpp'),Path(__file__).with_name('wire.h'),*amoled_runtime_sources(ROOT)]
    # Header edits also invalidate the host. Only local build output is written.
    dependencies += list((ROOT/'src').rglob('*.h')) + list((ROOT/'firmware/amoled_1_8_v1/main').rglob('*.h'))
    if not target.exists() or any(Path(p).stat().st_mtime>target.stat().st_mtime for p in dependencies):
        build_amoled_host(target,[Path(__file__).with_name('render_host.cpp')])
    return target


def render(p,catalog,output):
    require_device_size(p)
    if any(path.get('kind')=='roadNetwork' for path in p['map']['paths']):
        from market_export import render_native
        return render_native(p,catalog,output)
    report=validate(p,catalog)
    if report['errors']: raise ValueError(f'地图有 {report["errors"]} 项错误，请先完成校验')
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='native-',dir=output) as temp:
        subprocess.run([str(binary()),str(ROOT/'data'),temp],input=wire(to_runtime(p,catalog)),text=True,
                       capture_output=True,check=True,timeout=60)
        files=[]
        for path in sorted(Path(temp).glob('*.ppm')):
            target=output/(path.stem+'.png')
            with Image.open(path) as image: image.save(target)
            files.append(target)
        animation=sorted(output.glob('native-animation-*.png'))
        if animation and any(p.stem.startswith('native-animation-') for p in files):
            frames=[Image.open(p).convert('RGB') for p in animation]
            target=output/'native-animation.gif'
            frames[0].save(target,save_all=True,append_images=frames[1:],duration=280,loop=0,disposal=2)
            files.append(target)
    return files


def render_zip(p,catalog):
    with tempfile.TemporaryDirectory(prefix='preview-',dir=LOCAL) as temp:
        files=render(p,catalog,temp)
        out=io.BytesIO()
        if any(path.get('kind')=='roadNetwork' for path in p['map']['paths']):
            with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as archive:
                for file in files: archive.write(file,file.name)
                archive.writestr('README.txt','市集正式绘制器预览：overview 为完整地图，stick／round／amoled 为设备视口。\n包含玩家、同伴、常驻和结伴访客，使用固件 Market::Session 与 MarketView；固定种子 38。\n不是设备实拍，不代表实机验收。\n')
            return out.getvalue()
        with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as archive:
            for file in files: archive.write(file,file.name)
            archive.writestr('README.txt','native-map.png 使用正式资源包与 GameAssets 图块绘制器。\n'
                'native-route 图片使用正式 AMOLED 探索页面绘制器，按第一条路线逐点导出。\n'
                f'已自动核对 {len(EDITOR_TILES)} 个编辑器图块各帧的原生遮挡像素，以及透明动画的缓存重绘与前景遮挡。\n'
                f'另核对 {len(LAKE_MASKS)} 种自动湖泊各 8 帧的缓存重绘与前景覆盖。\n'
                f'另核对 {len(ROTATED_TILES)} 个旋转物件图块的正式遮挡深度。\n'
                '含动态物件时附 native-animation.gif；PNG 为对应静态帧。气球采用绿宝石原始四帧，约 280ms/帧。\n'
                '活动区域与 NPC 不在此地图/路线绘制器中；请使用编辑器“预览活动”查看 NPC，共享活动配置随游戏数据包导出。\n'
                '这些是本机运行正式绘制代码的结果，不代表已经刷入设备或通过实机验收。\n')
        return out.getvalue()


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project',type=Path,nargs='?')
    parser.add_argument('--build-only',action='store_true')
    parser.add_argument('--output-dir',type=Path,default=LOCAL/'native')
    args=parser.parse_args()
    if args.build_only: print(binary())
    elif args.project: print('\n'.join(map(str,render(json.loads(args.project.read_text()),Catalog(),args.output_dir))))
    else: parser.error('project or --build-only is required')
