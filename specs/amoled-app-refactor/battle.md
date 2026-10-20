# 阶段 3：战斗会话与表现拆分

已由用户确认执行；延续原优化方案的阶段 3，Claw 功能迭代继续延期。

## 验收要求

1. 当动作开始时，继续立即执行共享 prepare/commit；HP 数值先提交，280 ms 后开始表现掉血，不能趁拆分更改规则时序或随机调用顺序。
2. 当执行切宠、回合末效果、濒死替补、胜败或结交时，保持正式队序和现有日志顺序；同场奖励最多结算一次。
3. 当战斗交接成长页面时，逐个消费两只成员的成长队列；保存完整结果后才返回路线。
4. 当呈现首帧后才允许播放排队叫声；命中音、HP/XP、食物和切宠动画继续使用单调时钟，阻塞的音频/预载后重新取时。
5. Session/Timeline 不引用 App、平台、保存或渲染缓存，不暴露可写 state；App 消费有界、一次性效果执行外部动作。
6. 通过原生行为/渲染、架构与存档回归，以及 V1/V2 lite/debug/Claw、StickS3 和 Web 构建；与本批新鲜 lite 基线比较体积。不增加素材或改存档与分区。

## 设计

- `BattleSession` 拥有玩法阶段、当前出战位、回合计划、结算与结交、成长队列，并组合 `BattleTimeline`。继续调用既有共享规则服务。
- `BattleTimeline` 拥有攻击/HP/XP/投食/切宠动画、日志和呈现确认。状态通过 const 快照读取。时钟为显式注入的窄接口，便于确定性测试并保留预载后的计时锚点。
- App 保留场景、保存、音频播放与资源预载。效果消费之后显式确认切宠预载完成，再开始入场计时。成长交接返回槽位和旧等级。
- 日志容量仍为 24×64 字节。独立会话使用有界延迟分配的存储，避免扩大临时 App 的栈占用和多个 App 共享全局 FIFO；分配失败时保留可见日志退化路径。更新热路径不分配。
- UI 输入和模型只读访问状态，通过操作方法改变会话。旧测试的初始状态夹具迁移到专用 friend 探针，生产代码没有可写状态入口。

## 任务

- [x] 保存本批源码、工作区状态与 V1/V2 lite 新鲜构建基线；原生 62 项基线通过。
- [x] 固定伤害提交、日志、音频确认和重复结算行为回归。
- [x] 抽出 Session、Timeline 并接通 App 效果与确认。
- [x] 更新测试、源码清单和边界约束。
- [x] 完成 host、固件、Web 与体积验证，记录结果及实机限制。

## 实施结果

- `AmoledApp.cpp` 由本批基线的 14,354 行减至 13,036 行（减少 1,318 行）。`BattleSession.cpp` 1,069 行，`BattleTimeline.cpp` 391 行；包括迁出的模型构建和新增交接契约。
- App 不再拥有分散的战斗会话字段，不再直接 prepare/commit、推进回合末效果或发放战斗奖励。会话和 Timeline 的 `state()` 均只读，既有深层测试仅通过 friend 夹具设置初始状态。
- 保留动作开始立即提交、280 ms 后掉血、420 ms HP 动画、900 ms XP 动画和两段 360 ms 切宠。预载完成后以新鲜时钟锚定入场；叫声经原呈现路径确认后消费一次。
- 胜利结算新增显式 guard，等待奖励日志期间重复调用不会重复经验、努力值、金币、头目进度或结交随机判定。成长队列仍按两位成员顺序交接，保持原有保存边界。
- 日志 FIFO 取消进程全局共享，第一次入队时按需分配有界存储并保留复用；覆盖容量溢出、独立实例和分配不可用退化路径。探索拾取提示改用本地缓冲，不再借用战斗消息。
- 可移植音频枚举移至 `core/AudioCues.h`，Session/Timeline 不引入音频设备管理器。伤害共享服务头注释同步明确两种适配器的现有提交时机。

## 验证

| 检查 | 结果 |
| --- | --- |
| 原生 AMOLED debug 行为与渲染 | 67 项通过（原 62 项加 5 项模块行为场景） |
| 战斗/页面/成长/架构、保存编码/迁移、桌面协调、共享规则与 RenderDemand | 合并运行 97 项通过 |
| Web 平台适配及取消手势 | 1 项通过 |
| AMOLED V1 / V2 | 两者 lite、lite-debug、Claw 共 6 次构建通过；Claw 实际编译定义均为 1 |
| StickS3 | PlatformIO `m5stick-s3` 构建通过 |
| Web AMOLED V2 | Emscripten 构建通过 |

共 165 项测试通过。新增行为场景直接执行生产 Session/Timeline，覆盖动作提交和显示延迟、首帧音频确认、命中音只播放一次、新鲜时钟、日志 FIFO 容量与隔离、分配不可用、uint32 回绕、预载确认与队序、重复胜利结算、双成员成长、结交拒绝/仓库满/邀请。既有位置绑定检查迁移到模块边界或真实行为，不伪造旧源码位置。

测试命令与日志：完整原生入口为 `AMOLED_RENDER_DEBUG=1 python3 tools/test_amoled_native_render.py`；相关回归为 `test_amoled_battle`、`test_amoled_settings`、`test_amoled_team_layout`、`test_amoled_shop_layout`、`test_amoled_progression`、`test_evolution_sequence`、`test_amoled_page_controllers`、`test_architecture_boundaries`、`test_amoled_source`、`test_team_inventory_controllers`、`test_save_codec`、`test_save_migration`、`test_desktop_platform`、`test_evolution_commit`、`test_battle_action_service`、`test_battle_turn_controller`、`test_battle_status`、`test_render_demand`（`PYTHONPATH=tools python3 -m unittest ...`）。本机日志为 `/tmp/stickmon-battle-*.log`。

## 本批资源对比

相对阶段 0–2 完成后的新鲜 lite 构建。修改前 ELF 与源码归档独立保存；前值应用 bin 由该 ELF 使用同一构建规则的 elf2image 参数重建。字节数如下。

| 目标 | 应用 bin 前 → 后 | 增量 | .dram0.data 前 → 后 | .dram0.bss 前 → 后 |
| --- | ---: | ---: | ---: | ---: |
| V1 lite | 1,397,888 → 1,398,256 | +368 | 23,028 → 23,028 | 51,560 → 50,040（−1,520） |
| V2 lite | 1,718,816 → 1,719,200 | +384 | 23,220 → 23,220 | 53,840 → 52,320（−1,520） |

`.flash.text` 分别 +424 / +436 字节，`.flash.rodata` 分别 −48 / −64 字节；新增边界交接与编译单元改变了内联和布局。本批没有新增素材、修改资源格式、存档 schema 或分区。

静态 BSS 的减少主要是把 1,584 字节 FIFO 移到按需分配的实例存储；首次战斗后该容量保持复用。不能把这项转移报告为总 RAM 节省：满配会话约多 64 字节静态状态，另有堆分配元数据，具体高水位与分配耗时仍需设备测量。

未刷机或进行实机显示、触摸、音频验收。Claw 仅验证现有配置编译兼容；其并发、超时和幂等专项仍延期。后续继续阶段 4 的 ExploreSession/Presenter，再处理房间主体。
