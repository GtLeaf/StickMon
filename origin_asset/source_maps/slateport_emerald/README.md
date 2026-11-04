# 绿宝石凯那市集市原素材

来源为 [pret/pokeemerald 的 Slateport tileset](https://github.com/pret/pokeemerald/tree/master/data/tilesets/secondary/slateport)，2026-10-26 取回。逐文件来源 URL 与 SHA-256 见 `sources.json`；构建只读取此本地快照，不在运行时下载。

原始 PNG 是索引图，不能直接按其预览调色板使用。`tools/slateport_market.py` 读取 metatile 的前景四个 8×8 子块、调色板编号和原始水平/垂直翻转标志；透明索引为 0。仅移除作为底层的地面，保留物件原像素/原色，以最近邻将 16px 放大到编辑器 32px。

气球为 metatile 836，其四个子块对应 VRAM tile 736～739；`anim/balloons/0.png`～`3.png` 按原顺序循环。原代码 `src/tileset_anims.c` 的 `TilesetAnim_Slateport` 每 16 个 GBA tick 更新，`QueueAnimTiles_Slateport_Balloons` 按四帧循环；帧 1 与帧 3 的像素相同，为回正姿态。本项目使用现有地图时钟约 280ms/帧，未重绘或合成新的摆动形态。

这里只是原素材的技术来源记录，不改变素材原有权利归属。
