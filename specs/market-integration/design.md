# 设计

- ExploreDestinations：独立目的地目录；列表位置映射到稳定的地区编号或市集类型，不扩展持久化地区数组。
- market_export.py：校验最终图层、来源、物件占地与 roadNetwork；输出版本化半格导航、常驻／访客配置及图层。维护源 origin_asset/maps/market/market.stickmap.json。
- MarketSession：纯 C++ 100ms 模拟；道路半格网格容纳双向和避让，格中心活动区单独限制，统一动态预约；随机停留和目的地，同伴跟随。
- MarketView：共用 tile atlas、方向动画、原尺寸和深度语义；设备负责视口和输入适配。市集会话属于临时状态，不修改 SaveCodec。
- 市集与普通探索会话分开；地区列表经统一映射选择。当前左右常驻范围不自动扩大，访客可在摊位邻近道路等待／停留。

## 市集出门动画

- `ExploreDestinations::MARKET` 只作为临时出发目的地传入既有动画状态机，不写入持久化地区数组。
- AMOLED V1/V2 复用 `queueExploreDeparture`：回房后主精灵及健康同伴依次走到门口、穿门，房间全黑后才创建市集，再渐显；进入期间暂停市集行走。
- Stick／谷仓复用 `GameEngine::beginExploreDeparture` 与 `MainScene` 出门流程；`ExploreScene::onEnter` 在普通地区编号转换前分派市集。音乐保持 `MARKET`，加载失败回到选择页并清理临时出发状态。
- 市集继续跳过普通探索饥饿／地区解锁门槛，不记录 `lastExploredAt`、探索次数或探索费用；不能把市集编号当作普通地区索引。全部倒下时沿用渐隐兜底。

## 市集漫步

- 导出器额外输出最终碰撞网格 `pedestrian`，道路网格仍服务来访组；`enableManual()` 仅由横屏场景启用，AMOLED 默认道路模式不变。
- MarketWalkUi 管理摇杆和聊天邻近目标，命中和绘制共用几何；ExploreScene 负责设置方向、离场及音乐。
- 玩家每 200ms 跨半格；同伴遇反向输入先让路。自由漫步返程时两名成员分别沿动态路网回入口，避免前后顺序变化导致堵塞。
- 聊天新目标距离 32px，原目标保持到 44px；取脚底位置并沿碰撞网格检查视线，排除自己与同伴。
- MarketView 的 `freeWalk` 默认为 false。横屏启用后取消页脚、角色用 1×，视口高于地图时只延展边界地面，不复制摊位／树木／围栏。
- bgm_market 来自 Emerald 原 MIDI，以现有 Essentials SoundFont 转制，音色不同于 GBA 原硬件；8kHz IMA ADPCM 186404B，完整 46.25 秒，0.53575 秒起循环。AudioManager 仅对 8kHz 曲目流式插值到现有 16kHz 后端，原 16kHz 路径保持不变。

## 市集 NPC 与单只访客

- `market_npc_species.json` 是 NPC 专属素材目录，不向 `Species.cpp`、孵化／遭遇或 ORAS 名单注册；素材生成器和编辑器共同读取。`NPC_PMD_SPECS` 在原 PMD 枚举及 STATUS 后追加，旧包保持。
- `Family{adult, juvenile}` 保留数据版本 1；`juvenile=0` 是无同行成员。Market::Session 和 AuthoredNpcPopulation 均在单只分支跳过第二成员的激活、跟随和离场等待。
- NPC 源图固定 SpriteCollab 已用版本，身体与阴影一起离线导入；保留脚底标记及来源哈希。只为市集所需打入图标、选择页正面图和方向动画。
- 角色位置和候选只改地图维护源；不扩左右两格范围，不改变道路图层。0 从预载、物种计数和 ZIP 资源集合中排除；单只至少一个停留点，结伴至少两个。
- 验证完整进出、单格单只、混合候选、暂停、碰撞、玩家同伴和重入，以及三家族场景和四目标构建；核实谷仓实际上限，不扩分区。
