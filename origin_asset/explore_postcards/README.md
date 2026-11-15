# 探险封面来源

2026-11-15：雾林径、古瀑谷改为当前运行时地图截图，取消旧菜单背景及早期静态地图。
这两张图由 C++ `ExploreMapGenerator` 与 `GameAssets::drawExploreTileTo` 使用已安装的资源包绘制，
不依赖 Essentials 外部源目录，不重新设计地形。截图只含实际地图三层图块，不含角色与界面。

| 文件 | 地图生成调用 | 算法版本 | 地图指纹 |
| --- | --- | --- | --- |
| mist-forest.png | generate(38, BOTTOM, MIST_FOREST_PATH_AREA, ..., {5,7,false}) | 26 | 0c4adb87 |
| ancient-valley.png | generateValleyRegion(262, {10,0}, false, ...) | 26 | 231aac99 |
| market.png | 实际市集地图截图05「中央广场」 | 市集地图 | — |

雾林径取深林曲径；古瀑谷取高桥A下层，包含跨瀑布的桥和落水潭。
原图为416×312，动画帧为0。封面生成器按比例裁成160×122、量化64色并RLE编码；
雾林径居中取景，古瀑谷采用centering=(0.85,0.5)，向瀑布侧取景以保留桥面与落水潭。
游戏中2×显示到320×244的圆角场景卡片，卡片内叠加暖色蒙层，角色和界面另行绘制。

复现两张运行时截图：

```bash
python3 tools/capture_explore_cover_maps.py --output .stick-local/artifacts/explore-cover-recapture/runtime
cp .stick-local/artifacts/explore-cover-recapture/runtime/mist-stage-5.png origin_asset/explore_postcards/mist-forest.png
cp .stick-local/artifacts/explore-cover-recapture/runtime/valley-node-10.png origin_asset/explore_postcards/ancient-valley.png
python3 tools/generate_explore_postcards.py
```

地图算法或资源包变更后，重新截图并更新此表中的版本和指纹，避免封面与实际地图脱节。
本次验收目录：.stick-local/artifacts/explore-real-map-covers-20261115/。
