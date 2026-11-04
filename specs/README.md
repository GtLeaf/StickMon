# 项目规范索引

更新：2026-11-04。当前规范按主题组织，短篇需求、设计与任务已合并；
谷仓早期最小房间、三项圆屏菜单方案已被完整菜单与正式首次开机流程替代并删除。

| 主题 | 当前内容与验收边界 |
| --- | --- |
| [AMOLED 应用重构](amoled-app-refactor/tasks.md) | 保存与成长、洗澡／孵化控制器、房间权威状态、战斗／探索会话与表现、房间动作／伙伴／访客流程；分批记录保留各自设备验证范围，Claw 专项延期 |
| [微雪 V2 网页模拟器](amoled-web-simulator/README.md) | 真实应用入口、触摸／暂停／单写存档和发布契约；手机／跨浏览器、远端 CI 与线上部署未纳入本地验收 |
| [共享战斗执行](battle-execution/README.md) | prepare／commit 幂等、随机调用顺序、两端提交时机与应用效果边界 |
| [多设备 UI 隔离](device-ui-isolation/README.md) | 当前家族选择、只读帧／语义动作与生命周期；[十三批验收](device-ui-isolation/acceptance.md)保留历史范围，后续触摸改版按当前规范阅读 |
| [探索成本](explore-costs/README.md) | 行动时间、饱食账本、低饱食出发门槛与自动返家；原批次未刷机 |
| [探索昼夜](explore-daylight/README.md) | 时间／颜色规则、洞窟例外、暂停与局部重绘；实机性能待验证 |
| [探索生态](explore-ecology/requirements.md) | 家族组合、栖息地、独立随机数、遮挡／草坪与暂停；[设计](explore-ecology/design.md)和较长的[演进验收](explore-ecology/tasks.md)独立保留 |
| [探索队伍服务](explore-party-service/README.md) | 三个生产调用方已接入；独立对照、回归／构建与验收补录三项仍未关闭 |
| [探索共享参数与拾取](explore-shared-rules/README.md) | 区域参数、拾取表与随机调用位置；[验收](explore-shared-rules/acceptance.md)保留两端差异和既存失败 |
| [锁屏房间省电](locked-room-power/README.md) | 调度／呼吸／音频与无线释放；音频恢复、双机、电池浅睡与电流实测未闭环 |
| OSTB 3ST 适配（暂停） | 2026-10-30 起暂停；固件、规范和原生工具仅保留本地并由 Git 忽略，不再提交上传 |
| [房间与家具分层](room-furniture-layers/README.md) | v6 房间、独立家具、事务替换与缓存合成；Debug 预览和 Release 边界 |
| [市集四摊位服务方案](market-stalls/design.md) | 待评审草案：变隐龙稀有交易、耿鬼技能服务、袋兽家具与吉利蛋亲密／羁绊培养；价格与开集周期尚未实施 |
| [谷仓菜单与页面](sdgoods-full-menu/README.md) | 八项主菜单、中央 240×240 页面表面与输入映射；[验收记录](sdgoods-full-menu/acceptance.md)保留原批次覆盖 |
| [谷仓硬件、电量与首次开机](sdgoods-hardware/README.md) | 板级驱动、3000–4280mV、真实 180 秒孵化与触摸教程；[硬件验收](sdgoods-hardware/acceptance.md)和[首次开机验收](sdgoods-hardware/onboarding-acceptance.md)保留双机／掉电／放电等待办 |

当前架构另见[多设备 UI 隔离说明](../doc/多设备UI隔离.md)和
[AMOLED 模块边界](../firmware/amoled_1_8_v1/main/app/README.md)。
构建、刷入方式与最新板级状态查各固件目标 README。

## 维护规则

- 当前规范说明行为与所有权；较长且有价值的验收记录单独保留，并注明日期与覆盖范围。
  实现完成不代表所有实机或整仓测试通过，未完成项不能在合并时补勾选。
- 短篇需求／设计／任务优先合为一个主题页，相关重构子项收在同一目录。方案整体被替代时
  删除旧文并更新引用，不保留空壳或并行的旧标准；仍有效的硬件依据与未闭环事项应迁入现行文档。
- 历史行数、构建大小、固件哈希、测试数和失败只描述对应批次；新进展应更新当前入口，
  不让旧“下一批”成为当前排期。源码与文档冲突时，核对代码、测试和最新实机证据后修正。
- 截图、日志、冻结源码、临时报告和整理备份统一放 Git 忽略的 `.stick-local/`，详见
  [本地产物管理规范](../doc/本地产物管理规范.md)。历史 `/tmp/` 路径只作原始记录，
  证据缺失时不能据旧文字补写通过结论。
