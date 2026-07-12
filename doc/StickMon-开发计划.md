# StickMon 开发计划 v0.1

记录日期：2026-06-27

## 1. 项目边界

StickMon 是运行在 M5Stack StickS3 上的便携电子宠物游戏。目标是先做出一个稳定、可携带、可保存、可单机游玩的养成闭环，再逐步加入 ESP-NOW 对战、交换与通讯进化。

本计划以 `StickS3_电子宠物机_玩法与技术设计文档_v0.1.md` 的后半段 v0.2 补丁集为准。旧文档中与本计划冲突的内容，按本计划处理。

## 2. 硬约束

- 不实现 IR 通信。
- 不初始化 IR TX/RX，不引入 IR 链路层，不做 IR 唤醒、IR 交换、IR 通讯进化。
- 设备互联只使用 ESP-NOW。
- 通讯进化由 ESP-NOW 的交换/进化会话触发，仪式感通过屏幕动画、双方确认、SAS 4 位校验实现。
- 首发版本优先离线可玩；Wi-Fi 联网只作为后续天气/活动扩展，不阻塞核心玩法。
- UI 固定按 StickS3 竖屏 135x240 设计。
- 两键输入优先，Mic/IMU 是增强输入，不可成为必需操作。
- 所有玩家可见 UI 文案使用中文，并集中管理，避免散落硬编码。

## 3. 技术路线

### 3.1 工程基线

- 构建系统：PlatformIO。
- 框架：Arduino。
- 目标板：ESP32-S3 / M5Stack StickS3。
- 核心库：M5Unified、M5GFX。
- 分区：8 MB Flash，保留 NVS、factory、OTA0、OTA1、SPIFFS/LittleFS。
- 存档：ESP32 Preferences/NVS，双缓冲 + CRC。
- 工程配置参考 PokeBug：
  - `platform = espressif32 @ 6.6.0`
  - `board = esp32-s3-devkitc-1`
  - `framework = arduino`
  - `board_build.arduino.memory_type = qio_opi`
  - `board_build.partitions = default_8MB.csv`
  - `build_flags` 必须包含 `BOARD_HAS_PSRAM`、`ARDUINO_USB_CDC_ON_BOOT=1`、`ARDUINO_USB_MODE=1`
  - `lib_deps` 使用 `m5stack/M5Unified@^0.2.3`、`m5stack/M5GFX@^0.2.3`

### 3.2 PSRAM 使用策略

StickMon 默认启用 PSRAM，并把它当作渲染和动画性能优化层：

- 双 framebuffer / 大 sprite 缓冲优先放 PSRAM。
- 精灵渲染优先使用 `PokemonSprites` 压缩资源；资源缺失时保留色块/几何 sprite 作为降级路径。
- 宝可梦原型图像资源通过 `tools/generate_pokemon_sprites.py` 从 Pokemon Essentials 源图重新生成 RGB565 RLE C++ 常量，避免运行时解 PNG。
- 属性特效、状态效果、房间背景缓存优先使用 PSRAM。
- 高频小对象、场景状态机、存档镜像仍放 SRAM / NVS，避免 PSRAM 访问延迟影响逻辑。
- 若 PSRAM 初始化失败，降级到单 framebuffer + 简化特效，但核心玩法仍可运行。

### 3.3 推荐目录

```text
src/
  main.cpp
  core/
    GameEngine.*
    Scene.*
    ButtonDispatcher.*
    SaveManager.*
    UiStrings.*
  hardware/
    Hal.*
    PixelRenderer.*
    EspNowLink.*
  game/
    Species.*
    Monster.*
    BattleCalc.*
    Encounter.*
    Inventory.*
    Economy.*
  scenes/
    MainScene.*
    MenuScene.*
    TeamScene.*
    BagScene.*
    ShopScene.*
    ExploreScene.*
    BattleScene.*
    TradeScene.*
    SettingsScene.*
  assets/
    ...
tools/
  asset_convert/
doc/
```

### 3.3 可复用经验

- 从 PokeBug 借鉴：PlatformIO 工程配置、PSRAM build flags、场景切换、按钮分发、NVS 存档、ESP-NOW 房间/对战思路、低功耗主循环。
- 从 CyberGardenerV2 借鉴：135x240 竖屏资源约束、背景/主体分层、深睡唤醒最小更新路径、资源转换脚本风格。
- 不照搬 PokeBug 的昆虫属性体系，也不照搬 CyberGardenerV2 的 IR 社交链路。

## 4. 版本路线

## Milestone 0：工程初始化

目标：仓库从设计文档变成可编译的 StickS3 工程。

交付：
- `platformio.ini`
- `src/main.cpp`
- `Hal` 初始化显示、按键、电源、IMU、Mic/Speaker 占位接口
- `GameEngine` 主循环
- `Scene` 基类
- 空白主界面
- `pio run` 可通过

验收：
- USB 串口启动日志正常。
- 屏幕显示启动画面。
- A/B 两键可在日志或屏幕上产生响应。
- 不出现任何 IR 初始化代码。

## Milestone 1：渲染、输入与菜单

目标：形成基础 UI 壳，确认小屏交互手感。

交付：
- 主界面：当前出战 StickMon、状态栏、底部提示区
- 主菜单：Team、Bag、Explore、Battle、Trade、Shop、Settings、Sleep
- `ButtonDispatcher`：短按、长按、组合键
- `PixelRenderer`：像素文本、进度条、图标、简单 sprite
- 设置项：亮度、音量、游戏速度、idle 时间

验收：
- 所有菜单可用两键完整进入/返回。
- 文本不溢出 135px 宽度。
- idle 后可降亮度，长按可休眠。

## Milestone 2：存档与玩家数据

目标：所有核心状态可持久化，设备重启不丢进度。

交付：
- `SaveManager`
- 玩家档案：trainer_id、金币、步数、设置
- 队伍：2 只
- 仓库：首发 20 只，结构预留扩容
- 背包：球、食物、药、进化石、神奇糖果
- `PokemonRuntime` 或 `MonsterRuntime`：等级、经验、HP、亲密度、心情、饱食度、熟练度、濒死状态、捕获时间
- 存档版本号与 CRC 校验；当前 v13 不迁移旧版本，版本不匹配时回退新档

验收：
- 修改设置、金币、队伍后重启保持。
- 关键事件即时落盘。
- 损坏存档能回退到新档，不黑屏。

## Milestone 3：OOBE 与养成闭环

目标：第一次开机能获得初始伙伴，并形成每日照料循环。

交付：
- 首次开机蛋孵化流程
- IMU/Mic 三因子积分选择草/火/水初始伙伴
- 喂食、摸头、休息、心情、亲密度
- 饥饿/饱食度衰减
- 濒死 24 小时静养恢复
- 步数经验结算
- 每日 cap 系统

验收：
- 新机启动 3 分钟内能完成孵化并进入主界面。
- 不操作时数值按虚拟时间变化。
- 喂食/摸头/走路对数值有明确反馈。

## Milestone 4：图鉴、属性与单机战斗

目标：可以进行稳定的 PvE 战斗。

交付：
- 首发 32 只开发期 StickMon 数据
- 属性表压缩 LUT
- 普攻 + 特殊技触发系统
- 伤害公式
- 状态异常基础框架
- PvE 战斗场景
- 濒死轮换流程
- 战斗奖励：经验、金币、亲密度变化
- 经验成长按 Pokemon Essentials `GrowthRate` 曲线计算；当前物种使用 Medium、Parabolic、Slow，运行时经验为 32 位累计值，等级由曲线反推并刷新 HP 上限

验收：
- 战斗能完整开始、结算、返回主界面。
- HP、伤害、命中、特殊技触发可通过日志复核。
- 全队濒死会退出战斗并进入静养代价。

## Milestone 5：探索与捕捉

目标：打通“走路/探索 -> 遭遇 -> 战斗 -> 捕捉 -> 入队/入库”的核心玩法。

交付：
- 探索场景
- 步数阈值遭遇
- 天气接口占位，本地 fallback
- 捕捉指令与球种选择
- IMU 投球力度
- 捕获概率
- 图鉴首次捕获奖励
- 仓库满时拒绝捕获并提示

验收：
- 30-120 步随机遭遇逻辑可运行。
- 捕捉成功会写入队伍或仓库。
- 捕捉失败会消耗球并继续战斗。

## Milestone 6：商店、经济与成长

目标：让玩家有长期资源目标。

交付：
- 金币系统
- 商店场景
- 购买/使用球、食物、药、进化石、神奇糖果
- 等级进化、道具进化、亲密度进化
- 每日/每周限购
- 简单成就或图鉴进度奖励

验收：
- 金币收支正确。
- 道具上限正确。
- 进化动画和写盘流程稳定。

## Milestone 7：ESP-NOW 对战

目标：两台 StickS3 可以进行 1v1 对战。

交付：
- `EspNowLink`
- HELLO / BATTLE_REQ / BATTLE_ACK / BATTLE_TURN / BATTLE_END
- 会话 nonce、session_id、seq、CRC/HMAC 截断校验
- 4 位 SAS 配对确认
- 断线超时处理
- ranked Lv50 缩放
- PvP 奖励与每日主动发起次数限制

验收：
- 两台设备能发现、配对、开战、结算。
- 双方回合同步，不出现一边胜利一边失败。
- 中断后能超时回收并保存合理结果。
- 代码中仍无 IR 通信依赖。

## Milestone 8：ESP-NOW 交换与通讯进化

目标：两台设备可以交换 StickMon，并通过交换触发通讯进化。

交付：
- TRADE_REQ / TRADE_OFFER / TRADE_LOCK / TRADE_COMMIT / TRADE_CANCEL
- 双方选择待交换精灵
- 5 秒确认窗口
- 交换锁，防止中途重复保存
- 通讯进化条件检查
- 交换后自动进化动画
- 失败回滚

验收：
- 交换成功后双方仓库/队伍状态一致。
- 断线不会复制或吞掉精灵。
- 通讯进化完全基于 ESP-NOW 交换流程触发，不依赖 IR。

## Milestone 9：资源、美术与音效

目标：把占位素材替换成可发布的原创风格。

交付：
- 开发期 32 只精灵图像资源
- 主界面使用 64x64 `Icons` 两帧动画
- 战斗使用 72x72 压缩版 `Front` / `Back`
- 孵化场景使用压缩版 `Eggs/000`
- Essentials 道具图标：四种球、普通粮、两种伤药、解毒药、神奇糖果
- 四种球的投掷、打开、晃动和捕获星光动画
- 草丛小路、河畔、深林三套战斗背景
- UI 图标
- 属性特效
- 简单音效
- `tools/generate_pokemon_sprites.py` 资源转换脚本
- `tools/generate_game_assets.py` 通用游戏资源转换脚本
- 资源预算报告：32 只 + 蛋图，原始 RGB565 8,724,480 B，压缩资源 302,863 B；当前固件 Flash 用量 1,433,077 B，RAM 48,208 B
- 通用游戏资源包：原始 RGB565 348,512 B，Indexed4 RLE 包 64,632 B；2026-07-11 构建 Flash 1,074,105 B，RAM 48,968 B

验收：
- 所有素材可从源图重新生成 C/C++ 资源。
- 资源命名稳定。
- 屏幕上无明显闪烁、错位、透明色错误。

## Milestone 10：稳定性与发布

目标：准备 v0.1 可试玩固件。

交付：
- 自动保存和深睡压力测试
- 低电量策略
- v13 旧版/损坏存档回退新档测试
- ESP-NOW 长时间连接测试
- UI 文案统一
- README、烧录说明、玩法说明
- v0.1 release tag

验收：
- `pio run` 稳定通过。
- 连续运行 24 小时不崩溃。
- 重启 50 次不丢档。
- 双机对战/交换各 20 次无复制、丢失、死锁。

## 5. 首发功能裁剪

v0.1 必须有：
- OOBE 孵化
- 主界面养成
- 队伍/仓库/背包
- 单机探索
- PvE 战斗
- 捕捉
- 商店
- 存档
- 低功耗
- ESP-NOW 接口占位与协议结构定义，但不开放可玩联机

v0.1 可以没有：
- ESP-NOW PvP
- ESP-NOW 交换
- 通讯进化
- 天气联网
- Wi-Fi 活动
- NFC/GPS
- 多语言
- 完整原创化图鉴与原创美术

v0.2 再加入：
- ESP-NOW PvP
- ESP-NOW 交换
- 通讯进化
- 更多图鉴与技能

v0.3 再考虑：
- 天气遭遇率
- 活动事件
- NFC 实体卡
- 排行/战绩

## 6. 已确认设计决策

### 6.1 首批 32 只精灵

首批精灵全部走数据表驱动。名字、属性、进化关系、色块颜色、基础数值、普攻、特殊技都暴露在 `Species` 表里，方便后续替换原创名、美术和数值。基础数值、普攻、特殊技首版先参考对应宝可梦原型的设定落表，后续再平衡和原创化。

当前实现基于 `doc/精灵图鉴检索.md`、Pokemon Essentials v21.1 的 `PBS/pokemon.txt` 与 `Graphics/Pokemon` 资源落地。首批名单必须同时存在于 PMD 图鉴检索表和 Essentials `Graphics/Pokemon`，并覆盖御三家、神兽、通讯进化。叶伊布、冰伊布虽有 Essentials 图片，但不在 PMD 图鉴检索表中，已从首批移除；当前用梦幻、拉帝亚斯、拉帝欧斯补齐神兽覆盖与 32 只规模。

| ID | 名字 | 属性 | 进化 | 占位色块 |
|---|---|---|---|---|
| 001 | 妙蛙种子 | 草 / 毒 | 002 | 绿 + 深绿 |
| 002 | 妙蛙草 | 草 / 毒 | 003 | 绿 + 粉 |
| 003 | 妙蛙花 | 草 / 毒 | 0 | 深绿 + 粉 |
| 004 | 小火龙 | 火 | 005 | 橙色 |
| 005 | 火恐龙 | 火 | 006 | 红橙 |
| 006 | 喷火龙 | 火 / 飞行 | 0 | 红 + 蓝 |
| 007 | 杰尼龟 | 水 | 008 | 蓝 + 米色 |
| 008 | 卡咪龟 | 水 | 009 | 蓝 + 白 |
| 009 | 水箭龟 | 水 | 0 | 深蓝 + 棕 |
| 151 | 梦幻 | 超能力 | 0 | 粉 + 浅粉 |
| 172 | 皮丘 | 电 | 025 | 黄色 |
| 025 | 皮卡丘 | 电 | 026 | 黄 + 棕 |
| 026 | 雷丘 | 电 | 0 | 橙黄 |
| 133 | 伊布 | 一般 | 分支预留 | 棕色 |
| 134 | 水伊布 | 水 | 0 | 蓝 + 浅蓝 |
| 135 | 雷伊布 | 电 | 0 | 黄 + 淡黄 |
| 136 | 火伊布 | 火 | 0 | 红橙 + 金黄 |
| 196 | 太阳伊布 | 超能力 | 0 | 紫 + 粉 |
| 197 | 月亮伊布 | 恶 | 0 | 黑 + 黄 |
| 380 | 拉帝亚斯 | 龙 / 超能力 | 0 | 红 + 白 |
| 381 | 拉帝欧斯 | 龙 / 超能力 | 0 | 蓝 + 白 |
| 123 | 飞天螳螂 | 虫 / 飞行 | 212 | 绿 + 黄 |
| 212 | 巨钳螳螂 | 虫 / 钢 | 0 | 红 + 钢灰 |
| 092 | 鬼斯 | 幽灵 / 毒 | 093 | 紫色 |
| 093 | 鬼斯通 | 幽灵 / 毒 | 094 | 深紫 |
| 094 | 耿鬼 | 幽灵 / 毒 | 0 | 紫黑 |
| 129 | 鲤鱼王 | 水 | 130 | 红 + 金 |
| 130 | 暴鲤龙 | 水 / 飞行 | 0 | 蓝 + 米色 |
| 143 | 卡比兽 | 一般 | 0 | 蓝黑 + 米色 |
| 147 | 迷你龙 | 龙 | 148 | 蓝 + 白 |
| 148 | 哈克龙 | 龙 | 149 | 蓝 + 白 |
| 149 | 快龙 | 龙 / 飞行 | 0 | 橙 + 蓝 |

备注：这些名字作为开发期数据名暴露，发布前按版权策略替换为原创命名与原创美术。

### 6.1.1 精灵图像资源管线

当前图像资源来自 Pokemon Essentials v21.1：

- `Graphics/Pokemon/Icons/{SPECIES}.png`：128x64，切成两个 64x64 帧，用于主界面待机动画。
- `Graphics/Pokemon/Front/{SPECIES}.png`：160x160，转换为 72x72 RGB565 RLE，用于战斗野生方。
- `Graphics/Pokemon/Back/{SPECIES}.png`：160x160，转换为 72x72 RGB565 RLE，用于战斗己方。
- `Graphics/Pokemon/Eggs/000.png`：转换为 64x64 RGB565 RLE，用于 OOBE 孵化。

生成命令：

```bash
~/.platformio/penv/bin/python tools/generate_pokemon_sprites.py
```

生成产物：

- `src/assets/PokemonSprites.h`
- `src/assets/PokemonSprites.cpp`

压缩策略：透明像素写入 skip run，不透明像素写入 RGB565 run。渲染端由 `PixelRenderer::drawRgb565Rle` 直接解码到画布，不引入运行时 PNG 解码。

### 6.1.2 道具、捕捉动画与战斗背景

通用游戏图像继续来自 Pokemon Essentials v21.1：

- `Graphics/Items`：四种球、伤药、解毒药、神奇糖果；普通粮开发期映射为 `ORANBERRY.png`。
- `Graphics/Battle animations`：四种球各 8 帧旋转图、打开状态和 `ballBurst_star.png`。
- `Graphics/Battlebacks`：`field_bg.png`、`water_bg.png`、`forest_bg.png`，分别映射草丛小路、河畔、深林。

生成命令：

```bash
python3 tools/generate_game_assets.py
```

生成产物为 `data/packs/dev/game/game.smonfx`，路径登记在资源包 `manifest.json` 的 `gameAssets` 字段。包内统一使用最多 16 色的 Indexed4 RLE；运行时由 `GameAssets` 从 LittleFS 载入压缩帧，背景只解码并缓存当前区域的 240x135 RGB565 图像。

捕捉菜单支持精灵球、高级球、沉重球、时间球。高级球固定提高捕获率；沉重球根据目标基础能力总和加成；时间球随当前战斗回合数提高加成。捕捉动画为非阻塞时间轴，按钮和场景主循环保持运行。

### 6.2 属性系统

首版直接实现 18 属性表。首批 32 只只覆盖其中一部分属性，但战斗公式、属性 LUT、招式数据结构从第一版就按 18 属性设计，避免后续扩展返工。

### 6.3 主界面世界模型

主界面是精灵房间，不是静态看板。首版实现一个小型宠物活动区：

- 精灵在不规则活动区内移动、停留、睡觉、靠近食物；移动使用碰撞体校验和轻量路径重规划，不能穿出活动区。
- v0.1 可互动项只做两类：摸一摸、向碗里加食物。玩家不能直接投喂，精灵根据饱腹、昼夜和当前行为自行决定何时进食。
- 主界面 AI 分为需求、动作、导航三层。需求包含进食、休息、漫游、发呆，并带意图惯性；动作包含转身、移动、唤醒、进食、休息，避免行为频繁跳变。
- 运动速度、停留时间和漫游范围由精灵速度种族值、性格及少量物种配置共同决定；菜单往返时保留位置、朝向、目标和可恢复动作。
- 精灵优先使用压缩图像资源；美术资源缺失时统一降级为色块/几何 sprite 表示。
- 渲染对象带 `z` 或 `layer`，基础层级为 `background -> floor/props -> shadow -> monster -> state_effect -> interaction_prop -> HUD -> toast/modal`。
- `z` 轴同时用于后续扩展状态表现，例如睡觉在床后、濒死倒地、开心跳跃、饥饿靠近食物、特殊技蓄光。

### 6.4 时间与 OOBE

- 时间系统支持游戏速度倍率：1x / 2x / 4x / 8x。
- OOBE 蛋孵化基础时长为 180 秒，受游戏速度倍率影响。
- 调试时可通过速度倍率加速，不额外做跳过按钮。

### 6.5 UI 与存档

- UI 文案语言：中文。
- 仓库首版上限：20 只。
- 当前存档格式为 v13，只读取通过 magic、版本号和 CRC 校验的 v13 数据；旧版本或损坏数据直接初始化新档，不执行迁移。
- v0.1 完全不做可玩联机，只保留 `EspNowLink` 接口、消息枚举、协议结构和空实现，避免后续接入时改场景边界。

## 7. 近期任务清单

1. 初始化 PlatformIO 工程。
2. 搭 `Hal`、`GameEngine`、`Scene`、`ButtonDispatcher`。
3. 做一版 135x240 精灵房间主界面，精灵使用压缩 icon 帧并支持 z/layer 排序。
4. 做菜单和设置。
5. 做 NVS 存档骨架。
6. 定义首批 32 只 StickMon 数据表，暴露名字、属性、进化、占位色块、招式。
7. 实现 OOBE 孵化。
8. 实现养成数值 tick。
9. 实现 PvE 战斗核心。
10. 实现探索与捕捉。

## 8. 风险与处理

| 风险 | 处理 |
|---|---|
| 设计文档旧段落仍提 IR | 本开发计划明确定义为不实现，后续改文档时删除或标注废弃 |
| 宝可梦元素版权风险 | 当前资源只作为开发期原型。发布前名称、美术、叫声、图鉴全部原创化，只保留通用养成/属性/回合制机制 |
| 小屏 UI 塞不下 | 先做 32 只开发期图鉴和少量菜单，避免 UI 一开始过重 |
| ESP-NOW 对战不同步 | 单机战斗核心先做确定性结算，再接网络适配层 |
| NVS 写入磨损 | 关键事件即时保存，普通数值 5 分钟节流 |
| 电池续航不足 | 默认 30 秒 idle 降亮，1 分钟后 deep sleep，战斗外关闭功放与高频采样 |

## 9. 明确不做

- 不做 IR 交换。
- 不做 IR 通讯进化。
- 不做 IR 设备发现。
- 不做红外遥控彩蛋。
- 不为 IR 预留 Sprint。
- v0.1 不做可玩 ESP-NOW PvP、交换、通讯进化。
