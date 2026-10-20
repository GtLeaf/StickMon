# 访客与拜访门口流程拆分

日期：2026-10-19。延续已批准的阶段 5，Claw 专项延期。

## 范围与验收

- `VisitorFlow` 持有房主迎接/开门/让位、访客进门/落脚/离场的状态、路径编排及期限；`VisitAwayFlow` 持有本机精灵外出拜访的走门/跨门/离家状态。
- 共享 `Simulation`、`VisitArrivalFlow`、`VisitDepartureFlow` 保持规则权威；通信/队伍变更、PairFlow 聊天、保存、资源缓存、相机/帧/脏区由现有拥有者负责。
- 保留 PRNG 顺序、不可达/超时/零时长处理、footprint 与地面偏移语义、主客门资源交接、中途离场/探索取消、结束一次性效果。
- 通过 const 状态和固定大小效果交接，模块不持有 App/硬件/存储/缓存；不新增逐帧分配或静止重绘。
- 不改玩法、素材、存档、分区、通信协议与探索专用出入门状态机。

## 任务

- [x] 保存旧源码/固件/资源哈希并记录原 App 行为指纹。
- [x] 实施两个独立状态拥有者，明确移动与表现/外部动作的交接顺序。
- [x] 完成行为、架构、存档、共享状态机和原生渲染回归。
- [x] 完成固件/Web 构建、资源与内存体积对比。
- [x] 备份设备并刷入 V2，验证可执行的门口路径、保存与重启。

## 边界、顺序与修复

两个 Flow 使用 const `state()` 和固定大小的可消费效果；不持有 App、Platform、存储或渲染缓存。`VisitorFlow` 借用同一个 `VisitArrivalFlow`，让 PairFlow 继续拥有迎接聊天。通信服务、临时队员移除、调试联系人的收尾、缓存同步及保存仍由 App 执行。

移动交接顺序为 `update → takeEffects → App 相机/动画/脏区 → completeMovement → takeEffects`。访客落脚点不可用时才请求原有随机目标选择并确认一次，保留 PRNG 顺序。直接移动完成入场/出场后，先执行原有聊天/队伍收尾，再继续最后一帧与脏区更新。外出取消保持原帧截止时间，不重置动画时钟。

另修复原有的迎接中取消入场问题：房主在 HOST_APPROACH 阶段仍持有 DOOR，旧实现直接申请访客 DOOR 会导致共享 Simulation 不变量失败；现在先释放房主、停止其门口动作，再交给访客。该行为改善由独立回归覆盖，未通过重录 golden 掩盖差异。

## 验证结果

- 77 项 debug 原生测试、24 个相关套件的 99 项检查通过，共 176 项。源码位置检查已按新所有权调整，行为断言保留。最初两项旧源码位置断言失败，调整后该 29 项套件全部通过。
- 拆分前真实 App 单独编译记录 8 种子 × 8 场景 × 普通/回绕时钟，共 128 条指纹；角色轨迹/路径/任务、门资源、入场/聊天阶段、动画/脏区及后续 RNG 全部一致。场景覆盖不能移动物种、房主越界、断路、开门让位/入场超时、中途离场和直接出门回退。入场零时长不会提前显示访客。
- 额外行为测试覆盖 HOST_APPROACH 提前离场、探索取消时资源释放、落脚确认/效果不重复、本机寻路/直线/超时外出、各阶段取消、完成通知一次及 uint32 回绕。共享通信服务测试通过；原有 HomeFlow / PairFlow 指纹与原生局部/全帧对比继续通过。
- V1/V2 的 lite、lite-debug、Claw 六配置，StickS3 与 Web AMOLED V2 构建通过；Claw 仅验证编译兼容。
- App 主文件 10,734 → 9,999 行，头文件 721 → 693 行。VisitorFlow 为 613/102 行，VisitAwayFlow 为 100/52 行。

| 目标 | lite bin 前 → 后 | 增量 | .data | .bss 前 → 后 |
| --- | ---: | ---: | ---: | ---: |
| V1 | 1,401,536 → 1,400,400 | −1,136 B | 23,028 不变 | 50,232 → 50,400（+168 B） |
| V2 | 1,722,560 → 1,721,392 | −1,168 B | 23,220 不变 | 52,512 → 52,680（+168 B） |

`.flash.text` 分别 −300 / −320 B，`.flash.rodata` 均 −848 B。日志格式化留在 App，转换为有界的阶段事件；删除原逐帧访客诊断和重复字符串。BSS 增长来自效果交接、挂起移动与独立拥有者状态，不引入逐帧堆分配。158 个资源文件哈希未变；未改 schema、分区或协议。未据此推断帧耗时、堆高水位或功耗改善。

## V2 串口自动实机回归

- 设备 USB/串口曾卡住；esptool 复位及仅匹配该设备序列号的 USB 重枚举后恢复，不需人工拔插。备份原 NVS/应用区域后确认分区表匹配，仅更新 `0x20000` 的最终 V2 lite-debug 应用，刷写哈希校验通过。资源、bootloader、分区与其他数据分区保持原镜像。
- 新固件首先读取到原有快照：steps=4、coins=6220、team=2、storage=5、HP=89/56、level=32/23、potion=13。
- 为满足现有访客诊断要求，先通过正常队伍页把第二成员暂存通讯录。随后重启并执行提示/接受：host-approach → host-ready → visitor-visible → entered → talk positioned → talk-ready → exit → finish → complete，队伍收尾为单个永久成员。已观测完整新 VisitorFlow 与原 PairFlow 的交接。

- 访客收尾后验证探索出门、自然遭遇/逃跑、路线菜单暂停和返程。首次脚本假设能直接打开路线菜单，遇到自然战斗而超时；改为处理战斗后继续，最终出入门、角色释放和 RAM/磁盘保存核对通过，未修改游戏代码以适配脚本。
- 恢复完整原始 NVS（0x9000 / 0x6000），停留 bootloader 回读后逐字节相同；再重启，RAM/磁盘均读取测试前的 steps=4、coins=6220、team=2、storage=5、HP=89/56、level=32/23、potion=13。测试期间的暂存队友、访客关系/奖励和探索变化均不留存。私有备份为仅当前用户可读文件，不进入仓库。
- 成功运行日志未见 panic、watchdog、abort 或堆损坏。串口注入验证设备上的应用流程，不证明显示像素、实体触控、听音或电源/深睡正确。单台设备未执行真实 ESP-NOW 双机拜访；本机 VisitAwayFlow 完整生命周期由 host 测试覆盖，共享会话协议由其已有 host 测试覆盖。

证据：`/tmp/stickmon-visitflow-native.log`、`/tmp/stickmon-visitflow-shared.log`、`/tmp/stickmon-visitflow-home-source.log`、`/tmp/stickmon-visitflow-sizes.json`、`/tmp/stickmon-visitflow-device-visitor-result.log`、`/tmp/stickmon-visitflow-device-route-result.json`、`/tmp/stickmon-visitflow-nvs-restored.json`、`/tmp/stickmon-visitflow-reboot-result.json`。旧源码/固件与指纹日志路径由 `/tmp/stickmon-visitflow-baseline-path` 记录。

下一批优先提取探索专用的出发/返程门口编排（含 fade 确认、入门、让位和取消），App 保留导航/真实呈现确认。之后处理房间生存编排与 HomePresenter；继续复用共享规则，避免合回 HomeFlow。Claw 专项继续延期。
