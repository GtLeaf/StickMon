# StickMon 项目记忆

ESP32-S3（M5StickS3）电子宠物游戏。PlatformIO + Arduino 框架，屏幕 240×135（`Hal::DISPLAY_W/H`），资源经 LittleFS 加载，PSRAM 可用时优先用 PSRAM。

## 构建与上传

- 编译：`~/.platformio/penv/bin/pio run`（系统无全局 pio，penv 里有；环境名 `m5stick-s3`）。
- 上传固件 + 文件系统：`bash tools/upload_firmware_and_fs.sh`；只改固件用 `pio run -t upload`，只改资源用 `pio run -t uploadfs`。
- 串口 `/dev/cu.usbmodem2101`。若 esptool 报 "the port doesn't exist"，先 `lsof /dev/cu.usbmodem2101` —— 常见原因是 Chrome 的 Web Serial 页面（web-flasher 等）占用端口，真实错误是 Resource busy，关掉对应标签页即可。

## 资源管线（origin_asset → data/packs/dev → LittleFS）

- 游戏资源（道具/球/战斗背景/探索 tile/浴室等）：`python3 tools/generate_game_assets.py`，输出 `data/packs/dev/game/{ui,battle,maps,hatch}.smonfx`。
- 精灵图：`python3 tools/generate_pokemon_sprites.py`，输出 `data/packs/dev/sprites/*.smonsp` 并重写 `src/assets/PokemonSprites.h/.cpp`。
- 两条管线都依赖 Essentials 源图。默认从未跟踪的 `external/pokemon-essentials` 读取，也可通过 `ESSENTIALS_DIR` 环境变量指定外部目录。
- 测试：在 `tools/` 目录下跑 `python3 -m unittest test_generate_game_assets` / `test_generate_pokemon_sprites`（系统 python3 已装 PIL）。

### 必须遵守的不变量

- `src/assets/GameAssets.h` 的 `Kind` 枚举顺序与 `generate_game_assets.py` 的 `KIND_ORDER` 一一对应（生成器会校验）。**在枚举中间插入新 Kind 会使后续所有 Kind 的编号后移，必须全量重新生成四个包**（ui 包含末尾的 EXPLORE_PICKUP_BALL，也受影响）。
- 包上限（256 帧、200000 data words、2048 palette words、384000 payload）生成器和 `GameAssets.cpp` 双侧校验，改一侧必须同步另一侧。
- pack 路由在 `GameAssets.cpp::packSlotFor`：`<= SHOWER_BACKGROUND` 和 EXPLORE_PICKUP_BALL 走 UI 包；EXPLORE_TILE_* 段走 MAP；EGG 走 HATCH；其余默认 BATTLE。新增 Kind 落在哪段就进哪个包。
- 背景类资源统一 240×135、LANCZOS 缩放、量化 16 色，运行时用 `GameAssets::drawBattleBackground(kind)` 全屏绘制。

## 关键设计决定（勿回退）

- **探索菜单背景**：`EXPLORE_MENU_BACKGROUND`（BATTLE 包，枚举在 `BATTLE_BG_SNOW` 之后），源图 `origin_asset/mainScreen/menu/explore/bg_explore.png`，由 `ExploreScene::renderAreaMenu()` 绘制，加载失败回退黑色。
- **精灵 FRONT 不放大名单**：`generate_pokemon_sprites.py::ENEMY_FRONT_NO_UPSCALE_SPECIES`（妙蛙种子/杰尼龟/绿毛虫/铁甲蛹/波波/小拳石/皮丘/玛力露/乌波/土狼犬/拉鲁拉丝/蘑蘑菇/雪童子）。这些精灵基准高不足 48px 时需 1.33 倍以上 NEAREST 放大，与源图 2px 像素网格错位导致肉眼发糊，故保持 0.45 倍网格对齐基准尺寸。战斗/洗澡/探索预览共用同一 FRONT 帧，自动同步生效；副作用是这些精灵显示偏小。
- **洗澡爱心**：`ShowerScene` 冲水结束按已完成步骤（肥皂/刷出全身泡沫/冲水）计 1~3 个 `completionHearts`；3 个播放精灵叫声（CryPlayer）、2 个跳一下、1 个无动作。无肥皂冲水也能完成（1 爱心），且会照常发 RINSE 阶段 EXP/心情奖励——这是有意放宽的。

## 代码结构速查

- 场景在 `src/scenes/`（Main/Menu/Explore/Shower/Shop/HatchScene），统一 `Scene` 基类 + `GameEngine` 调度。
- 探索区域配置在 `ExploreScene.cpp` 的 `ROUTE_MAPS` 表（名称字符串在 `UiStrings.h::Ui::Explore`，顺序有 static_assert 保证一致）。
- 设计文档：`doc/StickMon-开发计划.md`（含资源管线详细说明，§6.1.2）。
