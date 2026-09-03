# ESP-Claw 接入问题与处理记录

更新时间：2026-09-01

本文记录 StickMon 接入 ESP-Claw、Coze 和微信 iLink 过程中已经遇到并排查过的问题。文档中的“已确认”来自源码、编译结果或设备串口日志；“待验证”表示代码已经处理，但还需要在设备上完成最终业务验证。

## 当前集成范围

- 目标固件：AMOLED 1.8 V1/V2，当前实机主要验证 V1。
- 分支：`codex/remote-chat`。
- ESP-Claw 源码目录：`/Users/gtleaf/project/esp/espclaw/esp-claw-master`。
- StickMon 集成桥接：`src/brain/`。
- ESP-IDF 组件包装：`firmware/common/components/app_claw`、`firmware/common/components/stickmon_claw`。
- 微信通道：微信 iLink Bot，使用二维码登录和长轮询 `getupdates`。
- LLM 通道：Coze，Bot ID 当前为 `7679649015453597711`。

## 问题总览

| 编号 | 问题 | 根因 | 当前状态 |
| --- | --- | --- | --- |
| 1 | ESP-IDF 激活失败，提示 `esp-rom-elfs has no installed versions` | 本机缺少 ESP-IDF 工具版本 | 已通过安装对应工具解决 |
| 2 | 启动时 ESP-Claw 初始化慢或阻塞游戏启动 | Wi-Fi、brainfs、内存和 Agent 初始化集中执行 | 已改为异步初始化 |
| 3 | ESP-Claw 内存初始化失败 | brainfs/内存组件初始化时机和资源条件不满足 | 已增加异步启动与日志，当前启动正常 |
| 4 | 管理页按钮返回 `Nothing matches the given URI` | 微信 URI 注册失败或命中了旧固件/旧页面 | URI 上限已提高并重新刷入，需按最新固件验证 |
| 5 | 二维码空白 | 手机连设备热点时无法加载微信外部图片 URL | 已改为设备端 Canvas 绘制，并保留图片链接回退 |
| 6 | 扫码后只显示一串字符串 | 错把微信轮询字段 `qrcode` 编码进 QR | 已改为编码 `qrcode_img_content` |
| 7 | 点击生成二维码导致设备重启 | HTTP 任务栈上放置过大的二维码临时缓冲区 | 已改为堆分配，实机复测不再重启 |
| 8 | 微信登录成功但发“你好”无响应 | Agent 处理消息时使用 PSRAM 栈访问 Flash 文件系统，触发缓存断言并重启 | 已将 Agent 栈固定到内部 RAM，待最终收发验证 |
| 9 | 登录成功但消息通道未生效 | `completed=1` 但 `persisted=0`，凭据仍是临时状态 | 管理页需点击“保存微信登录”并重启 |
| 10 | 刷写时串口被占用 | monitor/miniterm 持有串口独占锁 | 先退出串口监视再刷写 |
| 11 | 刷写是否清除存档 | 普通 `flash` 只覆盖固件和资源分区；NVS 工具会整分区覆盖 | 已确认普通刷写保留存档，NVS 工具需谨慎使用 |

## 问题时间线

1. 首先遇到 ESP-IDF 工具链不完整，导致环境激活失败；补齐 `esp-rom-elfs` 等工具后恢复编译能力。
2. ESP-Claw 与游戏在启动阶段同步初始化，造成启动慢；随后改为后台异步初始化，让游戏先进入主界面。
3. `brainfs`、memory 和 HTTP 管理页逐步接入，期间出现 memory 初始化失败、微信接口 404 和配置页面回填不清晰等问题。
4. 微信二维码经历了“空白”“扫出字符串”和“生成后重启”三个问题，分别定位到外链不可达、字段用错和 HTTP 任务栈空间不足。
5. 微信登录可以完成后，发送消息触发设备重启；回溯确认是 Agent 的 PSRAM 栈在访问 Flash 会话历史时触发 ESP32-S3 缓存断言。
6. 当前修复版已把 Agent 核心任务栈固定到内部 RAM，设备能够持续执行微信 `getupdates` 长轮询；最后还需要完成一条真实消息从微信到 Coze 再回微信的闭环验收。

## 1. ESP-IDF 工具缺失

### 现象

执行 ESP-IDF 环境激活时出现：

```text
ERROR: tool esp-rom-elfs has no installed versions.
ERROR: Activation script failed
```

### 原因

当前 ESP-IDF 5.5.4 的工具链没有完整安装，`export.sh` 只能配置环境，不能替代工具安装。

### 处理

使用 ESP-IDF 自带的工具安装命令安装缺失版本，然后重新执行：

```bash
python /Users/gtleaf/.espressif/v5.5.4/esp-idf/tools/idf_tools.py install
source /Users/gtleaf/.espressif/v5.5.4/esp-idf/export.sh
```

当前已经可以正常编译和刷写 AMOLED V1。

## 2. 初始化速度与异步启动

### 现象

配置 Wi-Fi 和 Coze 后，设备启动需要等待 Wi-Fi、brainfs、ESP-Claw memory、技能注册和 Agent 初始化，游戏画面出现较晚。

### 处理

`ClawRuntime::beginAsync()` 创建独立的 `stickmon_claw_init` 任务，游戏先完成显示和交互初始化，ESP-Claw 后台再启动。初始化任务完成后自动删除。

初始化过程会记录：

```text
ESP-Claw background initialization started
ESP-Claw background initialization completed in ... ms
```

### 当前内存策略

- 初始化任务：内部 RAM，32 KB，完成后释放。
- Agent 核心任务：内部 RAM，16 KB，原因见第 8 节。
- 微信轮询、二维码任务及其他 ESP-Claw 任务：仍优先使用 PSRAM。

这不是把整个 ESP-Claw 都迁回内部 RAM，因此不会重复占用所有大块内部内存。

## 3. brainfs 和 memory 初始化失败

### 现象

早期日志出现：

```text
Failed to init claw_memory: ESP_FAIL
ESP-Claw start failed: ESP_FAIL
```

### 处理

- 增加 `brainfs` FATFS 分区。
- 初始化时显式设置 ESP-Claw data/system 路径。
- 将 ESP-Claw 初始化放到异步任务，避免阻塞游戏主流程。
- 增加 memory、路径和网络前置条件日志。

当前 V1 启动日志已经可以看到：

```text
claw_memory: Initialized memory root=/brain/memory
Root agent ready id=0
App Claw runtime started
```

## 4. 管理页 `Nothing matches the given URI`

### 现象

点击“生成微信二维码”没有反应，串口出现：

```text
httpd_uri: URI '/api/wechat/login/start' not found
404 Not Found - Nothing matches the given URI
```

### 原因

管理后台页面请求了微信登录接口，但设备 HTTP server 中没有注册该 URI。排查时还要注意手机可能连接到了旧固件或同名旧热点。

### 处理

将 HTTP server 的 `max_uri_handlers` 从 9 提高到 16，为页面、配置、状态、日志和 4 个微信接口留出余量：

```text
/api/wechat/login/start
/api/wechat/login/status
/api/wechat/login/cancel
/api/wechat/login/persist
```

刷入新固件后，启动日志应包含：

```text
Setup HTTP routes registered: config/status/log/save/wechat
```

若页面仍返回 404，应先断开旧热点、重新连接当前设备热点，并对 `http://192.168.4.1/` 做强制刷新。

## 5. 二维码空白

### 现象

管理页提示“微信二维码已经生成，等待扫码”，但 `<img>` 或二维码区域为空。

### 原因

微信接口返回的 `qrcode_img_content` 是外部地址，例如：

```text
https://liteapp.weixin.qq.com/q/...
```

手机连接设备热点时通常不能访问该外部地址，所以页面图片加载失败。

### 处理

- ESP-Claw 状态接口增加 `qrcode` 字段透传。
- StickMon 管理页使用 Canvas 在本地绘制二维码。
- 当设备端二维码编码失败时，页面显示“打开微信二维码图片”链接作为回退。

注意：设备端本地绘制必须使用真正的微信登录 URL，而不是轮询编号，见第 6 节。

## 6. 扫码只显示字符串

### 现象

微信“扫一扫”识别后显示一串字符串，未进入登录确认页面。

### 原因

微信接口返回两个不同用途的字段：

- `qrcode`：短的临时轮询 key，仅用于设备请求 `get_qrcode_status`。
- `qrcode_img_content`：实际应放入微信二维码的登录 URL。

把 `qrcode` 编码成 QR 后，微信会把它当普通文本处理。

### 正确流程

```text
get_bot_qrcode
    ├── qrcode              -> 设备后台轮询
    └── qrcode_img_content  -> QR 内容，微信扫一扫
```

StickMon 当前应编码 `qr_data_url`，该字段对应 `qrcode_img_content`。正常扫码方式是微信内置“扫一扫”，不需要额外插件。

## 7. 点击生成二维码导致重启

### 现象

点击“生成微信二维码”后设备重启，早期崩溃地址位于 `writeWechatQrStatus()` 附近。

### 根因

HTTP 任务栈上同时放置了：

- 二维码点阵缓冲区，约 1.4 KB。
- 点阵 JSON 字符串缓冲区，约 1.4 KB。
- 微信登录状态结构和函数调用栈。

最终破坏了 HTTP server 任务栈，表现为 FreeRTOS 队列操作中的 `StoreProhibited`。

### 处理

二维码两个临时缓冲区改为 `calloc` 动态分配，并在正常和异常返回路径释放。实机复测日志为：

```text
WeChat QR matrix encoded size=25 modules=625 payload_len=32
WeChat status response active=1 ... phase=waiting_scan qr=1
```

连续轮询期间未再重启。

## 8. 微信消息收到后设备重启

### 现象

微信登录成功并保存后，发送“你好”没有回复；串口先显示消息已进入事件路由，随后重启：

```text
claw_event_router: processing event=msg-... type=message source=wechat_gateway channel=wechat
assert failed: spi_flash_disable_interrupts_caches_and_other_cpu cache_utils.c:127
```

回溯定位到：

```text
claw_memory_session.c -> stat()
FATFS / wear_levelling / esp_flash_read
claw_core_agent_loop_task
```

### 根因

Agent 核心任务原先使用 PSRAM 栈。消息处理会读取 brainfs 中的会话历史，而 Flash 操作会进入 cache-disabled 区域；ESP32-S3 此时要求任务栈位于内部 RAM，使用 PSRAM 栈会触发 `esp_task_stack_is_sane_cache_disabled()` 断言。

### 处理

在 ESP-Claw 的 `claw_core.c` 中将 Agent 任务栈策略改为：

```c
.stack_policy = CLAW_TASK_STACK_INTERNAL_ONLY
```

这只影响 Agent 核心任务；微信轮询和其他可迁移任务仍可使用 PSRAM。

### 当前状态

修复版已经编译并刷入 V1。刷入后设备可以持续正常请求：

```text
https://ilinkai.weixin.qq.com/ilink/bot/getupdates
```

仍需最终确认：收到一条新的微信消息后，Agent 不重启、Coze 请求成功，并通过 `wechat_send_message` 返回回复。

## 9. 登录成功但消息通道未生效

### 状态含义

管理页或接口可能显示：

```text
completed=1
persisted=0
phase=confirmed
```

这表示扫码确认成功，但凭据还只保存在内存中。只有 `persisted=1` 才表示已经写入 NVS 并完成应用。

### 正确操作

1. 扫码并在微信中确认。
2. 管理页出现“保存微信登录”按钮后点击。
3. 等待页面提示保存成功。
4. 重启设备。
5. 等待 Wi-Fi、ESP-Claw 和微信轮询启动。
6. 再发送微信消息。

启动后应看到微信 `getupdates` 长轮询；如果没有该日志，优先检查凭据是否保存或 Wi-Fi 是否联网。

## 10. 串口占用导致无法刷写

### 现象

刷写时报：

```text
Could not open /dev/cu.usbmodem2101, the port is busy
```

### 原因与处理

`idf.py monitor` 或 `serial.tools.miniterm` 会独占 USB 串口。刷写前先退出监视器：

```text
Ctrl-]
```

必要时检查并结束仍持有 `/dev/cu.usbmodem2101` 的监视进程，再执行刷写。

## 11. 刷写与本地存档

### 普通固件刷写

当前 `idf.py flash` 会覆盖 bootloader、应用、分区表、OTA 数据和 resources 分区。不会执行整片擦除，也不会主动覆盖：

- `nvs`：Wi-Fi、Coze、微信凭据。
- `brainfs`：ESP-Claw 会话和 memory。
- 游戏存档所在的数据区域。

### NVS 镜像工具风险

`tools/generate_claw_nvs.py` 生成的是完整 NVS 镜像，写入时会覆盖整个 NVS 分区。它适合新设备首刷或明确需要重置 NVS 的场景，旧设备使用前必须备份 NVS，否则可能清除 Wi-Fi、微信凭据和游戏存档。

## 12. sendmessage 返回 ret=0 但微信收不到回复（未解决，已暂停）

### 现象

微信入站、事件路由、Coze 补全、出站路由全部正常，`sendmessage` 返回 HTTP 200
`{"ret":0}`，但微信客户端始终看不到 bot 的回复。

### 已排除（2026-09-02，有实机与 Mac 直连证据）

- 请求体格式：逐字段对照官方插件 `@tencent-weixin/openclaw-weixin@2.4.8`
  （npm 源码）与 [epiral/weixin-bot PROTOCOL.md](https://raw.githubusercontent.com/epiral/weixin-bot/main/PROTOCOL.md)，
  `from_user_id=""`、`client_id`、`message_type=2`、`message_state=2`、
  `context_token`、`item_list`、`base_info` 全部一致。
- `context_token` 缓存无截断：缓冲区 160 字节，实测 token 136 字节；发送时的
  hash 与最新入站消息逐字节一致。
- 凭据与账号服务器侧有效：从设备 NVS 提取 token 后在 Mac 直连，
  `getupdates`/`getconfig`（成功拿到 typing_ticket）/`sendtyping`/`sendmessage`
  全部返回成功。
- `base_info` 差异：esp-claw 原来发送 `channel_version: "esp-claw-wechat"`，
  已改为对齐官方插件（`"2.4.8"` + `bot_agent: "OpenClaw"`，见
  `cap_im_wechat.c::cap_im_wechat_add_base_info`），改后实机仍 ret=0 且不到达。
- Mac 隔离测试脚本：`tools/test_wechat_ilink_send.py`（4 种变体：原格式、
  官方 base_info、先 sendtyping、带 run_id，服务器全部受理）。

### 关键现象

- `sendtyping` 成功返回但微信端连「对方正在输入」都不出现 —— 说明这个
  bot 账号到用户微信客户端的整条下行链路不生效，与消息体格式无关。
- 已发送消息在 `getupdates` 中从未出现回声（esp-claw 已将 type=2 回声
  提为 INFO 日志 `ignore outbound WeChat echo`，便于日后复查）。

### 待查方向（若重启调查）

- 登录响应的 `baseurl` / `scaned_but_redirect` 的 `redirect_host`：官方插件
  会在重定向时切换轮询主机，esp-claw 只处理 wait/scaned/confirmed/expired。
- 用户微信客户端是否支持 ClawBot 会话界面；回复是否进入了另一个会话入口。
- 用 Mac 做一次完整的新扫码登录 + 收发，验证是否账号绑定本身的问题。

### 当前状态

用户已暂停微信渠道，改用飞书。微信代码保持可用，上述 base_info 对齐修改保留。

## 推荐验收顺序

1. 启动设备，确认游戏先显示，随后出现 `ESP-Claw background initialization completed`。
2. 确认 Wi-Fi 已联网，日志中有 `sta ip:`。
3. 进入 `电脑 -> ESP-Claw`，确认管理页接口注册完成。
4. 点击“生成微信二维码”，确认设备不重启，且页面显示 Canvas 或图片链接。
5. 用微信内置“扫一扫”扫描新二维码。
6. 点击“保存微信登录”，确认 `persisted=1`。
7. 重启设备，确认持续出现 `getupdates` 长轮询。
8. 发送“你好”，确认依次出现：

```text
processing event=msg-...
Coze/LLM request ...
wechat_send_message ...
```

9. 再测试玩家操作优先：设备触摸操作期间不应发起自主动作，微信消息也不应覆盖当前玩家操作。

## 相关文件

- `src/brain/StickmonClawRuntime.cpp`
- `src/brain/BrainBridge.cpp`
- `src/brain/StickmonCapability.cpp`
- `src/presentation/QrCodeGen.cpp`
- `firmware/common/components/stickmon_claw/CMakeLists.txt`
- `firmware/amoled_1_8_v1/main/CMakeLists.txt`
- `firmware/amoled_1_8_v1/partitions.csv`
- `/Users/gtleaf/project/esp/espclaw/esp-claw-master/components/claw_capabilities/cap_im_platform/include/cap_im_wechat.h`
- `/Users/gtleaf/project/esp/espclaw/esp-claw-master/components/claw_capabilities/cap_im_platform/src/cap_im_wechat.c`
- `/Users/gtleaf/project/esp/espclaw/esp-claw-master/components/claw_modules/claw_core/src/claw_core.c`

## 重要注意事项

- ESP-Claw 外部源码位于 StickMon 仓库之外；部署到其他机器时需要设置 `STICKMON_ESPCLAW_ROOT`。
- 当前 V1 已做过实机联调；V2 仍需独立完成显示、触摸、联网和微信收发验收。
- 编译通过不等于微信链路完成，最终以设备串口和微信实际收发结果为准。
- Access Token、微信 token、Wi-Fi 密码不要提交到 Git，也不要放入文档或截图。

## 当前结论

- “初始化任务曾因 RAM 紧张而迁移到 PSRAM”和“Agent 核心任务必须使用内部 RAM”并不矛盾：前者针对可延后的启动工作，后者针对会读写 Flash/FATFS 的执行上下文。
- 当前内存分工是：初始化任务内部 RAM（完成后释放），Agent 核心任务内部 RAM，微信轮询/二维码及其他可迁移任务优先使用 PSRAM。
- 普通固件刷写不会主动清除游戏存档；写入完整 NVS 镜像会覆盖 NVS 分区，可能影响 Wi-Fi、Coze、微信凭据及相关存档数据，操作前必须确认目标分区并备份。
- 微信链路的“扫码登录”和“持续轮询”已经有设备日志证据；“收到消息后不重启、成功调用 Coze 并回复”中 Coze 调用与出站路由已验证，但最后一步“微信客户端实际收到回复”未通过（见第 12 节，调查暂停，渠道切换为飞书）。
- V1 的结果不能直接代表 V2 已稳定，V2 仍需要独立验证显示、触摸、联网、二维码和微信收发。
