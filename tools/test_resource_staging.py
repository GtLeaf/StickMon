import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import build_web

class ResourceStagingTests(unittest.TestCase):
    def test_only_external_target_receives_required_map_and_menu_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);data=root/'data';pack=data/'packs/dev'
            (root/'tools').symlink_to(build_web.ROOT/'tools',target_is_directory=True)
            catalog = root / 'origin_asset/room/wallpapers/catalog.json'
            catalog.parent.mkdir(parents=True)
            catalog.write_text(json.dumps([
                {'id': 0, 'name': 'Base', 'price': 0, 'asset': None, 'swatch': 'base.png'},
                {'id': 42, 'name': 'Sky', 'price': 15, 'asset': 'sky', 'swatch': 'sky.png'},
            ]))
            (pack/'game').mkdir(parents=True)
            for name in ['ui.smonfx','ui_amoled.smonfx']:
                shutil.copyfile(build_web.ROOT/'data/packs/dev/game'/name,pack/'game'/name)
            for name in ['catalog/10.smondata','catalog/20.smondata','catalog/99.smondata',
                         'ui/menu.smonmenu','ui/unused.smonmenu','fonts/zh16.smonfont',
                         'fonts/round-zh16-a4.smonfont', 'rooms/sky.smonwall', 'rooms/obsolete.smonwall']:
                p=pack/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'fixture')
            with patch.object(build_web,'ROOT',root):
                build_web.stage_resources('amoled-v2',root/'v2')
                build_web.stage_resources('sticks3',root/'stick')
            def selected(target):
                return {p.relative_to(root/target).as_posix() for p in (root/target).rglob('*')
                        if p.suffix in ['.smondata','.smonmenu']}
            self.assertEqual(selected('v2'),{'packs/dev/catalog/10.smondata',
                'packs/dev/catalog/20.smondata','packs/dev/ui/menu.smonmenu'})
            self.assertEqual(selected('stick'),set())
            self.assertFalse((root/'v2/packs/dev/fonts/round-zh16-a4.smonfont').exists())
            self.assertTrue((root/'v2/packs/dev/fonts/zh16.smonfont').exists())
            for target in ('v2', 'stick'):
                self.assertEqual([p.name for p in (root/target).rglob('*.smonwall')], ['sky.smonwall'])

if __name__=='__main__':unittest.main()
