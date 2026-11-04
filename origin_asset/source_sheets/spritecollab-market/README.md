# 市集 NPC 原动画来源

默认形态的吉利蛋（0113）、袋兽（0115）、信使鸟（0225）和变隐龙（0352）。

- 来源：[PMDCollab/SpriteCollab](https://github.com/PMDCollab/SpriteCollab/tree/350b68033bed372c630a1384dbaef86f42f84fb8/sprite)。使用与仓库现有阴影注册相同的固定版本，非 alternate／shiny 素材。
- 原作精灵美术：Pokémon / Nintendo / Creatures / GAME FREAK / Chunsoft。按源仓库的原作素材说明保留归属；不是新绘制的原创角色。
- `source.json` 记录上游版本及下载文件 SHA256；XML、Anim 和 Shadow PNG 均保留原字节。
- `tools/import_market_npc_frames.py` 离线按原阴影中心定位，最近邻 2 倍输出，保留透明度，不生成或重绘像素。
- `origin_asset/market_npc_species.json` 是角色列表；这四种当前只作 NPC，不改变可收服名单。
- 运行时图包由原 `generate_pokemon_sprites.py` 生成。预览图、接入日志和验收截图放 `.stick-local/`。
