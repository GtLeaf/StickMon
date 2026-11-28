# 关都、城都、丰缘扩充精灵素材

- `retimedSpeciesIds` 中的 56 只旧图包精灵使用同版原始 Idle、Walk、Sleep 及 Shadow，恢复完整动作与逐帧时长。其中鲤鱼王（0129）、冰鬼护（0362）保持既有顺序，其后按图鉴编号追加原单帧待机审计中的另外 54 只。旧版资源编号保留，新增动作块追加在已有编号末尾，原 29 只扩充名单及源文件哈希保持。重生成旧图包时需设置 `ESSENTIALS_DIR` 指向包含原战斗图与图标的 Essentials 素材目录。
- 哈克龙（0148）的 Idle 按原始 XML `CopyOf` 解析为 Walk，直接使用对应 Walk 原图。卡咪龟（0008）、比比鸟（0017）、大比鸟（0018）、小拳石（0074）、暴鲤龙（0130）、拉帝亚斯（0380）、拉帝欧斯（0381）的待机身体主要为原画布内位移；其 Shadow 白色定位点保持固定，按阴影注册会保留原始上下起伏，运行时应避免叠加同类程序浮动。

- Idle、Walk、Sleep 及阴影取自 [PMDCollab/SpriteCollab](https://github.com/PMDCollab/SpriteCollab/tree/350b68033bed372c630a1384dbaef86f42f84fb8/sprite) 的默认形态。信使鸟沿用 `../spritecollab-market/0225` 的同版原图。
- 战斗正面取自 [PokeAPI sprites](https://github.com/PokeAPI/sprites/tree/c0542f7c51215d9672439012838c99f520596cbd/sprites/pokemon/versions/generation-iv/heartgold-soulsilver)，背面取自同一固定版本的 `generation-iv/diamond-pearl/back`。原作美术归 Pokémon / Nintendo / Creatures / GAME FREAK / Chunsoft。
- `source.json` 保存版本、图包名单及文件 SHA256。从仓库根目录运行 `python3 tools/generate_pokemon_sprites.py --expansion-only`，验证源文件，按阴影中心注册待机、移动和睡眠帧，生成运行时图包和索引。
- 大于 65,535 个 RLE 数据字的传统图包使用 v4，在既有 28 字节头之后追加 2 字节小端高位计数。帧记录沿用 v3，解码载荷上限为 256 KiB。
- 85 只已接原始时长的精灵使用 v5 姿势库和逻辑帧引用表，自动复用相同、平移及水平镜像的姿势。v5 头追加高位 RLE 字数和姿势数量，载荷依次存放 16 字节逻辑帧、16 字节姿势记录、RLE 字池和调色板。原画布、脚点、阴影及 SpriteKind 编号保留，绘制时直接应用偏移与镜像。时长继续使用原始 60 Hz 时间表。读取器兼容 v2、v3、v4；重生成传统图包可加 `--legacy-frames`。
- `baseFrameOverrides` 保留已发布杰尼龟图包的 FRONT、STATUS 像素与落地点，来源为已发布图包的无损解码。两个 PNG 的哈希随原始动作文件一起校验，避免本地 Essentials 版本差异改变战斗和状态页素材。
