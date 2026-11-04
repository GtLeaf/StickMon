# AMOLED 应用重构

更新：2026-10-26。相关拆分规范集中在本目录；下表是已实施的模块边界，
各页保留对应批次的需求、设计与验收范围，构建数值和设备结果不作为当前版本结论。
当前生产模块见 [app/README](../../firmware/amoled_1_8_v1/main/app/README.md)。

| 模块 | 规范与记录 |
| --- | --- |
| 保存／成长、输入取消与只读控制器 | 本页首批记录：SaveCoordinator、ProgressionFlow |
| 洗澡／孵化控制器 | [ShowerController／HatchController](page-controllers.md) |
| 房间权威状态 | [Home::Actor 与保存坐标转换](home-state.md) |
| 战斗会话／时间线 | [BattleSession／BattleTimeline](battle.md)，共享行动规则见[战斗执行](../battle-execution/README.md) |
| 探索会话／表现 | [ExploreSession](explore.md)、[ExplorePresenter](explore-presenter.md) |
| 自主房间动作／伙伴互动 | [HomeFlow](home-flow.md)、[PairFlow](pair-flow.md) |
| 访客／外出拜访 | [VisitorFlow／VisitAwayFlow](visitor-flow.md) |

探索专用出发／返程门口、伙伴生存编排与 HomePresenter 仍在 App；
通用 InputRouter／NavigationCoordinator／SnapshotBuilder 未提取，renderer／板级物理整理仍待推进。
Claw 专项随迭代暂停。后续优先级应结合当前源码与风险重新评估，不沿用旧批次的“下一批”。
完整待验收项查各批次；实现和编译通过不能替代实机显示／触摸、音频、低功耗、双机或内存测量。

## 首批实施记录（2026-10-18，阶段 0–2）

范围为[结构风险与重构方案](../../doc/代码结构风险与重构方案.md)的阶段 0–2；Claw 专项延期。

## 已完成

- [x] 保存当前未提交源码与 V1/V2 lite 新鲜构建基线，按该基线比较本轮改动；保留原有 UI、资源与其他开发内容。
- [x] 在 App 中复用现有 `SaveCoordinator`。快照或遭遇历史写失败保留 dirty，松手后按受控间隔补存；成功才推进周期保存时钟。
- [x] 保留五分钟普通保存条件、高版本写保护、A/B 机制和孵化完成先提交再清进度的顺序。
- [x] 洗澡保留初次奖励保存延迟；失败统一走 coordinator，防止延迟更新同时触发两条重试路径。
- [x] 收回 Team、Inventory、Hatch、Shower 的可写 `state()`，通过拥有者方法操作手势、选择与反馈。Settings 原已只读。
- [x] 抽出 `ProgressionFlow`，迁移升级、进化、学招式及 28 项会话状态。Flow 不持有 App、硬件、存储或缓存；使用固定大小效果结构交回 App 消费。
- [x] 成长完成后禁止重复推进/结算；取消进化保留物种和石头；覆盖 uint32 时钟回绕。
- [x] 在现有背包/商店、队伍、设置、洗澡及成长入口/出口统一取消残留手势；保留孵化暂停与 IMU 确认机制。
- [x] 修正成长动画拖动/抬手的脏区范围，覆盖正常页眉上方的动态像素。
- [x] 将成长/进化的位置绑定检查改成行为验证，更新架构检查使用权威源码清单。
- [x] 在 [app/README](../../firmware/amoled_1_8_v1/main/app/README.md) 记录输入、更新、模型、绘制与呈现确认契约。
- [x] 完成 host 回归、固件和 Web 构建、Flash/.data/.bss 对比。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| 完整 debug 原生 AMOLED 渲染与流程 | 62 项通过 |
| 页面、成长/进化、Controller、架构/清单、存档编码/迁移、桌面协调与进化提交 | 43 项通过 |
| 战斗、设置、队伍/商店布局相关套件 | 53 项通过 |
| Web 平台及取消手势回归 | 1 项通过 |
| RenderDemand | 2 项通过 |
| AMOLED V1 / V2 | 两者 lite、lite-debug、Claw 配置共 6 次构建通过 |
| StickS3 | PlatformIO `m5stick-s3` 构建通过 |
| Web AMOLED V2 | Emscripten 构建通过 |

上述测试套件共 161 项通过。保存故障注入覆盖快照失败、历史单独失败与孵化提交失败；成长回归覆盖洗澡/照料返回、技能确认与熟练度重置、进化取消和完成、手势消费及局部/全帧一致。Claw 只做现有配置编译兼容，没有迭代其功能或修复延期风险。

## 资源对比

相对本轮修改前重新构建的 lite 基线；单位为字节。`.data`/`.bss` 分别使用 ELF `.dram0.data`/`.dram0.bss`；应用 Flash 使用可烧录 `.bin` 大小。

| 目标 | 应用 bin 前 → 后 | 增量 | .data 前 → 后 | .bss 前 → 后 |
| --- | ---: | ---: | ---: | ---: |
| V1 lite | 1,396,656 → 1,397,888 | +1,232 | 23,028 → 23,028 | 51,512 → 51,560（+48） |
| V2 lite | 1,717,552 → 1,718,816 | +1,264 | 23,220 → 23,220 | 53,792 → 53,840（+48） |

两者 `.flash.rodata` 均无增长；`.flash.text` 分别增加 1,228 / 1,272 字节。小幅增长来自保存调度、封装入口与流程效果交接。未新增素材、修改资源格式、存档 schema 或分区。拆分的收益是所有权与验证边界，不能据此推导堆内存、帧耗时或功耗改善。

`AmoledApp.cpp` 在该首批中从 14,842 行降至约 14,350 行；后续战斗、探索和房间拆分已实施，见上方总览。

## 首批边界与后续进展

- 阶段 3 的 `BattleSession` / `BattleTimeline` 已在[战斗批次](battle.md)实施；阶段 4 的 ExploreSession / ExplorePresenter 已在[会话批次](explore.md)和[表现批次](explore-presenter.md)完成；阶段 5 首批 HomeFlow 见[房间动作批次](home-flow.md)，阶段 5 第二批 PairFlow 见[伙伴互动批次](pair-flow.md)；阶段 5 第三批见[访客/拜访门口批次](visitor-flow.md)；后续处理探索专用出入门编排和 renderer/板级物理整理。
- 通用 InputRouter / NavigationCoordinator 和 SnapshotBuilder 尚未提取；本批先通过现有入口落实取消与保存契约。
- 此首批未刷机或执行实机验收；后续设备记录见对应批次。显示/触摸、锁屏唤醒/深睡、音频、ESP-NOW、IMU/RTC/电池、运行时内存高水位和帧耗时仍待设备验证。
- Claw 并发、超时与动作幂等专项仍随其恢复迭代处理，编译通过不代表这些风险已解决。
