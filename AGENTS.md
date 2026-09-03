# StickMon 项目记忆

更新日期：2026-08-29。

StickMon 是运行在 ESP32-S3 掌机上的电子宠物游戏。同一仓库维护
M5StickS3、AMOLED 1.8 V1、AMOLED 1.8 V2 三个固件目标；玩法、存档、
资源格式和 ESP-NOW 协议必须共用，硬件差异只留在平台层与表现层。

## 开发原则

- 修改前先读工作区状态。仓库经常有用户未提交的功能，不得回退、覆盖或
  顺手格式化无关文件。
- 优先修改 `src/game/` 的共享规则或已有共享服务，不在场景或 AMOLED 工程
  中复制一份业务逻辑。
- StickS3 与 AMOLED 使用不同 UI 和输入状态机，但相同操作必须得到相同的
  GameState 结果。
- 当前 AMOLED 迁移仍在开发中：已通过编译不等于通过实机验收，尤其不能把
  V2、低功耗、音频和 ESP-NOW 联调描述为稳定能力。
- 项目默认使用 ASCII 代码与注释；已有中文 UI、文档和资源名称可保持中文。

## 硬件与构建

### M5StickS3

- PlatformIO + Arduino-ESP32，240x135，M5Unified/M5GFX。
- 8 MB Flash，LittleFS 资源，Preferences/NVS 存档；运行时优先使用 PSRAM。
- `m5stick-s3` 是 release：关闭 Debug 菜单、trace 和渲染统计。
- `m5stick-s3-debug` 开启 Debug 菜单与 trace 日志。
- 构建：`pio run -e m5stick-s3` 或 `pio run -e m5stick-s3-debug`。
- 固件和 LittleFS 一起刷入：`bash tools/upload_firmware_and_fs.sh`。
- 只刷固件或资源分别使用 `pio run -e m5stick-s3 -t upload`、
  `pio run -e m5stick-s3 -t uploadfs`。
- 若上传口被占用，先检查串口监视器、Chrome Web Serial 等进程；不要用提高
  上传速度掩盖连接问题，当前上传速度有意固定为 115200。

### AMOLED 1.8 V1

- 工程：`firmware/amoled_1_8_v1`，ESP-IDF 5.5.x。
- SH8601 + FT3168，物理 368x448；使用 184x224 RGB565 逻辑画布并 2x 输出。
- 使用 SPIFFS 资源、PSRAM、NVS A/B 存档和共享 ESP-NOW 协议。
- 构建：在工程目录加载 ESP-IDF 后执行 `idf.py -B build build`。
- 刷入：`./flash.sh --port PORT`；应用、bootloader、分区表和 resources.bin
  必须来自同一 build 目录。

### AMOLED 1.8 V2

- 工程：`firmware/amoled_1_8_v2`，ESP-IDF 5.5.x。
- CO5300 + CST820，物理/逻辑分辨率与 V1 相同。
- 应用与渲染复用 V1 的 `AmoledApp`、`HomeScreen`；V2 只保留 `main.cpp`、
  `AmoledPlatform.*`、`TouchInput.*` 等板级差异。
- 构建：`idf.py set-target esp32s3 && idf.py build`。
- 当前状态是可编译、待 V2 实机验收，不能凭 V1 结果推断 V2 硬件正确。

## 架构边界

- `src/game/`：纯玩法、数值和状态变更，不能依赖 Arduino、M5 或 ESP-IDF。
- `src/core/`：应用协调、存档、场景流程和跨平台会话。
- `src/platform/api/PlatformServices.h`：硬件服务边界。
- `src/platform/m5stick_s3/`：StickS3 平台实现。
- `src/scenes/`：StickS3 横屏场景和交互，不应拥有可共享的业务规则。
- `firmware/amoled_1_8_v1/main/`：AMOLED 竖屏 UI、触摸状态机和 V1 平台层。
- `firmware/amoled_1_8_v2/main/`：V2 板级适配，应用代码通过 V1 工程复用。
- `src/presentation/`：Canvas565、像素绘制和可跨目标使用的表现工具。
- `src/brain/`：ESP-Claw 远程聊天桥接（开发中、未提交）。硬依赖 ESP-IDF、
  FreeRTOS 和外部 esp-claw 头文件，是 `src/` 下唯一的 ESP-IDF-only 目录；
  只被 AMOLED 固件编译，PlatformIO 的 `build_src_filter` 已排除该目录。
  它不拥有游戏规则，状态变更全部经 `HostAdapter` 回调委托给 `AmoledApp`，
  再走现有共享服务。
- `firmware/common/components/`：把外部 esp-claw 源码与 `src/brain` 包装成
  IDF 组件（`app_claw`、`stickmon_claw`），V1/V2 通过顶层 CMakeLists 的
  `EXTRA_COMPONENT_DIRS` 共用。

## ESP-Claw 远程聊天（开发中，未提交）

工作区有一批未提交的 ESP-Claw 集成（外部 AI agent 框架：WeChat iLink +
Coze Agent 远程读取/操作游戏状态），只接入 AMOLED V1/V2。

- `src/brain/`：`BrainBridge`（action 排队到 UI 任务执行、Snapshot、
  HostAdapter 回调和玩家优先仲裁）、`StickmonClawRuntime`（从 NVS 读凭据、
  WiFi STA、挂载 brainfs、启动 esp-claw、提供按需配置热点和微信二维码登录）、
  `StickmonCapability`（注册 `stickmon_get_context/get_inventory/say/eat/`
  `buy_food/start_expedition/return_home/invite_friend` 八个 LLM 工具，写操作
  带 RESTRICTED 标记）。
- 外部 esp-claw 源码不在仓库内：默认路径 `../espclaw/esp-claw-master`，
  可用环境变量 `STICKMON_ESPCLAW_ROOT` 覆盖；缺失时仅 warning，远程聊天
  不可用但固件仍可构建。
- 编译开关 `CONFIG_STICKMON_CLAW_ENABLE`（各工程 `main/Kconfig.projbuild`，
  默认 y）；凭据从 NVS 读取：`wifi` 命名空间 ssid/password，`claw` 命名空间
  `coze_token/coze_bot_id/coze_base_url`、IM 渠道列表 `im_channels`
  （逗号分隔 wechat/telegram/qq/feishu，旧存档缺失时按已存 token 推导）
  和各渠道键：`wechat_token/wechat_base_url/wechat_cdn_url/wechat_acct_id`、
  `telegram_token`、`qq_app_id/qq_app_secret`、`feishu_app_id/feishu_secret`（页面字段仍显示
  `feishu_app_secret`，内部键名遵守 NVS 15 字符限制）。
  门户删除渠道后点「保存配置」会清掉该渠道全部 NVS 键；`fillClawConfig`
  按渠道列表与已编译 cap（`CONFIG_APP_CLAW_CAP_IM_*`）生成
  `enabled_cap_groups`。
- 两个 AMOLED 工程分区表各新增 `brainfs` FATFS 分区（0xa30000、
  0x5d0000），flash 已用满 16 MB；`SPIFFS_MAX_PARTITIONS` 2→3。
- `tools/generate_claw_nvs.py` 生成 0x6000 完整 NVS 镜像并整分区覆盖，
  会清掉游戏存档：只用于新设备首刷，旧设备先备份 NVS 再写入。
- 2026-08-29 已处理的历史问题：PlatformIO 曾在 `build_src_filter` 漏排
  `src/brain` 导致 StickS3 构建失败（已加 `-<brain/>`）；两个 AMOLED 工程的
  `dependencies.lock` 曾写入 esp-claw 的个人绝对路径（已清理；本机构建时组件
  管理器会重新生成该 local 条目，属正常现象，不要再提交）。
- 远程动作经 `BrainBridge::enqueue` 排队到 UI 任务执行，默认等待 5000ms；
  超时不代表动作未执行，host 回调对重复状态转换是幂等拒绝的，LLM 重试安全。
- `电脑 -> ESP-Claw` 设置页（仅 AMOLED，`STICKMON_HAS_CLAW=1` 时编译）：
  启动按需 WPA2 热点 + HTTP 配置门户；页面二维码是运行时生成的 Wi-Fi
  配对码（`src/presentation/QrCodeGen.*`，字节模式/ECC L/mask 0/v1-5，
  带 4 模块静区），热点密码每次会话由 `esp_random()` 随机生成。golden
  测试：`python3 tools/test_qr_code_gen.py`。
- 连接状态日志：`src/brain/ClawStatusLog.*` 是 64 条环形缓冲（每条 63
  字节，超长按 UTF-8 边界拆成续行，不截断），由 `ClawRuntime` 用现有
  mux 串行化写入；设备页「连接/日志」双 tab 与门户 `/api/log?since=`
  共用同一份数据源，手机加入热点后设备页自动切到日志视图。
  host 测试：`python3 tools/test_claw_status_log.py`。

优先复用的共享服务包括：

- `ExperienceService`：经验、等级和 HP 成长；进化、学招式、存档和 UI 由调用方负责。
- `FriendshipService`：战后结交、保底、通讯录和邀请判定。
- `MonsterFactory`：新精灵的统一运行时状态。
- `MoveManagementService`：技能读取、遗忘和心之鳞片回忆。
- `BathService`：洗澡阶段奖励、肥皂消耗和按评分恢复 HP。
- `ItemInventory`、`ShopService`、`TeamRoster`、`HomeCare`、`CareTicker`。
- `VisitSessionService`：拜访会话；`AppSceneFlow`：跨固件语义场景与菜单顺序。

增加新流程时，先问“状态变更能否做成不依赖 UI 的共享服务”；可以的话先写
共享服务与 host 测试，再接 StickS3/AMOLED 表现。

## 存档兼容

- 当前 `Game::SAVE_VERSION = 3`，以源码为准；
  `doc/存档兼容与迁移规则.md` 可能落后，修改前必须同时读
  `GameState.h`、`SaveCodec.*`、`SaveManager.*`。
- 新写入使用 `SaveCodec` 显式小端字段编码，不允许直接把 C++ struct 布局当作
  新格式持久化。
- 快照带 schema、sequence 和 CRC；NVS 使用 `state_a`/`state_b` 双槽，读取
  sequence 最大且校验通过的记录，同时保留 `state` 兼容镜像。
- 当前支持 v1/v2/v3 读取和迁移。迁移必须先成功写入新格式，才能视为完成；
  写入失败时保留旧 blob 以便下次重试。
- 遇到高于当前固件版本的存档必须返回 `NEWER_VERSION` 并写保护，绝不能把
  默认状态覆盖回去。
- `GameState` 字段变化必须显式评估 schema 和迁移。不要依赖 padding 偷塞字段，
  不要仅修改 `sizeof` static_assert 让构建通过。
- M5StickS3 与 AMOLED 必须能读写同一种逻辑快照。平台只实现 blob 存储，
  不得各自定义存档结构。

## 绘制与性能契约

### StickS3

- 场景使用 `SceneUpdateResult` 与 `RenderDemand` 上报真实需求。
- `parked()`：无重绘、无定时更新，等待输入、切场景或外部状态唤醒。
- `frame()`：只重绘当前一帧，不自动安排下一次更新。
- `animate(delay)`：当前帧重绘，并按 delay 持续调度。
- `pollAfter(delay)`/`wakeIn()`：只安排逻辑检查，不代表需要重绘。
- 重绘请求与下一次更新时间是两个独立维度。静态菜单、设置和弹窗不得用
  持续动画掩盖漏掉的状态通知。
- 主界面、探索移动等动态场景可持续刷新，但路径规划、资源解码和全池扫描
  不能放进逐帧热路径。

### AMOLED

- 使用纵向脏区传屏；`AmoledApp::needsRender()` 与 dirty row 范围决定传输。
- 静止页面、暂停探索和静止洗澡页面不得持续全屏刷新。
- 页面切换/唤醒可全帧刷新，局部交互应只标记受影响行。
- 184x224 是 UI 坐标空间，368x448 只用于板级 2x 传输，业务布局不得混用
  物理坐标。

## 关键玩法不变量

- 队伍上限为 2，仓库上限为 20；`activeSlot` 是当前出战位，不等于永久队序。
- 战斗中“替换”只改变本场出战精灵，不改变队伍顺序；队伍页的“首位”才会
  调整正式队序。
- 访客可临时进入队伍/房间，但 `Origin::VISITOR` 的邀请、喂食、首位和离队
  行为必须走现有访客规则，不能按永久成员处理。
- 游戏初始时间为 07:00，初始金币 1000。
- 普通粮每份 3 口；碗容量为一份。具体饱腹、口味和经验使用 `HomeCare` 与
  食物 profile，不要在场景中重复常量。
- 居家 HP 每个游戏内分钟恢复最大 HP 的 1%；饥饿为 0 时每 tick 只恢复 1 HP。
  濒死休息按游戏时间 60 分钟结算，并清除 major status。
- 精灵技能共 3 槽：第 1 槽不可替换，两个特殊槽可遗忘/替换。学新招式、
  升级、进化等队列事件必须由所有经验来源统一触发。
- 洗澡完成按 1/2/3 星恢复 45%/70%/100% HP；阶段经验和心情奖励统一走
  `BathService`。
- 区域按击败前一区域头目顺序解锁；探索精灵池缓存直到时间轮换或头目刷新，
  不应在每次进入菜单时重新解码。

## 资源管线

- 源资源：`origin_asset/`；开发包输出：`data/packs/dev/`。
- StickS3 从 LittleFS 读取，AMOLED 从 SPIFFS 读取；资源格式和 pack 内容共用。
- 精灵图：`python3 tools/generate_pokemon_sprites.py`，输出
  `data/packs/dev/sprites/*.smonsp` 并更新 `PokemonSprites.h/.cpp`。
- 游戏资源：`python3 tools/generate_game_assets.py`，输出
  `data/packs/dev/game/{ui,battle,maps,hatch}.smonfx`。
- 字体：`tools/generate_font16cn.py`，共享 `zh16.smonfont` 与
  `ascii16-unscii.smonfont`。
- 外部 Pokemon Essentials 通过 `ESSENTIALS_DIR` 指定，或放在 git 忽略的
  `external/pokemon-essentials`；禁止提交个人绝对路径和受版权保护的外部仓库。
- 地图使用运行时 tile atlas，不要恢复为整张地图位图缓存。

### GameAssets 不变量

- `GameAssets.h::Kind` 与 `generate_game_assets.py::KIND_ORDER` 必须逐项同序；
  在枚举中间插入 Kind 后必须全量重生成四个包。
- 生成器与 `GameAssets.cpp` 两侧共同校验上限：256 帧、200000 data words、
  2048 palette words、384000 payload；修改时必须同步。
- `GameAssets.cpp::packSlotFor` 决定路由：UI 段与探索拾取球进 UI，
  `EXPLORE_TILE_*` 进 MAP，蛋进 HATCH，其余默认 BATTLE。
- 背景类 StickS3 资源为 240x135、LANCZOS 缩放、16 色量化；AMOLED 竖屏
  表现可裁切/重排，但不能修改共享 Kind 语义。
- 精灵 FRONT 的不放大名单在
  `generate_pokemon_sprites.py::ENEMY_FRONT_NO_UPSCALE_SPECIES`；修改会同时影响
  战斗、洗澡和探索预览，必须用真实设备或导出图检查像素清晰度。

## 验证

- 窄改动优先运行对应 `tools/test_*.py`，这些测试会按需编译相邻 `*_host.cpp`。
- 共享玩法/存档修改至少运行相关 host 测试；存档修改必须运行：
  `python3 tools/test_save_codec.py` 和 `python3 tools/test_save_migration.py`。
- 场景调度修改运行 `python3 tools/test_render_demand.py`；共享边界修改运行
  `python3 tools/test_architecture_boundaries.py`。
- 资源修改运行对应生成器测试，并重新生成受影响 pack；地图算法的 Python/C++
  顺序、PRNG 调用和指纹必须保持一致。
- 合并前至少构建受影响固件目标。触及共享源码时应构建 StickS3 和 AMOLED V1；
  若 V2 板级/共享 AMOLED 代码受影响，也构建 V2。
- 不能运行实机测试时必须明确记录。V2 显示/触摸、三目标 ESP-NOW、音频、
  PMU 深睡、电池/IMU/RTC 仍属于重点实机风险。

## 参考文档

- `doc/StickMon-开发计划.md`：总体玩法与资源方案，部分早期参数可能落后。
- `doc/ESP32-S3-Touch-AMOLED-1.8迁移与双版本兼容方案.md`：三目标架构与迁移状态。
- `firmware/amoled_1_8_v1/README.md`、`firmware/amoled_1_8_v2/README.md`：
  AMOLED 当前能力、构建和待验收项。
- 任何文档与源码冲突时，以代码、测试和最新实机日志为准，并同步修正文档。
