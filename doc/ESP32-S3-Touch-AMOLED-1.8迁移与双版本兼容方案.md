# StickMon 迁移至 ESP32-S3-Touch-AMOLED-1.8 及 V1/V2 兼容方案

记录日期：2026-08-26

## 1. 目标与结论

本方案用于将当前运行在 M5Stack StickS3 上的 StickMon 迁移到 Waveshare `ESP32-S3-Touch-AMOLED-1.8`，并持续支持以下三个硬件目标：

- M5Stack StickS3：保留现有版本，继续接收业务功能更新。
- ESP32-S3-Touch-AMOLED-1.8 V1：当前实机，优先完成迁移和验证。
- ESP32-S3-Touch-AMOLED-1.8 V2：保持可编译、协议兼容，取得实机后完成硬件验收。

总体决策：

1. 所有目标放在同一个 Git 仓库、同一个主分支中维护。
2. 游戏业务、存档格式、资源格式和 ESP-NOW 协议只保留一份实现。
3. StickS3、AMOLED V1、AMOLED V2 分别生成独立固件，不使用硬件长期分支。
4. AMOLED V1/V2 共用同一套 `184x224` 逻辑竖屏 UI 和资源，仅显示、触摸驱动不同。
5. V1 和 V2 初期不追求同一个二进制自动识别硬件；通过构建目标选择版本，降低错误初始化屏幕的风险。
6. AMOLED 固件采用 ESP-IDF；现有 StickS3 固件暂时保留 PlatformIO + Arduino，不强制同步更换框架。

推荐形态可以概括为：**一个仓库、一套业务内核、三种固件产物、两套 UI 布局、三个平台适配器。**

## 2. 当前项目基础

当前代码已经具备迁移所需的大部分架构基础：

- `src/game/` 中的战斗、养成、探索、物种和数值规则不依赖 Arduino、M5 或 ESP-IDF。
- `src/platform/api/PlatformServices.h` 已抽象时钟、日志、输入、显示、音频、IMU、电源、存储、资源、内存和点对点通信。
- `src/platform/m5stick_s3/` 集中了 M5Unified、M5GFX、Preferences、LittleFS 和 ESP-NOW 的具体实现。
- `Canvas565` 和 `PixelRenderer` 已可直接操作抽象 RGB565 framebuffer，不再要求使用 M5GFX 绘图 API。
- `EspNowLink` 的协议和会话逻辑已与底层 ESP-NOW API 分离，通过 `IPeerTransport` 收发数据。
- 已有 DesktopPlatform 和主机端回归测试，可用于保护迁移过程中的业务行为。

现阶段仍有三个主要耦合点：

1. 逻辑显示尺寸全局固定为 `240x135`。
2. 输入模型固定为 A/B 两键。
3. 场景中存在大量面向 `240x135` 横屏的像素坐标和全屏背景资源。

因此迁移不需要重写游戏，但需要扩展平台接口，并为 AMOLED 建立新的竖屏表现层。

## 3. AMOLED V1/V2 硬件差异

根据 Waveshare 官方文档和官方示例仓库，两个版本的关键差异如下：

| 项目 | V1 | V2 | 对 StickMon 的影响 |
|---|---|---|---|
| AMOLED 控制器 | SH8601 | CO5300 | 初始化命令和面板驱动不同 |
| 触摸控制器 | FT3168 | CST820 | I2C 触摸驱动和坐标修正不同 |
| 显示接口 | QSPI | QSPI | 平台接口可保持一致 |
| 触摸接口 | I2C | I2C | 输入事件模型可保持一致 |
| 分辨率 | 368x448 | 368x448 | 共用同一套 UI 与资源 |
| 主控 | ESP32-S3R8 | ESP32-S3R8 | 业务代码无差异 |
| PSRAM / Flash | 8MB / 16MB | 8MB / 16MB | 共用内存和分区策略 |
| PMU | AXP2101 | AXP2101 | 共用电源服务 |
| IMU | QMI8658 | QMI8658 | 共用动作和计步服务 |
| RTC | PCF85063A | PCF85063A | 共用游戏时钟和低功耗策略 |
| 音频 | ES8311 + 麦克风/扬声器 | ES8311 + 麦克风/扬声器 | 共用音频服务 |
| TF 卡 | SDMMC | SDMMC | 共用可选扩展存储服务 |
| 无线 | 2.4GHz Wi-Fi / BLE / ESP-NOW | 相同 | 通信协议完全共用 |

注意：官方资料确认 V2 实际装配的是 `CST820`。部分官方源码仍使用 `CST816S` 或 `Arduino_CST816x` 族名称，这是兼容驱动/API 的命名，不应据此把硬件型号写成 CST816S。

官方 ESP-IDF BSP 的主版本也体现了硬件代际：V1 使用 BSP `^1` 系列，V2 使用 BSP `^2` 系列。当前官方组件最新版本为 `2.0.3`，其面板实现面向 CO5300，触摸侧会探测 CST816S 兼容设备或 FT5x06 族设备。因此初期应为 V1/V2 固定各自 BSP/驱动依赖，不直接假设一个 BSP 版本能够可靠初始化两种面板。

## 4. 仓库与工程组织

### 4.1 为什么保持一个 Git 仓库

三个硬件版本共享以下高频变化内容：

- GameState、精灵数据和成长规则。
- 场景流程、经济系统、探索和战斗。
- 存档 schema 与升级逻辑。
- ESP-NOW 对战、交换、拜访和通讯进化协议。
- 精灵图、字体、音频和资源生成工具。
- 主机端测试与协议兼容测试。

如果拆成多个仓库，任何业务修改都需要跨仓库同步，容易出现存档版本、通信消息和游戏数值不一致。硬件适配不足以构成拆仓理由。

只有未来 AMOLED 版本成为独立产品、玩法和数据格式明确分叉时，才考虑拆仓。在当前目标下不拆仓。

### 4.2 推荐最终目录

```text
stickMon/
  shared/
    stickmon/
      domain/                 # 纯业务规则与数据结构
      application/            # GameEngine、用例、场景状态和存档协调
      protocol/               # ESP-NOW 帧、会话、重试和事务协议
      presentation/
        common/               # Canvas565、PixelRenderer、字体、动画工具
        landscape_240x135/    # StickS3 横屏布局
        portrait_184x224/     # AMOLED V1/V2 共用竖屏布局
      resources/              # 资源包、解压和资源索引

  firmware/
    m5stick_s3/
      platformio.ini
      src/main.cpp
      platform/M5StickS3Platform.*

    amoled_1_8/
      common/                 # AXP2101、QMI8658、PCF85063、ES8311、SD、NVS、ESP-NOW
      v1/
        CMakeLists.txt
        main/app_main.cpp
        main/idf_component.yml
        platform/AmoledV1Display.*
        platform/Ft3168Touch.*
        sdkconfig.defaults
      v2/
        CMakeLists.txt
        main/app_main.cpp
        main/idf_component.yml
        platform/AmoledV2Display.*
        platform/Cst820Touch.*
        sdkconfig.defaults

  simulator/
  assets/
    source/
    generated/
      common/                 # 精灵、音频、字体和数据
      landscape_240x135/      # StickS3 全屏背景
      portrait_184x224/       # AMOLED V1/V2 全屏背景
  tools/
  tests/
  doc/
```

迁移期间不做一次性大规模移动。第一阶段可以保留现有 `src/`，新增 AMOLED 工程并引用现有共享源码；两个目标稳定后再逐步整理到最终目录。

### 4.3 分支和版本策略

- `main` 必须同时满足 StickS3、AMOLED V1、AMOLED V2 的构建要求。
- 不创建长期 `v1`、`v2` 或 `amoled` 分支。
- 硬件差异通过独立构建目标和平台适配器表达。
- 游戏版本号统一，例如 `StickMon 0.8.0`。
- 发布产物分别命名：
  - `stickmon-0.8.0-m5stick-s3.bin`
  - `stickmon-0.8.0-amoled-1.8-v1.bin`
  - `stickmon-0.8.0-amoled-1.8-v2.bin`
- Web Flasher 或发布页必须要求用户按设备背面标签选择 V1/V2，避免刷错固件。

## 5. 技术栈

### 5.1 StickS3

迁移期间保留现状：

- PlatformIO
- Arduino-ESP32 2.0.14
- M5Unified 0.2.17
- M5GFX 0.2.24
- LittleFS + Preferences/NVS

这样不会因为 AMOLED 迁移同时引入 StickS3 框架升级风险。

### 5.2 AMOLED V1/V2

建议共同使用：

- ESP-IDF 5.5.x，基线固定到团队验证过的具体补丁版本。
- C++17 业务代码。
- `esp_lcd` / `esp_lcd_touch` 显示和触摸接口。
- `nvs_flash` 保存小型持久化数据。
- SPIFFS 或 LittleFS 保存游戏资源；后续可增加 TF 卡覆盖包，但不作为启动依赖。
- `esp_now` 实现设备间通信。
- `esp_codec_dev` / I2S 驱动 ES8311 音频。

版本差异：

- V1：SH8601 + FT3168 驱动，优先参考 Waveshare V1 BSP `^1` 和原版官方示例。
- V2：CO5300 + CST820 驱动，固定 Waveshare BSP `^2.0.3` 或后续经过验证的 `2.x` 版本。

LVGL 不作为 StickMon 游戏画面的必选依赖。当前 `Canvas565 + PixelRenderer` 已适合 15fps 像素游戏，可以直接提交 RGB565 framebuffer。LVGL 只在未来系统设置、OTA、文件管理等复杂控件确有收益时引入，避免 V1/V2 的 LVGL 和 BSP 版本反过来限制业务层。

## 6. 平台接口调整

### 6.1 显示指标动态化

移除平台 API 中固定的 `LOGICAL_DISPLAY_W/H`，改为：

```cpp
enum class DisplayProfile : uint8_t {
    LANDSCAPE_240X135,
    PORTRAIT_184X224,
};

struct DisplayMetrics {
    uint16_t logicalWidth;
    uint16_t logicalHeight;
    uint16_t physicalWidth;
    uint16_t physicalHeight;
    uint8_t integerScale;
    DisplayProfile profile;
};
```

AMOLED V1/V2 都使用：

- 物理分辨率：`368x448`
- 逻辑分辨率：`184x224`
- 缩放倍率：`2x`
- 像素格式：RGB565

采用整数缩放可以保持像素画边缘清晰，V1/V2 不需要分别生成资源。

### 6.2 输入改为语义事件

现有 `InputButton::PRIMARY/SECONDARY` 只适合 StickS3。建议增加统一事件：

```cpp
enum class InputAction : uint8_t {
    CONFIRM,
    BACK,
    NEXT,
    PREVIOUS,
    MENU,
    POINTER_DOWN,
    POINTER_MOVE,
    POINTER_UP,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    LONG_PRESS,
};

struct InputEvent {
    InputAction action;
    int16_t x;
    int16_t y;
    uint32_t timestampMs;
};
```

映射原则：

- StickS3 A/B 映射为场景需要的 `CONFIRM/NEXT/BACK/MENU`。
- AMOLED 触摸点击、滑动和长按映射为相同语义动作。
- PWR/BOOT 作为 AMOLED 的备用返回、唤醒或调试输入。
- 核心操作只依赖单点触摸，不依赖多点手势，保证 FT3168 和 CST820 行为一致。

### 6.3 硬件能力表

平台启动后向业务层提供能力，而不是在场景中判断宏：

```cpp
struct PlatformCapabilities {
    bool touch;
    bool microphone;
    bool speaker;
    bool imu;
    bool rtc;
    bool removableStorage;
    bool peerTransport;
};
```

业务层只根据能力决定是否显示增强功能，不包含 `#ifdef AMOLED_V1` 等硬件判断。

## 7. V1/V2 驱动兼容策略

### 7.1 推荐：两个固件目标

V1/V2 使用相同的 `AmoledPlatform` 服务组合，但替换两个子组件：

```text
AmoledPlatform
  common services
    clock / logger / NVS / resource store
    AXP2101 / QMI8658 / PCF85063A
    ES8311 / SDMMC / ESP-NOW
  board-specific services
    V1: SH8601 display + FT3168 touch
    V2: CO5300 display + CST820 touch
```

两个 `app_main.cpp` 应保持极薄，只负责绑定对应平台服务并启动同一个 `GameEngine`。

优点：

- 不会向错误面板发送另一型号的初始化命令。
- V1/V2 可以分别固定官方 BSP 和依赖版本。
- 固件体积不携带无用的另一套面板驱动。
- 发生显示或触摸回归时，问题范围明确。
- 用户可以通过设备背面标签可靠选择固件。

### 7.2 暂不采用：单固件运行时自动识别

理论上可以先扫描 I2C 触摸控制器，再据此选择显示控制器，但不作为首版方案，原因包括：

- 官方 V2 代码可能使用 CST816S 兼容驱动名称，芯片命名和识别逻辑容易混淆。
- 触摸设备无响应时无法安全推断面板型号。
- 两代 BSP 的显示依赖和初始化流程不同。
- 自动识别失败可能导致黑屏，而不是可恢复的功能降级。

后续如果有大量混合设备需要统一分发，可以在两个固件均稳定后增加统一引导固件，或开发包含双驱动的自动检测版本；这不是当前迁移的阻塞项。

## 8. 业务逻辑迁移范围

| 当前模块 | 迁移方式 | 说明 |
|---|---|---|
| `src/game/**` | 直接共享 | 战斗、养成、探索、精灵数据保持唯一实现 |
| `GameRandom`、`GameClockService`、`CareTicker` | 直接共享 | 继续使用主机测试验证确定性 |
| `Canvas565` | 直接共享 | framebuffer 尺寸改为运行时获取 |
| `PixelRenderer` | 基本共享 | 字体、图元和 RLE 绘制继续使用 |
| 资源包、解压、字体 | 直接共享 | 全屏背景按布局 profile 分包；V1/V2 共用中文和 ASCII 字库 |
| `EspNowLink` | 共享并移动到 protocol | 不再放在 hardware 命名空间 |
| `GameEngine` | 小幅重构 | 去掉固定显示尺寸和原始按钮假设 |
| `SaveManager` | 保留业务、重做 codec | 避免跨编译器直接保存结构体 |
| Scenes 状态机 | 共享 | 输入处理逐步改为语义动作 |
| Scenes 绘制坐标 | 分布局实现 | StickS3 横屏和 AMOLED 竖屏分别布局 |
| M5StickS3Platform | 保留 | 继续维护现有固件 |
| AMOLED 平台适配 | 新增 | V1/V2 仅显示、触摸不同 |

预期复用比例：

- 业务规则和数据：90% 以上。
- 通信协议：80% 以上。
- 资源加载和像素绘制：70% 以上。
- 场景状态与流程：60%～80%。
- UI 像素布局：需要针对竖屏重新设计。
- 硬件驱动：AMOLED 新写。

当前字库落地方式：`data/packs/dev/fonts/zh16.smonfont`（中文，16x16）和
`data/packs/dev/fonts/ascii16-unscii.smonfont`（ASCII，16x16）由 V1/V2 的
`CMakeLists.txt` 自动复制到 SPIFFS 资源分区。启动时 `FontResource` 按 manifest
加载两套字形；AMOLED 页面检测到 UTF-8 文案后使用共享 `PixelRenderer` 绘制，英文
标签仍使用页面原有的紧凑字形。新增可见中文时只需更新共享字库并重新构建两个固件，
不复制字体文件，也不在 V1/V2 之间维护不同的字形版本。

## 9. 存档兼容方案

当前 SaveManager 将包含 `GameState` 的 C++ 结构整体写入 blob。Arduino-ESP32 2.x 与 ESP-IDF 5.x 使用不同工具链，结构体 padding 和后续字段变化存在兼容风险。

迁移前应增加共享 `SaveCodec`：

```text
SaveManager
  SaveCodec
    encode(GameState -> explicit binary schema)
    decodeV1
    decodeV2
    migrateV1ToV2
    validate + CRC
  IBlobStore
    StickS3 Preferences backend
    AMOLED NVS backend
```

要求：

- 所有字段使用明确的定长整数和固定字节序。
- schema 版本与游戏版本分开维护。
- 新版本至少能读取最近一个旧 schema。
- 存档使用 A/B 双槽；新槽完整写入并校验成功后再切换活动槽。
- `MainSceneViewState` 中与屏幕坐标相关的数据不能直接跨布局恢复；只共享业务状态，显示坐标应按目标布局重建或归一化保存。
- V1/V2 使用完全相同的存档 schema。
- StickS3 和 AMOLED 若未来通过 USB/ESP-NOW 迁移存档，也使用同一 codec，不直接复制 NVS blob。

## 10. ESP-NOW 兼容方案

三种设备使用同一套应用协议：

- StickS3 ↔ StickS3
- StickS3 ↔ AMOLED V1
- StickS3 ↔ AMOLED V2
- AMOLED V1 ↔ AMOLED V2

底层分别实现 `IPeerTransport`，上层 `EspNowLink` 不感知硬件版本。帧头至少包含：

- 协议 magic 和 protocol version。
- message type。
- session id。
- sequence number。
- payload length。
- CRC；涉及交换和奖励的消息增加会话认证字段。

兼容原则：

- 协议版本独立于固件版本。
- 未知消息必须忽略并返回明确的不兼容结果，不能崩溃。
- 对战和交换使用固定规则版本，连接时先协商 `protocolVersion` 和 `rulesVersion`。
- 交换继续采用 lock/commit/done 两阶段事务，避免断线复制或吞掉精灵。
- V1/V2 只交换结构化游戏数据，不交换屏幕资源。

## 11. 迁移实施步骤

### 阶段 0：冻结基线和建立构建矩阵

- 保证现有 `m5stick-s3` release/debug 构建持续通过。
- CI 增加主机端架构和业务回归测试。
- 为未来 AMOLED V1/V2 预留独立构建任务。
- 记录当前固件 Flash、RAM、PSRAM 和资源包基线。

验收：任何共享代码提交都必须运行 StickS3 构建和主机测试。

### 阶段 1：补齐共享边界

- 将显示尺寸改为 `DisplayMetrics`。
- 引入语义 `InputEvent` 和 `InputRouter`。
- 引入 `PlatformCapabilities`。
- 把 EspNowLink 从 hardware 概念迁移为共享 protocol/application 服务。
- 增加稳定的 SaveCodec 和旧存档读取测试。

验收：StickS3 行为不变，架构测试禁止 game/presentation 直接依赖具体硬件。

### 阶段 2：AMOLED V1 硬件冒烟测试

由于当前已有 V1 实机，先完成 V1：

- SH8601 RGB565 显示和亮度。
- FT3168 点击、按下、抬起、滑动和坐标校准。
- AXP2101 电池、电源键、屏幕休眠和唤醒。
- QMI8658 IMU。
- PCF85063A RTC。
- ES8311 扬声器和麦克风。
- NVS、资源分区和 PSRAM。
- ESP-NOW 广播、单播和收发回调。

验收：提供独立 board-check 固件，所有外设结果可通过串口和屏幕确认。

### 阶段 3：旧画面兼容运行

- 先在 AMOLED 上运行现有 `240x135` Canvas。
- 居中或旋转显示，仅作为迁移验证模式。
- 触摸屏提供虚拟 A/B 区域，跑通孵化、房间、菜单、探索、战斗、商店、设置和社交。
- 此阶段不重做美术，目标是证明业务、资源、存档和通信完整运行。

验收：完整单机闭环运行，重启后存档正常，V1 与 StickS3 可通过 ESP-NOW 发现和建会话。

### 阶段 4：原生 AMOLED 竖屏 UI

- 建立 `184x224` 逻辑画布并 2x 输出到 `368x448`。
- 按顺序迁移：主房间 → 菜单/设置 → 孵化/商店 → 探索/战斗 → 社交。
- 场景状态机继续共享，只替换布局和触摸命中区。
- 精灵、图标、字体和音频资源共享。房间继续使用同一份 `240x161`
  世界资源，AMOLED 通过 `184x148` 镜头裁剪显示，不维护第二套房间设计。
- V1/V2 资源分区同时打包 `zh16.smonfont` 和 `ascii16-unscii.smonfont`，中文
  精灵名、招式名和提示可以直接走共享字库渲染。

验收：无非整数缩放、文字不溢出、点击目标足够大、核心流程不依赖多点触控。

### 阶段 5：AMOLED V2 构建目标

- 接入 CO5300 和 CST820 适配器。
- 固定并锁定 BSP `2.x` 依赖。
- 共用 V1 的 `184x224` UI、资源、存档和 ESP-NOW 协议。
- 在没有 V2 实机时，CI 只声明“编译通过”，不能声明硬件通过。
- 获得 V2 实机后重复阶段 2 的 board-check 验收。

验收：V1/V2 固件均可构建；取得实机后完成颜色、偏移、触摸坐标、音频、电源和休眠测试。

### 阶段 6：跨设备联调与发布

- 完成 StickS3/V1/V2 的两两 ESP-NOW 测试。
- 验证协议版本不匹配、断线、重试和交易回滚。
- 验证三种固件读取相同测试存档后的业务状态一致。
- 发布三个带硬件标识的固件包和 manifest。
- Web Flasher 增加设备版本选择和刷写前确认。

## 12. CI 矩阵

每个合并请求至少包含：

| 任务 | 必须通过 | 是否需要实机 |
|---|---|---|
| 主机业务测试 | 是 | 否 |
| 架构边界测试 | 是 | 否 |
| 资源生成一致性测试 | 是 | 否 |
| StickS3 release 构建 | 是 | 否 |
| StickS3 debug 构建 | 是 | 否 |
| AMOLED V1 ESP-IDF 构建 | 是 | 否 |
| AMOLED V2 ESP-IDF 构建 | 是 | 否 |
| SaveCodec golden file 测试 | 是 | 否 |
| ESP-NOW 协议编解码测试 | 是 | 否 |
| V1 board-check | 发布前 | 是 |
| V2 board-check | 取得设备后、发布前 | 是 |
| 跨设备 ESP-NOW | 发布前 | 是 |

编译通过不能替代 V1/V2 面板、触摸、PSRAM、USB、音频和传感器的实机验证。

## 13. 风险与控制

### 13.1 V1 停产

V1 已停产，但当前有实机且需要维护。V1 驱动依赖应锁版本并保存可重复构建所需的组件版本，避免上游删除旧版本后无法构建。

### 13.2 官方驱动命名混淆

V2 物理芯片是 CST820，但官方代码可能使用 CST816S 兼容驱动 API。项目内部命名以产品硬件版本为准，例如 `AmoledV2Touch`，第三方驱动名称只出现在适配器内部。

### 13.3 场景布局工作量

业务逻辑可大量复用，但 `240x135` 横屏到 `184x224` 竖屏不是简单缩放。必须保留两套 UI 布局 profile，不应在场景中堆叠大量 V1/V2 宏。房间本身不属于 UI 布局：StickS3 和 AMOLED 共用 `240x161` 世界坐标、行走多边形和交互锚点，AMOLED 仅增加镜头。V1/V2 分辨率一致，因此 AMOLED 内部只有一套布局和镜头实现。

### 13.4 存档损坏或不兼容

在迁移到 ESP-IDF 前先建立明确的 SaveCodec、版本迁移和 golden file 测试；不要把当前内存结构直接当作长期磁盘格式。

### 13.5 BSP 升级回归

V1 和 V2 分别锁定依赖。只有 board-check、StickMon 冒烟测试和功耗测试都通过后才升级 BSP，不使用不受控的浮动 latest。

## 14. 首个可执行迭代

建议第一个迁移迭代只完成以下内容：

1. 新增 `DisplayMetrics`、`PlatformCapabilities` 和语义输入接口。
2. 保持 StickS3 功能和画面完全不变。
3. 建立 AMOLED V1 ESP-IDF board-check 工程。
4. 在 V1 上验证 SH8601、FT3168、AXP2101、PSRAM、NVS 和 ESP-NOW。
5. 将现有 `240x135` Canvas 输出到 V1 屏幕，并用触摸虚拟 A/B 跑通主菜单。
6. CI 增加 AMOLED V1/V2 编译占位和共享层边界测试。

完成这一步后，项目已证明迁移路径成立，再投入竖屏 UI 重构；不要在硬件链路尚未稳定前一次性重画所有场景。

## 15. 官方参考

- Waveshare 产品文档：<https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8>
- Waveshare ESP-IDF 文档：<https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.8/Development-Environment-Setup-ESP-IDF>
- Waveshare 官方示例仓库：<https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8>
- ESP Component Registry：<https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_8>

## 16. 当前实施进度（2026-08-26）

已完成：

- 新增独立 ESP-IDF 工程 `firmware/amoled_1_8_v1`，目标为原版 V1（SH8601 + FT3168）。
- 建立 `184x224` 逻辑画布，以 2x 整数缩放输出到 `368x448` AMOLED。
- 主界面、右上角菜单按钮、锁屏按钮、碗和精灵点击区域、可拖拽菜单已可运行。
- 复用共享 `.smonsp` 资源链，并在 V1 实机加载妙蛙种子资源。
- 通过 TCA9554 执行 V1 官方复位时序；FT3168 使用 GPIO21 中断读取，空闲时不再产生周期性 I2C 超时。
- 接入 ESP-IDF NVS、共享 `GameState` 和 `SaveManager`；等级、HP、心情、饱腹度、食物库存和碗状态可跨重启保存。
- 将摸摸与食物入碗规则提取到共享 `game/HomeCare`，M5StickS3 与 AMOLED 使用同一实现。
- 将吃食物的口数、口味、饱腹度和心情结算提取到共享 `game/HomeCare`，两套固件不重复维护数值规则。
- AMOLED 已接入共享 `GameClockService`、`CareTicker` 和 `MonsterMind`：游戏时间按设置倍率推进，饥饿/心情/居家 HP 恢复和每日计数会更新，精灵可自主移动并在饥饿时走向碗自动进食。
- 复用共享 `standard.smonroom` 的 `240x161` 房间世界；AMOLED 主界面使用
  `184x148` 镜头和安全区跟随，精灵、碗、行走多边形和触摸命中继续使用世界坐标。
- 主界面运动区域逻辑已提取为共享 `core/RoomMovementArea`。StickS3 与 AMOLED
  使用同一份房间行走多边形、精灵占地采样和路径分段校验；AMOLED 会按当前
  精灵帧尺寸计算占地半径，初始化站位、随机漫游、进食站位和移动过程均不能
  穿出区域。端点都合法但中间穿过凹形边界的直线路径也会被拒绝。
- 已实现纵向脏区的按需绘制和按需传屏：时间栏 `0..24`、房间
  `24..172`、状态栏 `172..224`、菜单内容 `28..224`；页面切换和唤醒才刷新全屏。
  以 RGB565 物理传输量计，单独刷新顶部、房间和状态栏约为全屏的
  `10.7%`、`66.1%` 和 `23.2%`。
- V1 实机已验证房间从 SPIFFS 加载为 `240x161`，日间房间首次解压约
  `89 ms`，精灵资源加载约 `25 ms`；启动后没有重复解压、崩溃或周期性 I2C 超时。
- AMOLED 固件 CPU 已提升到 `240 MHz`；显示传输使用两块 DMA 缓冲和
  `32` 个逻辑行（`64` 个物理行）条带，在发送上一条带时并行完成下一条带的
  2x 放大。V1 实机完整 `368x448` 帧传输约 `21.6 ms`，两块缓冲占
  `94,208` 字节，分配后仍有约 `238 KB` DMA 内存。
- 已迁移原生竖屏探索区域选择页：主菜单的 `EXPLORE` 可进入区域列表，列表
  支持拖拽和惯性滚动，按共享进度展示已解锁区域和下一个锁定区域，并支持
  区域选择、返回及右上角菜单。探索菜单返回时会恢复探索页，而不是跳回房间。
- 六个区域的推荐等级已提取到共享 `game/ExploreAreaCatalog`，StickS3 横屏探索
  与 AMOLED 竖屏列表使用同一份数据；连续 Boss 解锁规则继续复用
  `ExploreItemProgression`，并新增 C++11 主机回归测试。
- 已迁移原生竖屏探索路线行走：再次点击已选中的解锁区域会生成并进入路线，
  点击地图可开始或暂停自动行走，镜头会在 `16x12`、每格 `26` 像素的共享世界
  中跟随精灵。到达终点后再次点击返回区域列表。
- 路线生成直接复用共享 `ExploreMapGenerator`，地图绘制直接加载共享
  `maps.smonfx` 图块资源；图块缺失时保留几何降级画面。推荐等级、场景底色及
  道路路径坐标分别由 `ExploreAreaCatalog` 和 `ExploreRouteGeometry` 统一提供，
  StickS3 与 AMOLED 不维护两套路线规则。
- 路线移动时仅以 `90 ms` 节奏重绘并传输 `24..224` 行；暂停和静止时不持续
  刷新。打开右上角菜单会暂停路线计时，返回后从原位置继续，不产生时间跳变。
- 新增共享 `core/AppSceneFlow`：语义场景、菜单来源/返回状态、正式菜单顺序、
  目标场景和图标索引不再由 AMOLED 固件硬编码。AMOLED 已删除本地
  `View/menuReturnView`，StickS3 菜单也改为读取同一份共享目录。
- AMOLED 主菜单已与 StickS3 正式版对齐为“探索、队伍、房间、背包、商店、
  电脑、设置、返回”，并直接复用 `MenuAssets` 中同一组 `40x40` 图标。竖屏只
  保留独立的列表布局、触摸命中和拖拽滚动；新增菜单项时不再维护第二份编号。
- 已迁移探索路线中的独立竖屏菜单，顺序为“队伍、背包、结束、返回”。菜单项
  语义、目标场景和图标索引进入共享 `AppSceneFlow`，StickS3 的按键动作也改为
  读取同一目录。打开菜单会暂停路线；左上返回或“返回”从原位置继续，“结束”
  返回区域列表。按下反馈和提示只重绘 `28..224` 内容行，菜单静止时不持续刷新。
- 已迁移竖屏背包与商店。共享 `game/ItemInventory` 统一库存字段映射、堆叠上限、
  回复/状态治疗/复活效果和背包顺序；共享 `game/ShopService` 统一 StickS3 商品
  目录、价格、探索进度解锁、每日糖果上限及买卖的原子状态变更。AMOLED 只维护
  触摸列表、确认弹窗和竖屏绘制，成功使用、购买或出售后立即写入共享存档。
- AMOLED 资源分区包含 StickS3 共用的 `game/ui.smonfx`，背包和商店通过
  `GameAssets::itemKind` 绘制同一套 `36x36` 物品图标。商店分类页对齐 StickS3
  的左右分栏：左侧分类、右侧图标预览；商品子页为左侧可拖拽图标列、右侧选中
  商品详情和操作按钮，停止滚动后吸附到最近物品。内容操作只重绘 `28..224`。
  探索菜单打开背包时通过共享 `AppSceneFlow` 子场景返回状态回到探索菜单，不会
  提前恢复路线。
- 当前背包已支持回复药、状态治疗药、复活药、金珠、大珍珠和星星碎片；进化石
  会进入竖屏成长流程，心之鳞片进入队伍招式回忆页。通信入口作为独立的
  `COMM` 菜单项接入，不把通信动作伪装成背包物品效果。
- 已迁移探索子菜单的队伍页面，并同时接通主菜单 `TEAM`。竖屏成员卡片展示共享
  精灵资源、等级、HP、饥饿值和异常状态；第二名正式成员可确认后切换为队首，
  访客保持不可切换。共享 `game/TeamRoster` 负责队伍重排，StickS3 的
  `GameEngine::moveTeamMemberToFront` 也委托同一规则；AMOLED 切换后会同步主界面
  行为参数、精灵缓存和存档。
- 主界面锁屏和菜单按钮已从右上角迁到底部 HUD 左侧，顶部只保留品牌、游戏
  时间和电量。底部右侧复用 StickS3 的饥饿图标资源与 HP 颜色规则，固定预留
  两行正式队伍成员；共享 `game/HomeHud` 统一过滤访客、无需进食精灵和昏厥
  状态，共享 `presentation/HudRenderer` 统一饥饿图标裁切。数值变化和底部触摸
  反馈仍只刷新 `172..224` 行。
- 已接通主菜单“房间”及其“食物、洗澡、返回”子页。食物页展示七种食物的
  共享库存和选中状态，返回房间后点击碗仍由共享 `HomeCare` 完成放置；浴室
  背景、三种肥皂、刷子、花洒、泡沫和精灵正面图直接复用 `ui.smonfx` 与
  `.smonsp` 资源，不维护第二套素材。
- 洗澡库存和数值规则已提取到共享 `game/BathService`：StickS3 与 AMOLED 共用
  三种肥皂消耗、每日照护经验上限、起泡/刷洗/冲洗三阶段经验和心情奖励。
  StickS3 原有升级、进化和招式学习仍由 `GameEngine` 结算；AMOLED 通过共享成长
  服务和自己的竖屏成长页面同步更新经验、等级、HP、进化和招式槽位。
- AMOLED 洗澡不依赖体感。玩家选择肥皂后，从底部拖住肥皂在精灵身体区域内
  来回擦拭；完成后以同样方式拖住刷子。仅连续移动路径计入进度，工具静止或
  移出身体区域不计数。冲洗为约 `1.8 s` 的自动动画，未冲洗完成退出会二次确认。
  肥皂选择后立即扣除并保存，三个奖励阶段也分别立即保存。
- 洗澡页继续遵守按需绘制：肥皂/刷子拖动时重绘内容区，冲洗期间每 `80 ms`
  更新一次，静止、选择工具和完成后的常驻页面不持续刷新。冲洗进度会同步减少
  泡沫，完成后停止动画刷新。
- 已迁移房间 `COMPUTER` 和 `SETTINGS`：电脑包含只读的当前队伍状态页和仓库
  浏览页，仓库支持拖拽/惯性滚动查看最多 20 个槽位；设置页可修改亮度、音量、
  游戏速度、省电超时和语音开关，并立即写入共享 `GameState`。
- 已迁移 AMOLED 竖屏成长流程：照护或探索获得经验后依次处理升级、进化、招式
  学习和满招式替换，进化会同步 HP 上限、动态精灵缓存和可保留招式；完成后返回
  原来的洗澡或探索场景。
- 已迁移探索路线的事件闭环：每步更新步数并按共享区域遭遇表生成野生精灵，
  拾取金币/药品，路线终点按共享 Boss 保底和特殊遭遇规则生成普通 Boss、首次
  Snorlax/Latias/Latios、游荡 Latias/Latios 及 Mew 事件。AMOLED 战斗支持普通、
  特殊和蓄力招式、双精灵切换、背包治疗、逃跑、后备精灵经验；Boss 胜利会更新
  区域进度、特殊 Boss 已击败标记和普通 Boss 保底状态。胜利升级继续进入成长流程，
  失败返回房间并保存昏厥状态。
- 已完成共享显式 `SaveCodec` 和 `SaveManager` A/B 存档：采用小端字段编码、CRC、
  sequence 和 `state_a`/`state_b` 双槽回退，同时保留 `state` 兼容镜像；旧 v1/v2/v3
  存档可迁移。AMOLED NVS 和 StickS3 后端共用同一份编码，不复制内存结构。
- 已完成 AMOLED V1 的 ESP-IDF `IPeerTransport`：Wi-Fi STA、ESP-NOW 广播发现、
  加入确认、会话 ACK、自动重传和接收队列均接入共享 `EspNowLink`。主机端覆盖发现、
  加入和 ACK 回环；AMOLED 已接入 `VisitSessionService` 通信页面，支持主机/搜索房间、
  加入确认、精灵状态同步、访问倒计时、心跳、召回和断线失败状态。
- 已完成 V1 平台层的按能力降级：NVS、SPIFFS、PSRAM、音频编解码和 ESP-NOW 可用；
  当前 V1 BSP 没有可用的 AXP2101 电池、QMI8658 IMU、PCF85063A RTC 驱动，因此电量、
  体感和 RTC 维持 unavailable/系统时间，不返回假数据。锁屏目前关闭面板但 MCU 保持
  运行，真正 PMU 深度睡眠仍需硬件验证。
- 2026-08-09 已对 V1 完整烧录应用与 SPIFFS 资源分区。实机启动确认 SH8601、
  FT3168、NVS、房间、精灵和资源包均正常初始化，未出现崩溃或栈溢出。
- M5StickS3 PlatformIO release 构建、AMOLED V1 ESP-IDF 构建和 AMOLED V2 ESP-IDF
  构建均已通过；V2 使用 Waveshare BSP `2.0.3`，应用分区余量为 63%。

待完成：

- 实机确认 FT3168 坐标方向、拖拽连续性和锁屏唤醒点击。
- 实机确认探索路线的真实图块、镜头跟随、暂停/继续、退出确认及终点返回手感。
- 实机确认洗澡工具拖拽命中、路径进度、冲洗动画和未完成退出确认的手感。
- 实机确认捕获/结交确认、队伍招式管理、路线阻挡/谜题、战斗动画、命中特效和音效
  的触摸手感与节奏；主机测试和 AMOLED 编译已通过。
- 补齐睡眠姿态及更完整的移动/进食动画，并继续调校成长页面的触摸手感。
- 在获得对应 BSP 驱动后接入 AXP2101、QMI8658、PCF85063A 的真实能力，并完成真正的
  低功耗休眠；ES8311 音频接口已具备，仍需实机验证录放音效果。
- 完成 StickS3/V1/V2 两两 ESP-NOW 实机联调，包括断线、重试、版本不匹配和交易回滚；
  当前已有主机回环和共享协议测试，仍缺 V1/V2 实机组合验证。
- 取得 V2 实机后完成 CO5300、CST820、音频、电源、休眠和触摸坐标的 board-check。

### 本轮执行结果

本轮已完成捕获/结交、背包剩余道具、队伍招式管理、路线阻挡/谜题、战斗演出与音效，
并完成通信业务入口、访问会话服务、自动锁屏、V1/V2 板级边界和 V2 构建目标：三者
继续使用同一个 Git 仓库、同一份 `src/game` 业务规则、同一份 `GameState`、同一份
`SaveCodec` 和同一份 ESP-NOW 会话协议；AMOLED 只维护竖屏页面、触摸状态机和平台
适配，不复制 StickS3 的数值逻辑。V1 固件和 V2 固件均已通过编译，9 项共享主机回归
测试均已通过；当前剩余项以 V1/V2 实机手感、两台设备联调以及睡眠/低功耗真实驱动
验证为主。
