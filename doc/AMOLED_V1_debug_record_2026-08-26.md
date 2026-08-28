# AMOLED V1 启动故障调试记录

日期：2026-08-26  
项目：StickMon 迁移到 ESP32-S3-Touch-AMOLED-1.8 V1  
工程：`firmware/amoled_1_8_v1`

## 1. 故障现象

刷入 AMOLED V1 固件后，设备曾出现以下现象：

- 上电后屏幕短暂点亮，随后黑屏。
- 串口日志最初停在 `LCD panel create success`。
- 后续增加日志后，设备在启动过程中不断重启。
- 最终修复前，日志能够执行到亮度初始化成功，但没有进入主界面绘制。

## 2. 刷写结果确认

刷写日志显示以下镜像均已写入并通过校验：

| 镜像 | 地址 | 结果 |
| --- | ---: | --- |
| Bootloader | `0x00000000` | 写入成功，Hash 校验通过 |
| 应用固件 | `0x00020000` | 写入成功，Hash 校验通过 |
| 分区表 | `0x00008000` | 写入成功，Hash 校验通过 |
| OTA 初始数据 | `0x0000f000` | 写入成功，Hash 校验通过 |
| 资源分区 | `0x00620000` | 写入成功，Hash 校验通过 |

因此，本次故障不是烧录失败，也不是资源镜像未写入。

## 3. 启动日志分析

设备启动基础环境正常：

```text
Found 8MB PSRAM device
Speed: 80MHz
SPI SRAM memory test OK
cpu freq: 240000000 Hz
Calling app_main()
```

显示初始化日志如下：

```text
Starting AMOLED V1 display milestone
PSRAM size: 8388608 bytes
DMA buffers: 94208 bytes
Initialize SPI bus
LCD panel create success, version: 2.0.0
Display: bsp_display_new returned ESP_OK
Display: brightness init returned ESP_OK
Display: brightness set returned ESP_OK
```

这证明以下部分均已正常完成：

- ESP32-S3 启动
- 8 MB PSRAM 初始化
- DMA 缓冲区分配
- TCA9554/I2C 初始化
- QSPI 总线初始化
- SH8601 面板创建、复位和初始化
- AMOLED 显示开启
- 屏幕亮度设置

之前误以为卡在 `LCD panel create success`，原因是该阶段之后缺少应用侧阶段日志。实际上 `bsp_display_new()` 后续已经返回 `ESP_OK`。

## 4. 排查过程

### 4.1 排除依赖缺失

当前工程使用 ESP-IDF 5.5.4，依赖均已完成解析并成功编译。缺少依赖通常会在构建阶段报错，不会表现为应用已经进入 `app_main()` 后重启。

### 4.2 排除刷写不完整

应用、分区表、Bootloader 和资源分区都显示 `Hash of data verified`，排除了镜像损坏或资源未刷入。

### 4.3 排除 SH8601 初始化卡死

在 `bsp_display_new()` 前后加入日志后，确认该函数返回成功。亮度初始化和亮度设置也都返回 `ESP_OK`，因此显示驱动不是最终重启原因。

### 4.4 定位到应用启动阶段

重启发生在显示初始化之后，主界面首帧绘制之前。该阶段集中执行：

- `AmoledApp` 对象创建
- NVS 存档加载或创建
- SPIFFS 资源分区加载
- 房间资源和中文字库初始化
- 主界面模型和首帧绘制

这些逻辑全部运行在 ESP-IDF 的 `main_task` 中。原配置为：

```text
CONFIG_ESP_MAIN_TASK_STACK_SIZE=12288
```

对于当前包含完整业务状态、资源解析和绘制逻辑的 `AmoledApp`，12 KB 主任务栈空间不足，导致启动阶段异常重启。

## 5. 修复内容

将 AMOLED V1 工程主任务栈提升到 24 KB：

```text
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
```

同步更新：

- `firmware/amoled_1_8_v1/sdkconfig.defaults`
- 当前工程生成的 `sdkconfig`

同时在 `main/main.cpp` 增加了启动阶段诊断日志：

- Reset reason
- 显示传输信号量创建
- DMA 传输回调注册
- 平台服务绑定
- `AmoledApp` 创建
- 存档和资源加载
- 初始帧绘制
- 初始帧提交
- 主任务剩余栈空间

## 6. 验证结果

调整主任务栈后，固件重新构建成功，设备可以：

1. 正常启动 Bootloader 和应用。
2. 正常初始化 8 MB PSRAM。
3. 正常初始化 SH8601 AMOLED。
4. 正常设置亮度。
5. 正常完成主界面初始化和首帧绘制。
6. 屏幕保持点亮，不再循环重启。

## 7. 当前结论

本次故障的根因是：

> `main_task` 栈空间不足，导致 AMOLED V1 在应用首次加载资源和绘制主界面时异常重启。

不是以下原因：

- ESP-IDF 依赖没有安装完整
- 烧录失败
- Bootloader 或分区表错误
- 资源分区没有烧录
- PSRAM 故障
- SH8601 面板初始化失败

## 8. 后续注意事项

- V1 工程保持 `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576`，不要降回 12 KB。
- 新增启动阶段资源解析、字体加载或复杂绘制逻辑时，继续关注主任务剩余栈空间。
- 保持使用同一个 `build/` 目录生成和刷写 Bootloader、应用、分区表、OTA 数据及资源镜像。
- 不要混用 `build-v1/` 和 `build/` 中的产物。
- 出现启动异常时，优先保留从 `Calling app_main()` 到下一次 `rst:` 的完整串口日志。
- 如果未来主任务栈继续逼近上限，应将资源预加载、首帧绘制或显示提交迁移到独立任务，而不是无限增大主任务栈。

## 9. 推荐刷写方式

```bash
cd /Users/gtleaf/project/esp/StickMon/firmware/amoled_1_8_v1
./flash.sh --port /dev/cu.usbmodemXXXX
```

只有在存档或旧镜像状态不确定时，才使用一次：

```bash
./flash.sh --port /dev/cu.usbmodemXXXX --erase
```

`--erase` 会清除 NVS 存档，不应作为普通升级步骤使用。
