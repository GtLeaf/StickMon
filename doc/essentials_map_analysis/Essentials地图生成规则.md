# Pokemon Essentials 69 张地图扫描与生成规则

> 扫描日期：2026-07-13。来源：Pokemon Essentials v21.1。69/69 个 `MapXXX.rxdata` 均成功解析和渲染。

## 关键结论

- 当前草地灯塔构图不成立：灯塔素材虽然完整，但小池塘不足以建立海岸语义。灯塔应位于连续海域中的近岸基座、礁石平台、岛屿或防波堤上。
- `Outside.png` 确实包含 `3941～3983` 的 3x6 白色灯塔模块，但 69 张示例地图中实际使用灯塔的地图数量为 **0**。因此灯塔位置必须从 Route 4、Route 6、Route 8、港口和岛屿的海岸规则推导。
- Essentials 的地图不是按装饰块随机铺设，而是先确定边界、入口/出口、主路线和功能节点，再用草丛、水域、森林、建筑及前景层填充剩余空间。
- 多出口地图保留所有有效路线；路线应共享入口侧主干，并保证每段道路通向出口、建筑门口或明确事件点。

![扫描统计](scan_summary.png)

## 全量统计

- 地图总数：69
- 户外路线：12；城镇/户外设施：4；公园/野外区域：3；岛屿/海岸：3
- 洞窟/水下：5；普通室内：18；商店/服务设施：13；道馆/竞技设施：10
- 含水域：15；含森林边界：19；含 Layer 1 前景高草：1
- Tileset 使用量：Outside 21；Interior general 20；Department store interior 7；Poke Centre interior 5；Gyms interior 4；Caves 3；Trainer Tower interior 3；Mart interior 2；Game Corner interior 1；Dungeon cave 1；Underwater 1；Harbour interior 1

## 通用生成规则

### 1. 先画拓扑，再画景观

先确定入口、1～2 个出口、建筑门口、楼梯和事件点，再生成连接这些节点的主路线。剩余区域才分配草丛、水域、森林和装饰。不得先铺大面积道路，再反向寻找可走路线。

### 2. 边界必须有语义

户外地图通常用森林、悬崖、海洋、围栏或建筑连续封边，只在实际转场位置留下窄开口。屏幕边缘不能出现被截断的独立树顶、树根或半个建筑。

### 3. 多出口道路共享主干

多个出口可以同时存在，但路线应先共享入口侧主干，再在靠近出口处形成最短分支。每段道路必须通往入口、出口、门口或事件点；没有用途的道路属于浪费。道路宽度由玩法决定，StickMon 保持两格宽并允许精灵沿两格中线自由移动。

### 4. 地形必须形成连续区域

道路、草丛、水面和山壁使用完整边缘、转角、内部与收口组合。禁止把内部 tile 直接贴到普通草地，也不能用池塘模块冒充海洋。小水面使用池塘边缘；海岸使用 Sea deep -> Sea edge -> shore -> land 的连续过渡。

### 5. 草有两种高度语义

`390` 是 Layer 0 的普通可遇敌草；`391` 是 Map028 使用的 Layer 1 前景高草，具有 `priority=1` 和 `terrainTag=10`。高草应成片分布在道路两侧，不能盖住出口、道路、水面、灯塔或建筑门口。

### 6. 森林是多行模板

Map039 的森林由 Layer 2 树冠、Layer 1 栅栏、Layer 0 接缝行、树顶/树身对和根部组成。根部上一行必须是树身；位于地图底部时仍要保持完整序列，不能把树顶直接接根部。

### 7. 地标需要环境支撑

地标不应孤立摆放。建筑门口需要前庭和道路；喷泉需要广场；灯塔需要连续水域、海岸或堤岸；桥梁两端需要可达道路。地标周围应留出可识别轮廓和交互空间。

### 8. 建筑门口对齐道路

城镇建筑通常沿道路或广场布置，门口前至少留出 1～2 格净空。屋顶、招牌和树冠可进入上层，但碰撞仍由底层结构和 passage 决定。

### 9. 室内保持中央通道

住宅、商店和设施多采用矩形房间：墙体封边，家具沿墙或围绕功能区布置，入口到柜台、楼梯或事件点的中央通道保持畅通。竞技场和多层商店则使用明显的轴线与对称布局。

### 10. 洞窟按高度带组织

洞窟用连续岩壁带、台阶和窄口表达高度变化。岩石用于修整轮廓和控制通路宽度，不能随机散落到主路中央。楼梯必须连接两个可读的高度区域。

### 11. 三层图块语义不能互换

Layer 0 放地面和主体；Layer 1 放高草、围栏、建筑与可遮挡结构；Layer 2 放树冠、屋檐和更高前景。`passages`、`priorities`、`terrainTags` 必须随原 tile 保留。

### 12. StickMon 的裁图规则

先在更大的逻辑地图上生成完整拓扑，再选择 16x12 格镜头区域。镜头内必须同时看见当前位置、至少一段前进方向和一个景观锚点；不要为了塞满屏幕而截断树、建筑或海岸过渡。

## 代表性地图

![户外路线规则](rules_outdoor_routes.png)

![城镇与野外规则](rules_settlements.png)

![海岸与洞窟规则](rules_coasts_caves.png)

![室内规则](rules_interiors.png)

## 应用到 StickMon：灯塔海岸修正

修正版不再使用错误的 `392～410` 空地图块。地图左侧按 Map039 的深海、深海边缘、近岸和陆地顺序构造连续海岸，灯塔基座位于近岸水中，道路与草地留在陆地侧。小型封闭水池另用 Map028 的 `1088～1106` 岩岸静水模块；禁止仅修改 passage/terrainTag 把视觉空地伪装成水面。

![灯塔海岸修正](lighthouse_coast_corrected.png)

## 图层证据

![Map028 高草图层](layers_map028_high_grass.png)

![Map039 森林图层](layers_map039_forest.png)

## 全部地图图册

![all_maps_01](all_maps_01.png)

![all_maps_02](all_maps_02.png)

![all_maps_03](all_maps_03.png)

![all_maps_04](all_maps_04.png)

![all_maps_05](all_maps_05.png)

## 逐图索引

| ID | 名称 | 类别 | 尺寸 | Tileset | 事件 | L1/L2覆盖 | 特征 |
|---:|---|---|---:|---|---:|---:|---|
| 001 | Intro | 特殊场景 | 20x15 | Poke Centre interior | 2 | 0%/0% | 基础地形 |
| 002 | Lappet Town | 城镇/户外设施 | 32x21 | Outside | 4 | 17%/2% | 森林边界、栅栏、水域 |
| 003 | Player's house | 住宅/普通室内 | 31x15 | Interior general | 14 | 23%/3% | 室内分区 |
| 004 | Pokémon Lab | 住宅/普通室内 | 20x15 | Interior general | 11 | 28%/1% | 室内分区 |
| 005 | Route 1 | 户外路线 | 36x24 | Outside | 11 | 11%/4% | 道路、草丛、森林边界、栅栏 |
| 006 | Kurt's house | 住宅/普通室内 | 20x15 | Interior general | 2 | 18%/4% | 室内分区 |
| 007 | Cedolan City | 城镇/户外设施 | 60x43 | Outside | 11 | 24%/6% | 道路、森林边界、栅栏、水域 |
| 008 | Daisy's house | 住宅/普通室内 | 20x15 | Interior general | 6 | 20%/3% | 室内分区 |
| 009 | Cedolan City Poké Center | 商店/服务设施 | 20x15 | Poke Centre interior | 6 | 21%/1% | 室内分区 |
| 010 | Cedolan Gym | 道馆/竞技设施 | 20x17 | Gyms interior | 4 | 5%/0% | 室内分区 |
| 011 | Pokémon Institute | 住宅/普通室内 | 20x15 | Interior general | 5 | 31%/6% | 室内分区 |
| 012 | Cedolan City Condo | 住宅/普通室内 | 20x15 | Interior general | 5 | 23%/1% | 室内分区 |
| 013 | Game Corner | 商店/服务设施 | 20x32 | Game Corner interior | 31 | 23%/1% | 室内分区 |
| 014 | Cedolan Dept. 1F | 商店/服务设施 | 20x17 | Department store interior | 6 | 22%/1% | 室内分区 |
| 015 | Cedolan Dept. 2F | 商店/服务设施 | 20x17 | Department store interior | 6 | 21%/3% | 室内分区 |
| 016 | Cedolan Dept. 3F | 商店/服务设施 | 20x17 | Department store interior | 5 | 24%/6% | 室内分区 |
| 017 | Cedolan Dept. 4F | 商店/服务设施 | 20x17 | Department store interior | 5 | 23%/2% | 室内分区 |
| 018 | Cedolan Dept. 5F | 商店/服务设施 | 20x17 | Department store interior | 6 | 21%/3% | 室内分区 |
| 019 | Cedolan Dept. Rooftop | 商店/服务设施 | 32x20 | Department store interior | 3 | 11%/3% | 室内分区 |
| 020 | Cedolan Dept. Elevator | 商店/服务设施 | 20x15 | Department store interior | 5 | 2%/0% | 室内分区 |
| 021 | Route 2 | 户外路线 | 39x77 | Outside | 17 | 14%/11% | 道路、草丛、森林边界、栅栏、水域 |
| 023 | Lerucean Town | 城镇/户外设施 | 41x41 | Outside | 9 | 24%/4% | 道路、森林边界、栅栏 |
| 024 | Lerucean Town Poké Center | 商店/服务设施 | 20x15 | Poke Centre interior | 6 | 21%/1% | 室内分区 |
| 025 | Lerucean Town Mart | 商店/服务设施 | 20x15 | Mart interior | 4 | 20%/1% | 室内分区 |
| 026 | Pokémon Fan Club | 住宅/普通室内 | 20x15 | Interior general | 8 | 17%/0% | 室内分区 |
| 027 | Pokémon Day Care | 住宅/普通室内 | 20x15 | Interior general | 4 | 14%/0% | 室内分区 |
| 028 | Natural Park | 公园/野外区域 | 50x45 | Outside | 3 | 21%/2% | 道路、草丛、前景高草、森林边界、栅栏、水域 |
| 029 | Natural Park Entrance | 住宅/普通室内 | 20x15 | Interior general | 4 | 18%/1% | 室内分区 |
| 030 | Natural Park Pavillion | 住宅/普通室内 | 20x15 | Interior general | 7 | 10%/0% | 室内分区 |
| 031 | Route 3 | 户外路线 | 70x69 | Outside | 30 | 2%/3% | 道路、草丛、森林边界 |
| 034 | Ice Cave | 洞窟/水下 | 42x41 | Caves | 11 | 30%/0% | 岩壁分层 |
| 035 | Ingido Plateau outside | 户外路线 | 35x40 | Outside | 2 | 22%/5% | 道路、森林边界 |
| 036 | Pokémon League entrance | 住宅/普通室内 | 25x19 | Poke Centre interior | 8 | 19%/1% | 室内分区 |
| 037 | Pokémon League room 1 | 道馆/竞技设施 | 20x15 | Gyms interior | 3 | 13%/1% | 室内分区 |
| 038 | Hall of Fame | 道馆/竞技设施 | 20x15 | Gyms interior | 1 | 0%/0% | 室内分区 |
| 039 | Route 4 | 户外路线 | 44x23 | Outside | 1 | 16%/3% | 道路、草丛、森林边界、栅栏、水域 |
| 040 | Route 4 Cycling Road | 户外路线 | 33x21 | Outside | 1 | 18%/3% | 道路、草丛、森林边界、栅栏、水域 |
| 041 | Route 5 | 户外路线 | 36x138 | Outside | 96 | 14%/4% | 道路、草丛、森林边界、栅栏、水域 |
| 044 | Route 6 | 户外路线 | 45x38 | Outside | 1 | 18%/6% | 道路、草丛、森林边界、栅栏、水域 |
| 045 | Route 6 Cycling Road | 户外路线 | 39x24 | Outside | 1 | 22%/1% | 道路、草丛、森林边界、栅栏、水域 |
| 046 | Route 4 Cycling Road gate | 住宅/普通室内 | 20x15 | Interior general | 3 | 28%/0% | 室内分区 |
| 047 | Route 7 | 户外路线 | 70x43 | Outside | 12 | 11%/2% | 草丛、森林边界 |
| 049 | Rock Cave 1F | 洞窟/水下 | 34x29 | Caves | 16 | 20%/1% | 岩壁分层 |
| 050 | Rock Cave B1F | 洞窟/水下 | 30x26 | Caves | 4 | 41%/9% | 岩壁分层 |
| 051 | Dungeon | 洞窟/水下 | 20x15 | Dungeon cave | 1 | 0%/0% | 岩壁分层 |
| 052 | Battle Frontier | 城镇/户外设施 | 44x36 | Outside | 6 | 23%/4% | 基础地形 |
| 053 | Battle Frontier Poké Center | 商店/服务设施 | 20x15 | Poke Centre interior | 6 | 21%/1% | 室内分区 |
| 054 | Battle Frontier Mart | 商店/服务设施 | 20x15 | Mart interior | 6 | 19%/1% | 室内分区 |
| 055 | Battle Tower | 道馆/竞技设施 | 20x15 | Trainer Tower interior | 12 | 22%/5% | 室内分区 |
| 056 | Battle Tower arena | 道馆/竞技设施 | 20x15 | Trainer Tower interior | 2 | 7%/0% | 室内分区 |
| 057 | Stadium Cup lobby | 道馆/竞技设施 | 20x15 | Trainer Tower interior | 11 | 19%/5% | 室内分区 |
| 058 | Battle Palace | 住宅/普通室内 | 20x15 | Interior general | 7 | 16%/2% | 室内分区 |
| 059 | Battle Palace arena | 道馆/竞技设施 | 20x15 | Interior general | 2 | 5%/0% | 室内分区 |
| 060 | Battle Arena | 道馆/竞技设施 | 20x15 | Interior general | 5 | 13%/2% | 室内分区 |
| 061 | Battle Arena arena | 道馆/竞技设施 | 20x15 | Gyms interior | 2 | 3%/0% | 室内分区 |
| 062 | Battle Factory | 住宅/普通室内 | 20x15 | Interior general | 8 | 21%/3% | 室内分区 |
| 063 | Battle Factory intro corridor | 住宅/普通室内 | 20x15 | Interior general | 1 | 16%/0% | 室内分区 |
| 064 | Battle Factory arena | 道馆/竞技设施 | 20x15 | Interior general | 2 | 12%/0% | 室内分区 |
| 065 | Battle Factory corridor | 住宅/普通室内 | 20x15 | Interior general | 1 | 16%/0% | 室内分区 |
| 066 | Safari Zone outside | 公园/野外区域 | 22x21 | Outside | 1 | 13%/3% | 道路、草丛、森林边界、栅栏 |
| 067 | Safari Zone gate | 住宅/普通室内 | 20x15 | Interior general | 2 | 14%/0% | 室内分区 |
| 068 | Safari Zone | 公园/野外区域 | 51x34 | Outside | 3 | 10%/4% | 道路、草丛、水域 |
| 069 | Route 8 | 户外路线 | 46x35 | Outside | 5 | 16%/3% | 草丛、森林边界、栅栏、水域、瀑布 |
| 070 | Route 8 underwater | 洞窟/水下 | 46x35 | Underwater | 0 | 15%/0% | 水域、岩壁分层 |
| 071 | Route 8 harbor | 岛屿/海岸 | 20x15 | Harbour interior | 4 | 42%/5% | 水域 |
| 072 | Berth Island | 岛屿/海岸 | 29x31 | Outside | 8 | 9%/6% | 森林边界、水域 |
| 073 | Faraday Island | 岛屿/海岸 | 29x31 | Outside | 4 | 9%/6% | 森林边界、水域 |
| 074 | Route 6 Cycling Road gate | 住宅/普通室内 | 20x15 | Interior general | 3 | 28%/0% | 室内分区 |
| 075 | Tiall Region | 户外路线 | 32x24 | Outside | 1 | 7%/2% | 草丛、森林边界 |
