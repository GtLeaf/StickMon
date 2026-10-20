# AMOLED V2 显示异常排查

日期：2026-10-19。

用户此前使用物理 V1，但运行 `firmware/amoled_1_8_v2/flash.sh` 刷入的固件。
切换为 CO5300 / CST820 的物理 V2 后，洗澡拖动刷子、探索出现重影和撕裂。

## 结论与修复

脚本未发现选错工程、混用 V1 产物的问题：显式传入 `v2`，按 profile 使用
V2 独立 build 目录；应用、bootloader、分区表、OTA 初始化数据和资源均来自
同一目录。V2 CMake 复用 V1 的应用与 UI 是既定架构，实际编译的板级入口和
触控实现仍来自 V2。

发现直接传屏路径漏掉官方 BSP 的刷新矩形对齐规则。BSP 的 LVGL rounder
将起点向下取偶数、包含式终点向上扩到奇数；本项目绕过 LVGL，自行调用
`esp_lcd_panel_draw_bitmap`，因此也必须实施对应规则：排他式终点取偶数。
旧代码仅锁屏矩形对齐，洗澡、探索等普通局部更新可以传入奇数坐标和跨度。
这是明确的适配遗漏；修复刷入后，用户确认洗澡拖动及探索实屏“已经正常”。

`main/main.cpp` 现在统一对全宽脏行及局部矩形向外做 2 像素对齐，按扩展后的
起点、宽度从持久 framebuffer 拷贝像素，再分为 32 行 DMA 传输。不能仅改
LCD 窗口地址而不调整 payload，否则像素行跨度会错位。未增加图片资源或
framebuffer，原有双 DMA 缓冲与完成等待继续使用。

另修正了 V2 启动日志误写 V1 的问题；刷机脚本现在打印目标与构建目录。
脚本和 README 移除了“混刷后例行 erase”的建议，明确 `--erase` 会删除存档
和凭据。此次刷机未擦除整片 Flash。

官方实现参考：[Waveshare BSP 2.0.3](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_8/versions/2.0.3/readme)。
本地实现为 managed component 内的 `rounder_event_cb` / `bsp_lvgl_rounder_cb`；
修复未修改 managed component。

## 验证

- 新增 `tools/test_amoled_v2_display.py`，直接编译生产传屏函数，以延迟 DMA
  完成的模拟面板验证奇数矩形、单像素、32 行分块、屏幕边缘、越界裁剪、
  空矩形和缓冲区复用。所有传输后面板像素必须逐项等于 framebuffer。
  去掉本次对齐调用后，该回放在奇数面板坐标断言处失败；修正版通过。
- `tools/test_amoled_v2_touch.py` 通过，仅证明历史校准回放未变化。
- `tools/test_amoled_native_layout.py` 5 项通过。此前其中一个源码断言仍在
  AmoledApp 查找已迁入 ExplorePresenter 的探索刷新常量，本次同步检查位置。
- V2 Lite Debug、Lite Release、默认 Claw Release 均构建成功，Release 打包成功。
- 已向当前新 V2 刷入 Lite Debug。设备标识与旧 V1 不同，启动日志实测
  `Touch CST816S 0x15 found`，为官方 CST820 兼容驱动路径；此前旧设备日志
  是 `Touch FT5x06 0x38 found`。不能把历史测试当成 CST820 硬件验收。
- 实机运行 12 秒自动刷子拖动，后段完整窗口约 48–49 FPS；探索自动移动并
  进入遇敌首帧，未出现传屏报错、崩溃或 watchdog。性能数字来自本次短测，
  不代表所有精灵或地图的帧率保证。
- 备份的是本次新设备自身的 0x9000 / 0x6000 NVS，测试后按设备标识核对并
  恢复，读回 24576 字节逐字节一致。重启后 RAM 与磁盘的金币、队伍、仓库、
  HP、等级和道具等诊断字段与测试前一致，设备返回房间，串口已释放。
- 用户随后观察新固件的洗澡拖动和探索画面，反馈“已经正常”，本次报告的
  重影、撕裂问题完成实屏验收。

临时日志：`/tmp/stickmon-v2-display-device-test.log`、
`/tmp/stickmon-v2-display-explore.log`、`/tmp/stickmon-v2-display-reboot.log`。
这些日志属于本次本地诊断产物，不随仓库分发。

## 尚未确认的边界

UART 日志本身不能证明物理画面无重影或无扫描撕裂；本次视觉验收依据是
用户观察新固件后的确认。本次修复覆盖刷新窗口与像素传输对齐，未加入
TE 同步，不能推广为所有负载下都不会产生扫描撕裂的保证。

另有独立的触控风险：现有九点校准实际上来自旧物理 V1 的 FT 控制器，
当前仍无条件应用于 CST820。本次未改触摸映射；README 已修正测量来源。
这可能导致触点偏移，但不能据此解释显示重影。

后续已单独处理该触控问题，新增 CST820 / FT 控制器配置选择，并以真实 V2
两轮九点测量重新拟合；参见 [V2 触控校准](AMOLED-V2-触控校准.md)。

启动和音频切换日志仍有 `i2s_channel_disable: the channel has not been enabled yet`，
属于另一个音频生命周期问题，此次没有更改音频代码。
