# 多设备 UI 隔离

当前行为与模块边界见[隔离规范](../specs/device-ui-isolation/README.md)，
[十三批验收目录](../specs/device-ui-isolation/acceptance.md)提供阶段入口。
下文按批次保留实现与验证历史；各节旧顶栏尺寸、待拆页面及“不加触摸”只说明当时范围，
当前 OSTB 布局以[圆角规则](../specs/ostb-3st-port/safe-area.md)为准。

2026-10-23：横屏场景及应用外层 UI 隔离已完成代码收口，包含十一组菜单、主房间、探索／战斗、洗澡、无线通讯、Debug、成长、孵化、菜单叠层和应用外层导航／资源提示／淡入淡出。
第十三批验收记录见文末；AMOLED 按原设计保持独立竖屏应用，真实设备验收仍待完成。
保持现有界面和玩法，用构建选择界面家族。

## 当前边界

| 位置 | 职责 |
| --- | --- |
| `src/game/ShopService`、`ItemInventory` 等 | 价格、库存、解锁、效果和游戏状态规则 |
| `src/core/features/ShopFlow` | 共用商店目录、选择、购买／出售流程，调用 ShopService |
| `src/core/features/BagFeature` | 共用背包目录、居家／战斗可见性、使用目标与语义结果 |
| `src/core/features/TeamFeature` | 共用成员／访客身份与队伍操作能力查询，复用 TeamRoster |
| `src/core/features/MovesFeature` | 共用技能目录、真实槽位映射、回忆选择／替换语义 |
| `src/core/features/RoomFeature` | 只读食物库存、有效选项、房间动作与洗澡能力查询 |
| `src/core/features/ContactsFeature` | 复用 ContactRoster 查询联系人能力、动作和邀请限制 |
| `src/scenes/ShopScene`、`MenuScene`、`SettingsScene`、`MainScene`、`ExploreScene`、`ShowerScene`、`SocialScene`、`HatchScene` | 应用协调：导航、教程、通知、保存标记、游戏变更、设置硬件副作用、进化和战斗结果 |
| `src/ui/common/` | 共用接口、反馈、图标和确认弹窗；不复制玩法规则 |
| `src/ui/stick/` | Stick 十一组菜单、主房间、探索／战斗、洗澡、无线通讯、Debug、成长、孵化及公共叠层的排布、A/B 输入、列表／文字动画与状态倾斜滚动 |
| `src/ui/compact_touch/` | OSTB 十一组菜单、主房间、探索／战斗、洗澡、无线通讯、Debug、成长、孵化及公共叠层的排布、点击／拖动、物理按键、状态跟手切页／回弹 |
| `src/ui/ApplicationUi.h`、`SceneInput.h` | 构建选择应用外层绘制及通用触摸命中／虚拟按键策略 |
| `src/ui/UiProfile.h` | 界面家族、显示尺寸、顶部边距和输入能力 |
| `src/platform/` 与固件板级实现 | 屏幕传输、触摸采样、电源及其他硬件 |

`ShopScene` 委托选中的 `ShopUi`；`MenuScene` 的对应页面委托 `MainMenuUi`、`BagUi`、`TeamUi`、`StatusUi`、`MovesUi`、`RoomUi`、`FoodUi`、`ComputerUi`、`ContactsUi`、`DebugUi`（仅 debug）。
`SettingsScene` 委托所选 `SettingsUi`，包含设置菜单、帮助与重置确认三个视图。
`MainScene` 委托所选 `HomeUi`，保留主房间的活动、访客、碰撞和存档协调。
`ExploreScene` 委托所选 `ExploreUi`，保留路线、遭遇、战斗时序、资源加载及存档协调。
`ShowerScene` 委托所选 `ShowerUi`，保留洗澡阶段、泡沫规则、奖励、消耗、保存及退出协调。
`SocialScene` 委托所选 `SocialUi`，保留链路生命周期、会话动作和成功后返回主房间的协调。
`HatchScene` 委托所选 `HatchUi`，保留孵化进度、计数、运行时恢复、保存和完成选种；
`MenuScene` 通过 `MenuOverlayUi` 绘制底色、通知及教程，保持原分支的可见性和叠层顺序。
`GameEngine` 通过 `ApplicationUi` 绘制底部导航、资源缺失提示及淡入淡出；保留输入拦截、资源诊断、过场时钟、场景切换和帧提交。`Scene` 通过家族 `SceneInput` 获得命中与按键语义，保留 `TouchTap` 和回调分发。
背包返回语义动作，场景继续协调原有 GameEngine 操作，避免遗漏保存、进化和战斗联动。
布局坐标、滚动量、触摸状态不放进共享功能流程。

Stick 的 ShopUi 只有横向栏目和选中项动画状态；触屏版本只有网格／详情和触摸状态。
BagUi 的家族专用字段在编译时选择，Stick 不携带触屏滚动状态。
两家族各自的实现不相互引用，通用绘制保留在 common。

`MenuPageHost` 给主菜单及队伍／状态／技能／房间／食物／电脑／通讯录／Debug 子页提供只读游戏状态、导航、教程和通知接口。
各专用 host 回调继续调用原 GameEngine 服务，保留存档标记和场景副作用；
带精灵名的通知由场景复制保存，避免引用界面模块的临时字符串。

队伍的动作顺序和展示差异保留：compact_touch 隐藏访客的首位／离队，
Stick 保留原动作展示，操作仍经共享规则校验。`MovesFeature::Flow` 不拥有确认框、
滚动坐标或设备输入；`MovesUiBase` 共用技能变更反馈。
`StatusUiContent` 共用相同的信息绘制；视口、导航、指示器及交互由家族持有。
OSTB 的临时切页图只在交互期间分配，完成／取消／离页／销毁时释放；
Stick 不包含这一组触摸或缓存字段。

房间／食物的共用语义动作位于 `RoomUiBase`／`FoodUiBase`：选择食物调用原
`GameEngine::selectFood`，洗澡通过原场景请求回调进入；库存不会因查看、选择或拖动而消耗。
`RoomFeature` 不写状态，复用 ItemInventory 查询库存，无效食物索引按原查询归一为 0。
Stick 房间总库存保留 255 封顶，OSTB 保留完整总量。两家族分别持有房间／食物游标、滚动与
输入状态；仅 Stick 持有食物说明跑马灯。OSTB 房间使用安全顶部栏，食物页继续原 38px 顶栏，
本次未扩大圆角调整范围；提示期限／文本和教程进度由场景管理，绘制委托 `MenuOverlayUi`。

电脑入口的布局、选中项和输入由家族持有，Stick 保留列表，OSTB 保留 74px 卡片与
38px 顶栏。`ComputerUi::showsNotices()` 保留原来的通知显示差异：OSTB 叠加通知，
Stick 电脑页不显示。通讯录两家族目前使用相同旧列表和弹窗，因此 `ContactsUiContent`
共用内容绘制，`ContactsUiBase` 共用选择／确认状态及语义动作，家族各自持有滚动和输入。
通讯录继续使用底部返回／选择／确认；本批没有重做样式或扩大圆角适配。

`ContactsFeature` 复用 ContactRoster，邀请与删除仍调用原引擎服务。host 保留来访／每日锁定
查询、队伍成员实时状态页映射、保存标记和到达演出。邀请结果为 `JOINED` 时，界面先关闭
操作弹窗，再调用 `contactJoined()` 同步切场景，避免退出逻辑之后继续改写界面状态。

设置的选择、帮助／重置确认和通知由 `SettingsUiBase` 共用，Stick 单独持有列表缓动，
compact_touch 单独持有滚动、滑杆和触摸状态；各家族保留原列表顺序、布局与顶栏。
独立的 `SettingsHost` 查询当前状态及显示值，统一执行硬件设置、试听、保存标记、
重播教程与重置／导航。现有变更只有一个应用入口，不额外复制一套玩法服务。
Stick 入页音量按 10% 取整，compact_touch 保留滑杆精确音量；亮度按原硬件值切档。
触摸松手清理在激活前完成，返回、重播教程和重置成功同步切场景后立即返回。

主菜单的当前／上次游标、选择推进和教程通知由 `MainMenuUiBase` 共用，条目数与动作映射
复用 `AppSceneFlow`，保留 release 八项／debug 九项。`MainMenuHost` 扩展 `MenuPageHost`，
由 `MenuScene` 激活条目或返回主场景，保留探索拦截、原 1100ms 提示和导航副作用。
Stick 单独持有选中项缓动及电量状态，每 30 秒检查电量；compact_touch 单独持有网格滚动与
触摸，静止后停绘。保留原公开点击接口的返回目标语义；点击和按键激活可能同步销毁菜单，
返回途中不再访问其成员。

主房间以 `HomeFrame` 提供只读的世界坐标、精灵帧／姿态、HUD 数值和对话／教程信息。
家族 `HomeUi` 持有相机、状态栏、提示位置、菜单入口与命中映射；Stick 保留覆盖房间的紧凑
状态栏，OSTB 保留 240×161 原比例房间居中和上下留白。`HomeUiBase` 共用按键语义映射、
轻点取消、通知期限、饥饿动画及 debug 组合保存状态；保存动作通过 `HomeHost` 调用原引擎。
`HomeUiContent` 共用房间资源、精灵、脚底阴影、食物、爱心、光照、访客对话、教程和调试
边界的像素绘制，保留原深度排序和逐层裁切。两家族继续复用 `RoomRenderer`，其资源采样和
AMOLED `drawViewport` 路径不变。

`MainScene` 保留角色活动、寻路、碰撞脚印、食盆资源仲裁、访客／门过场、成长队列、
摸头／添粮效果和位置存档。UI 不引用引擎，不持有可写游戏状态；成长弹窗经原 host 调用
`core/ProgressionUi` 应用适配器，由选定家族的 `ProgressionUi` 绘制。主房间继续每 66ms 更新，
通知只在原来的正常更新末段到期，过场的提前返回不改变这点。
本批顺带修复原来“接受来访者的探索邀请后仍写入已销毁场景”的问题，将选项复位前移。

探索／战斗通过 `ExploreHost` 提供只读 `ExploreRouteFrame`／`ExploreBattleFrame`，
地图只传常量指针，精灵／日志引用由场景持有。`ExploreUiBase` 管理区域／侧菜单／战斗命令
游标、确认选择和语义输入；蛇行展示朝向等纯视觉状态归 UI。两家族各有编译选择的入口和
显式宽高参数，相同的区域、地图和战斗绘制复用 `ExploreRouteUiContent`／`ExploreBattleUiContent`，
无需复制相同界面。布局公式、遮挡／深度排序、精灵尺寸和提示位置不变；本批不增加圆角内缩、
顶部导航或探索／战斗触摸入口，嵌入的探索菜单仍只转发原按键。

`ExploreScene` 保留路线与碰撞、拾取／头目／遭遇、回合和 HP 提交、日志、EXP／投食／换人
时序、成长和菜单暂停协调。游标缓动与解码调度分离，保留最终游标帧先绘、80ms 后解码，
轮播 2800ms、移动 500ms；日志 1000ms、出手 960ms、投食 700ms、换人每阶段 360ms／120px、
EXP 900ms 均保持。共用时间常量位于 `ExploreBattleTiming`，不借用 AMOLED 不同参数的竖屏控制器。
出发成功立即返回；调试逃跑日志结束显式报告场景已替换，更新不再访问旧对象。

洗澡通过 `ShowerHost` 提供只读 `ShowerFrame`，界面接收语义动作，游戏变更仍由
`ShowerScene` 调用原 `BathService`／`BathInteraction` 和引擎服务。`ShowerUiBase` 持有
菜单／肥皂／退出选择、工具位置、IMU 平滑、水滴、氛围、跳跃／摇摆／转身、浮字和提示；
`ShowerUiContent` 共用原横屏绘制，两家族入口显式提供布局尺寸。相同输入实现仍可共用，
不为隔离复制两份相同代码。Stick 保留 IMU，OSTB 保留无 IMU 时按住 A 擦洗，未增加触摸拖动。
洗澡阶段、泡沫合并／冲洗、一次性奖励、1／2／3 心恢复及库存保存均不变；时间常量在
`ShowerTiming` 共用。未完成提示达到 700ms 后同步切场景立即结束更新，避免继续访问已销毁对象。

无线通讯经 `SocialHost` 提供只读 `SocialFrame`，继续使用原 `VisitSessionService::ViewModel`。
`SocialUiBase` 持有菜单／房间游标、状态提示起点、房间数观察和等待点时钟；
`SocialUiContent` 共用菜单、房间列表、请求确认、状态及错误画面，两家族显式提供宽高。
`SocialScene` 保留会话状态观察及 trace、进入／退出时的链路管理、会话服务调用和 ACTIVE
导航。UI 不调用链路或变更游戏状态；原按键优先级、500ms 等待点及 66／200ms 唤醒保持。
静态页继续停绘，长按返回和连接成功的同步销毁后立即返回。没有新增无线通讯触摸入口，
AMOLED 继续原独立通讯控制器；协议、重试／超时和访客规则不变。

## 家族与设备分开

当前有 `STICK` 和 `COMPACT_TOUCH` 两个家族，以 `STICKMON_UI_COMPACT_TOUCH=0/1`
显式选择，不根据分辨率或运行时有没有触摸设备来猜测。

设备参数包括宽高、`headerInsetX`、`headerTop`、触摸／按键能力。
OSTB 使用 296×240、顶部左右内缩 28px、顶部 4px；Stick 为 240×135。
顶部安全区只约束顶部导航，正文仍能延伸到屏幕两侧。

现在的 compact_touch 保留已经确认的详情坐标和控件尺寸。新设备尺寸差异很大时，
仍需检查详情排布、圆角和点击区；选择同一家族不代表任意分辨率已经自动适配。

Debug 子页通过 `DebugHost` 查询当前时间／光照／运动／边界开关，向 `MenuScene` 提交
恢复、升级、切换精灵、金币、时间、开关、战斗及联系人事件等语义动作。`DebugUiBase`
保留原分类、光标、滚动、三位物种／四位时间数字弹窗和输入；家族提供显式尺寸。原空队伍、
满级、倒下、伙伴数及事件忙碌拦截、通知时长和战斗返回不变。`STICKMON_ENABLE_DEBUG_FEATURES`
完整包围 Debug 类型与实现，release 不携带实例或链接 Debug UI；保留原底部虚拟 A/B 导航。

`core/ProgressionUi` 现在只作为兼容原场景调用的应用适配器：构造只读 `MoveLearnFrame`、
执行原学招式／替换接口和叫声效果。所选家族的 `ProgressionUi` 及共用 `ProgressionUiBase`
持有原升级／进化／取消／学招式／替换绘制、视觉时钟和纯输入决策，UI 不读取引擎。
确认仍默认取消，替换失败且待办仍存在时关闭确认以便重试；成长队列、存档和演出调用顺序不变。
两家族当前共用相同内容，通过独立家族入口演进；AMOLED 不引用该横屏适配器或 UI。

## 构建选择

- PlatformIO 的 Stick release／debug：`core/features`、`ui/common` 和 `ui/stick`，排除其他 UI 实现。
- OSTB CMake：`core/features`、`ui/common` 和 `ui/compact_touch`，不加入 `ui/stick`。
- `tools/build_web.py` 的 `sources_for` 与 `ui_flags_for` 同时定义 host／Web 源列表和配置。
  `sticks3` 选择 Stick，`ostb` 供原生 host 使用；Web 公开构建目标仍是 sticks3／amoled-v2。
- AMOLED V1／V2 继续使用原来的竖屏 UI 和显式源清单；两者仍共用 game 服务，
  本次没有把 InventoryController 的滚动／动画搬进共享功能。

增加源文件后，OSTB 需要执行 CMake reconfigure。ESP-IDF 组件在 script mode 下读取源列表，
不要为这些 GLOB 添加不受支持的 CONFIGURE_DEPENDS。

## 后续接入方式

1. 硬件接入平台服务，明确分辨率、输入能力和圆角参数。
2. 能沿用现有交互时选择对应家族，检查各页面的排布与点击区。
3. 交互差异较大时增加新家族及构建源选择，使用共享 feature 的查询／语义动作。
4. 新玩法先进入 game 服务；保存、场景跳转等跨功能工作留在应用协调层。
5. 用相同初始状态验证不同界面操作后的游戏结果，再检查各家族画面。

当前已覆盖商店／背包／队伍／状态／技能／房间菜单／食物选择／电脑入口／通讯录／设置／主菜单十一组菜单、主房间、探索／战斗、洗澡、无线通讯、Debug 子页和共用成长表现。
剩余横屏 UI 为 `HatchScene` 孵化页，以及 `MenuScene` 公共底色／通知／教程叠层；AMOLED 仍用原独立竖屏应用。
后续按页面逐批迁移，当前范围不代表全部设备 UI 已分离。

## 本次验证

- `test_shop_flow.py`、`test_bag_feature.py`：共享目录、交易失败原子性、每日限制、
  背包目标选择、食物投掷与战斗结果等。
- `test_item_ui_parity.py`：通过真实场景公开入口，在两家族执行购买／出售／使用／取消，
  操作结果相同；17 张帧缓冲的 SHA-256 与重构前一致。基线在
  `tools/fixtures/item_ui_parity.json`，以后有意调整画面时需审查并更新。
- `test_team_feature.py`、`test_moves_feature.py`：正式成员／访客／探索能力、技能稀疏槽位、
  查询不修改状态、回忆失败原子性、消耗恰好一枚心之鳞片和遗忘规则。
- `test_party_ui_parity.py`：两家族各 36 帧与第二批重构前的像素基线一致；
  通过真实场景按键入口验证首位、状态翻页、取消／确认遗忘、回忆和离队，结果相同。
  基线为 `tools/fixtures/party_ui_parity.json`，包含队伍／弹窗／访客／探索、
  队伍与仓库五页状态及滚动、蛋、技能／遗忘确认／回忆／替换。
- `test_ostb_native.py`：原有完整触摸、物理键、锁屏、静态停绘和场景检查通过；
  75 张 PPM 与重构前逐字节一致。
- `test_ui_family_sources.py`：8 项检查通过，覆盖互斥源选择、配置、错误参数、
  ESP-IDF script mode 源列表与 host 一致、共享功能依赖边界，以及场景不再持有已迁移页面几何／动画状态。
- render demand、架构边界及存档编码／迁移检查通过；存档结构和版本未修改。
- 第二批四目标构建通过：Stick 1,410,384 B；OSTB 1,349,952 B；
  AMOLED V1 lite 1,403,424 B；AMOLED V2 lite 1,724,800 B。
  以上为最终 app 二进制文件大小；实际编译记录确认 Stick／OSTB 没有编译对方的界面源文件。

第三批房间／食物记录：

- `tools/fixtures/room_ui_parity.json` 保存重构前 45 帧：Stick 26、OSTB 19，覆盖房间、
  食物选中／空库存／总量、说明跑马灯、滚动和提示。`test_room_ui_parity.py` 验证像素与真实场景操作，
  45 帧均与基线一致；两家族选择／空库存、触摸拖动与取消、返回栈、静态停绘、
  孵化及空队伍拦截和洗澡场景均通过。
  相同操作断言也在冻结的重构前源码上通过，确认本批保留原行为。
- `test_room_feature.py`、`test_ui_family_sources.py`、`test_item_ui_parity.py`、
  `test_party_ui_parity.py`、`test_render_demand.py`、`test_architecture_boundaries.py`
  共 26 项测试通过；原有商店／背包 17 帧、队伍／状态／技能 72 帧保持一致。
- `test_ostb_native.py` 通过，完整 75 张 PPM 与本批冻结前基线逐字节一致。
  本次未修改存档 schema；前两批已通过的存档编码／迁移检查仍为兼容依据。
- 四目标构建通过：Stick 1,412,256 B；OSTB 1,350,608 B；
  AMOLED V1 lite 1,403,424 B；AMOLED V2 lite 1,724,800 B。以上为 app 文件大小，
  Stick／OSTB 已覆盖最后的 MenuScene 修改；AMOLED 独立竖屏源集合未变，增量兼容检查通过。
  构建未修改依赖锁文件，未刷机。

第四批电脑／通讯录记录：

- `tools/fixtures/contacts_ui_parity.json` 由冻结的重构前源码生成两家族各 48 帧，合计 96 帧，
  覆盖电脑入口、通讯录列表、空列表、滚动、各动作与邀请／删除确认、羁绊和限制提示；
  `test_contacts_ui_parity.py` 通过：96 帧均与基线一致，两家族各 14 组真实旅程通过。
  包含查看、邀请／删除确认与取消、限制、返回及停绘；确认后联系人失效仍保留旧提示并
  延长 1400ms，邀请成功后由引擎进入新主场景并触发 `TEAM_ENTER_CROSS` 到达演出。
- 两家族 ASan／UBSan 检查通过，覆盖引擎持有菜单时邀请成功、同步销毁旧场景和进入新主场景，
  未发现悬空访问或未定义行为。
- 原有商店／背包 17 帧、队伍／状态／技能 72 帧、房间／食物 45 帧全部保持一致。
  `test_ostb_native.py` 通过，75 张 PPM 与本批冻结前逐字节一致。
- `test_ui_family_sources.py` 8 项通过，包含电脑／通讯录的家族源选择及场景委托边界。
- 独立 12 脚本共 25 项回归通过：联系人／房间／商店／背包／队伍／技能共享功能、
  ContactRoster、Friendship、渲染调度、架构、存档编码及迁移。本批未修改存档 schema。
- 四目标构建通过，当前 app 文件大小：Stick 1,416,160 B；OSTB 1,353,120 B；
  AMOLED V1 lite 1,403,424 B；AMOLED V2 lite 1,725,024 B。
  Stick／OSTB 编译记录确认只加入各自家族源，并覆盖本批最终界面源码；依赖锁文件未改动。
  未刷机，实机交互和性能留待后续验收。

第五批设置记录：

- 冻结旧源码生成 Stick 37 帧、OSTB 38 帧，共 75 帧设置、帮助与重置确认基线。
  `test_settings_ui_parity.py` 验证新实现全部一致，两家族各 20 组真实操作旅程通过。
  覆盖音量取整／精确值、五档亮度、静音／试听、速度／省电、帮助与重播、重置取消／失败／成功、
  滑杆横纵隔离／取消／重复松手、保存标记和停绘；同一操作断言在冻结旧源码上通过。
- 两家族 ASan／UBSan 通过，覆盖引擎持有场景时触摸返回、帮助最后页重播和重置成功的同步销毁。
  将原触摸松手时的清理移至激活前，消除切场景后访问旧界面的风险。
  `test_ostb_native.py` 通过，完整 75 张 PPM 与本批前基线逐字节一致。
- 原有商店／背包 17 帧、队伍／状态／技能 72 帧、房间／食物 45 帧、电脑／通讯录 96 帧
  均与原基线一致；源隔离 8 项通过，已覆盖设置场景委托与互斥源选择。
- 渲染调度、架构、存档编码与迁移共 17 项检查通过；本批未修改存档 schema。
- 四目标构建通过，当前 app 文件大小：Stick 1,414,160 B；OSTB 1,352,464 B；
  AMOLED V1 lite 1,403,424 B；AMOLED V2 lite 1,725,024 B。
  Stick／OSTB 实际仅编译各自家族，所有选中源码的对象时间均覆盖最终实现；
  依赖锁文件未改动。未刷机，实机交互和性能留待后续验收。

第六批主菜单记录：

- 冻结旧源码生成四组合共 123 帧主菜单基线：Stick release 28／debug 29、
  OSTB release 32／debug 34；`test_main_menu_ui_parity.py` 验证新实现像素全部一致，
  四组合各 24 组真实操作旅程在新旧实现上通过。
- 四组合 ASan／UBSan 及严格语法检查通过，覆盖引擎持有菜单时触摸进入商店／设置／
  探索／主场景、A／B 长按及 compact_touch 直接返回目标的同步销毁；
  debug 仅覆盖入口与战斗返回请求，未将 debug 子页纳入本批隔离。
- `test_ostb_native.py` 通过，完整 75 张 PPM 与本批前逐字节一致；原五批
  17+72+45+96+75=305 帧及各自真实旅程回归通过。
- `test_ui_family_sources.py` 8 项通过；AppSceneFlow、渲染调度、架构、存档编码／迁移
  共 18 项通过。本批未修改存档 schema。
- 四目标构建通过，当前 app 文件大小：Stick 1,413,536 B；OSTB 1,351,024 B；
  AMOLED V1 lite 1,403,424 B；AMOLED V2 lite 1,725,024 B。
  Stick／OSTB 分别只编译所选家族，生产源码及相关新头文件均被最终对象覆盖；
  V1／V2 独立竖屏源集合未变，增量兼容检查通过。依赖锁文件未改动，未刷机；
  实机交互和性能留待后续验收。

第七批主房间记录：

- `tools/fixtures/home_ui_parity.json` 由冻结旧源码生成，Stick release 172／debug 173、
  OSTB release 172／debug 173，原有 690 张完整 RGB565 画面保持一致。
  另从同一冻结旧源码追加 56 张小房间 HUD／提示画面，覆盖上下留白 23／24、30／31、
  33／34、50px，一／双精灵与低／未知电量；最终每家族 release 186／debug 187，
  合计 746 张逐字节一致，原 690 张哈希未改。
  覆盖原比例房间与相机、一／双精灵、阴影／姿态／浮空／落地／睡眠、昼夜／光照、
  食物／爱心、HUD／电量／饥饿动画、访客对话、教程、成长弹窗与 debug 边界。
- 四组合各 24 组真实操作检查在新旧源码上通过，包括摸头限制、添粮／满碗／混粮／
  无库存、菜单与教程、输入拦截、拖动／取消／重复松手、位置保存与恢复、过场和正常更新的
  通知时序。新实现四组合严格语法及 ASan／UBSan 通过，覆盖引擎持有场景时按键／触摸
  进入菜单和接受来访者探索邀请时的同步销毁；后者已消除旧场景销毁后的选项写入。
  补齐小房间留白后，Stick release／debug 严格语法与 ASan／UBSan 再次通过。
- 原有六套菜单回归的 428 帧及真实操作、六套 feature／flow 均通过；完整 OSTB 原生
  75 张 PPM 与本批冻结前逐字节一致，未更改既有像素基线。
- 源隔离 8 项、MainScene 静态契约 42 项通过；适配了六处在冻结旧源码上也已失败的
  旧状态赋值／睡眠断言，保留实际协调器、等待时序、饱腹阈值与空间不变量。
  共享 HomeActorController／Simulation／Coordinator／Runtime／Chase／VisitArrival／
  VisitDeparture／Care／Hud、访客会话、成长规则、教程、架构、渲染调度及存档编码／迁移通过。
  教程重播的源断言同步适配原有引擎引用，实际操作由设置回归继续覆盖。
- 四目标最终构建通过：Stick 1,414,736 B；OSTB 1,351,968 B；
  AMOLED V1 lite 1,403,424 B；V2 lite 1,725,024 B。Stick／OSTB 只编译所选家族，
  各目标生产源码与依赖头文件均被最终对象覆盖，包含 Stick 小房间留白保真补丁。
- 本批只改变 MainScene 的应用适配和新增 HomeUi 文件；其他已有生产源码、存档 schema、
  房间资源、AMOLED 独立竖屏源集合和依赖锁均未改变。未刷机，实机性能留待验收。

本次未刷机，实机交互和性能需要后续验证。额外运行的现有 test_amoled_source.py
发现附加源数量断言落后于已有电池实现（V1 实际 6、断言 5）；该历史断言未在本次改动中调整。


第八批探索／战斗记录：

- `tools/fixtures/explore_ui_parity.json` 完全由冻结的重构前源码生成，Stick／OSTB 各
  release 321／debug 322，合计 1,286 张完整 RGB565 画面。新实现逐像素一致；覆盖区域
  解锁／轮播／剪影、六种地图、遮挡与跟随者／头目排序、蛇行连续帧、菜单、全部提示、
  战斗状态／日志／HP／EXP／换人／投食及成长表现。
- 每配置 36 组真实操作检查在冻结旧源和新实现上通过，包含按键优先级、冰面输入锁、
  侧菜单与子页、路线暂停时间补偿、80ms 延后预加载、停绘／定时唤醒、战斗命令与动画期限、
  结交／特殊遭遇确认及返回。四配置严格 host 语法与 ASan／UBSan 通过。
- 独立使用冻结旧源的 ASan 重现选择区域出发、调试逃跑日志结束两处释放后访问；新实现
  通过引擎真正持有并同步销毁场景的路径检查。没有用堆栈上的伪场景代替销毁验证。
- 原六套菜单的 428 帧、主房间 746 帧及真实操作／六套 feature 回归通过；完整 OSTB
  原生 75 张 PPM 与第八批冻结前逐字节一致，原像素基线均未改动。架构、渲染调度、
  AppSceneFlow、存档编码／迁移与源选择检查通过。
- 探索／战斗、教程、精灵资源和源隔离 12 脚本共 70 项通过，源断言仅迁移到新的
  绘制／输入位置，玩法断言保留。额外运行的 `test_item_system.py` 有 5 个跨历史重构
  的旧源断言失败，冻结前和当前失败集合相同；其余 4 项通过，本批未修改这些旧断言。
- 四目标构建通过，最终 app：Stick 1,417,936 B、OSTB 1,354,928 B、AMOLED V1 lite 1,403,424 B、V2 lite 1,725,024 B。
  本批只修改既有 ExploreScene.cpp／.h，新增 16 个 UI／表现文件；其他生产源码、
  存档 schema 和资源保持原样。未刷机，实机交互与性能待验收。

Stick／OSTB 只链接各自 UI 家族，最终源码与头文件依赖均已覆盖；AMOLED 未加入横屏 ExploreScene 或 UI。三份依赖锁文件哈希未变。


2026-10-23 电脑／设置顶部对齐：

- compact_touch 电脑入口及设置（含帮助／重置确认）复用队伍 `safePageHeader`，
  顶部左右内缩 28px、上方 4px、栏高 48px；标题绘制 y=16，返回圆心 (48,24)，
  返回点击区同步为 `safeBack()`。列表绘制、点击、拖动和按键 reveal 使用一致几何，
  首行从 y=52 开始；正文宽度、滑杆范围及游戏规则不变。帮助卡片下移以避开顶部栏。
- 修改前电脑／通讯录 48 帧及设置 38 帧与已有 OSTB 基线完全一致。
  本次有意更新电脑 6 帧、设置 38 帧的 OSTB 像素基线；通讯录及 Stick 基线保留。
  导出图中电脑、设置、帮助、重置确认的返回区域与队伍顶部逐像素一致。
- 电脑／通讯录与设置两家族真实操作及像素回归通过，源隔离 8 项与 OSTB 原生检查通过；
  OSTB 重新配置后固件构建通过，app 1,356,656 B。未刷机，实机观感待确认。


第九批洗澡记录（2026-10-23）：

- 沿用现有横屏骨架完成收口。原 `ShowerScene.cpp／.h` 来自 Git
  `a430cd6c0deaa7ebfa9c9d76ae70284ad98d0731`；其他源码冻结于本次验收开始时。
  基线只由原单体场景生成，未使用新 UI 反向生成期望值；来源及哈希见
  `tools/fixtures/shower_ui_parity.source.json`。
- Stick／OSTB 的 release／debug 各 218 帧，共 872 张 RGB565 画面与旧实现一致。
  覆盖八个模式、五种精灵、菜单循环／缓动、肥皂库存／工具、泡沫层级／遮挡、氛围透明度、
  水滴池边界、跳跃／转身／摇摆、爱心／经验浮字、提示和退出确认。
  `test_shower_ui_parity.py` 正式回归通过，既有像素基线未改动。
- 每配置 35 组真实操作在旧版和新版均通过，包含 IMU／无 IMU 按住 A、长短按与返回、
  空库存／选择后耗尽／重复使用、立即保存、泡沫合并／冲洗、阶段奖励防重、访客限制、
  1／2／3 心恢复、完成后保存、定时唤醒与静止停绘。没有新增洗澡触摸入口。
- 四配置 ASan／UBSan 通过且画面仍与原基线一致。旧版未完成退出的 700ms 到期路径
  独立重现 heap-use-after-free；新版由引擎真实持有、同步销毁场景的同一路径通过。
  严格 host 语法检查通过，沿用项目 GameState 匿名结构扩展，仅豁免该两类既有警告。
- 原八套 UI 像素／操作、六套 feature、BathService／BathInteraction、源选择、架构、
  渲染调度、AppSceneFlow、存档编码及迁移共 22 脚本 42 项通过；AMOLED 独立洗澡的
  拖动／脏区／成长返回／阶段奖励／恢复／保存三项回归通过。
  原生 OSTB 在干净目录实际导出 71 张 PPM，与冻结旧场景逐字节一致；原输出目录另有
  四张历史遗留截图，本次未将其计入有效帧数，也未删除原文件。
- 四目标构建通过，app：Stick 1,419,312 B、OSTB 1,356,656 B、
  AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。编译源、对象和链接记录确认
  Stick／OSTB 仅包含各自家族；AMOLED 未加入横屏 ShowerScene 或 UI。
  三份 dependencies.lock 哈希未变。
- 本轮验收没有再改动已存在的生产拆分源码；补齐独立基线、修正验收脚本的文字比较及
  真实场景创建，完成验证与文档。玩法、存档 schema、资源保持原样，未刷机。
  当前剩余 SocialScene 无线通讯、其余 Debug 子页、共用 ProgressionUi；AMOLED 原竖屏
  控制器继续独立。洗澡实机交互、观感与性能待验收，OSTB 触摸拖动属于后续增强。


第十批无线通讯记录（2026-10-23）：

- 冻结本批开始时的完整源码及源清单，原 `SocialScene.cpp／.h` 与 Git
  `a430cd6c0deaa7ebfa9c9d76ae70284ad98d0731` 一致。独立原场景生成
  `tools/fixtures/social_ui_parity.json`，来源及 SHA-256 见相邻 `.source.json`；
  既有像素基线没有更新。
- Stick／OSTB release／debug 各 210 张完整 RGB565、共 840 帧与原实现一致。
  覆盖十二状态、三项菜单、0～4 房间及全部游标、房间缩减后保留旧游标、房间号边界、
  接受／拒绝、主客提示、错误文案／超长文字，以及等待点 499／500／999／1000／1500ms 边界。
- 每配置 32 组操作在旧版和新版均通过；包含真实协议帧驱动的发现／加入／接受与拒绝、
  房主及访客完整握手与结束／召回、空队伍／非单人队伍、加入／搜索／握手超时、
  输入优先级、取消、生命周期、状态／房间数重绘、停绘及定时唤醒。
  真正由引擎持有场景，验证 A／B 长按和菜单返回同步销毁，以及主客 ACTIVE 切回主房间。
- 四配置 ASan／UBSan 和严格 host 语法检查通过（仅允许既有 GameState 匿名结构扩展）；
  内存检查生成的全部画面仍与原基线一致。两家族的六种代表画面已导出复核。
- 原九套 UI 像素／真实操作、六套 feature、洗澡服务、无线协议与会话、访客进出、架构、
  源选择、渲染调度、应用流程和存档编码／迁移共 27 脚本 50 项回归通过；AMOLED PairFlow 的行为、
  时钟回绕及结束／取消副作用三项通过。OSTB 干净目录导出的 71 张原生画面与冻结旧版一致。
- 四目标构建通过，app：Stick 1,419,952 B、OSTB 1,357,488 B、
  AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。编译源、对象和链接记录确认
  Stick／OSTB 仅包含选中家族，AMOLED 没有加入横屏 SocialScene 或 UI。
  三份依赖锁哈希不变。
- 本批生产修改仅为原 SocialScene.cpp／.h 及新增 11 个 SocialUi 文件。
  VisitSessionService、EspNowLink、玩法、存档 schema 和资源未改；原会话静态测试只调整了
  UI 迁移后应用回调的参数断言。未刷机，真实设备间无线联调、交互和性能待验收。
  当前隔离剩余其余 Debug 子页和共用 ProgressionUi；AMOLED 继续原独立竖屏应用。

## 第十一批：Debug 子页与共用成长表现（2026-10-23）

- 修改前冻结完整工作树源码、源清单及文件哈希于 `.stick-local/artifacts/device-ui-final-20261023/`；
  使用已经包含前十批的实际工作树作为基线，没有用 Git HEAD 覆盖当前菜单代码。
- `DebugUi` 接管分类、列表、滚动、数字弹窗、按键与原底部导航；`MenuScene` 保留
  语义调试动作、页面栈、通知及游戏服务调用。release 编译门控和战斗返回保持。
- `ProgressionUiBase` 接管升级、进化／取消、学招式／替换提示、视觉时钟和输入决策；
  `core/ProgressionUi` 保留原调用接口，只传只读数据、执行游戏结果和叫声效果。
- 独立旧源码生成四配置 764 张完整 RGB565：Stick／OSTB release 各 111，debug 各 271。
  新版逐像素一致；debug 各 68 组、release 各 11 组真实操作通过，覆盖确认／取消、
  无效物种、时间数字边界、通知期限、限制、招式熟练度与保存、进化／取消和叫声时序、
  时钟回绕、资源释放，以及联系人／双精灵／战斗／长按／底部点击的引擎持有场景销毁。
  四配置严格语法及 ASan／UBSan 通过，内存检查下的像素也一致。
- `tools/test_final_ui_parity.py` 与 `final_ui_parity_host.cpp` 作为后续回归入口；
  基线及全部冻结源文件哈希记录于 `tools/fixtures/final_ui_parity*.json`。
  原十套 UI 的 4,172 帧基线未改且回归通过，33 脚本 87 项检查通过；
  包含成长规则、Debug 门控、菜单返回、教程、存档编码／迁移及家族源隔离。
  OSTB 新导出的 71 张原生画面与冻结版一致，两家族抽样预览已检查。
- AMOLED 扩展检查 26 项中 23 项通过，包含成长交互四项。三个既存源码断言失败：
  `test_serial_contact_diagnostics_follow_debug_events`、
  `test_talk_debug_marks_bottom_center_without_labels`、
  `test_debug_subcategory_back_still_returns_to_root`；分别仍期待旧访客日志、旧锚点日志、
  旧的直接 ROOT 赋值。相关 AMOLED 实现与测试相对本轮开始的 SHA-256 均未变，
  未修改这些断言；详见验收目录 `amoled-results.json`。本批不能标记为全部仓库测试通过。
- 四目标加 Stick debug 共五个固件构建通过：Stick release 1,421,360 B、debug 1,437,152 B、
  OSTB 1,358,704 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。
  对象及头文件依赖覆盖最终源码；仅链接选中家族，AMOLED 无横屏 UI 源。
  release 符号审计未链接 Debug UI，debug 正常包含；三份依赖锁不变。
- 本批生产变更仅 `MenuScene.cpp／.h`、`core/ProgressionUi.cpp／.h` 及新增 16 个 UI 文件；
  玩法、存档 schema、资源与 AMOLED 源码未改。未刷机，真实按键／屏幕／叫声／性能仍待设备验收。
- 全场景复核确认：`HatchScene` 的孵化界面、`MenuScene` 公共底色／通知／教程叠层仍待迁移。
  AMOLED 继续独立竖屏应用；本批完成不代表全仓库 UI 已完全隔离。


第十二批孵化与菜单公共叠层记录（2026-10-23）：

- 冻结本批开始的完整工作树源码、源清单和文件哈希，独立旧版基线见
  `tools/fixtures/hatch_ui_parity.json` 及相邻 `.source.json`，验收材料位于
  `.stick-local/artifacts/device-ui-hatch-overlays-20261023/`。原十一套像素 fixture 未改。
- 孵化通过只读 HatchFrame 提供时刻、夜间、进度和剩余秒；HatchUi 管理相机、蛋、
  HUD、700ms 提示、视觉观察、点击区域及语义按键。elapsed／轻拍和擦拭计数、运行时
  恢复、15 秒保存、失败重试、离页保存及完成仍由 HatchScene 调用原接口；
  HatchService 和选种规则不动，保存先于完成检查，同步销毁后立即返回。
- MenuOverlayUi 负责公共底色、通知样式与教程条件／绘制。应用持有通知文本与期限、
  导航及教程完成，保持 STATUS 不叠加通知、TEAM／BAG 自有通知、Stick 电脑页隐藏
  通知等原分支；触屏／探索上下文／未孵化时教程抑制，正文和全屏通知位置不变。
- 四配置共 2,640 张 RGB565 与独立旧源码逐像素一致：每家族 release 648、debug 672，
  包含正常资源及缺失蛋资源两次独立启动；验证完整房间背景、昼夜差异与缺失房间回退，
  不只比较背景回退图。覆盖晃动 45／46／184／230／231ms、倒计时边界、全部菜单页
  通知／叠层及教程条件组合。已检查两家族导出画面。
- 正常／缺失蛋资源两种启动均通过真实操作和调度检查点：Stick release 49／debug 51，
  OSTB release 50／debug 52；覆盖保存失败重试、退出恢复、运行时回退、全部选种、
  700／1100／1400ms 通知与时钟回绕、夜间重绘、四档速度、点击取消／拖动／重复松手，
  以及真实引擎持有场景的完成和返回菜单销毁路径。四配置 ASan／UBSan 和严格语法通过；
  生产源码仅允许原 GameState 匿名结构扩展，测试宏访问的 Clang 警告在局部隔离。
- 原十一套 UI 合计 4,936 帧保持一致；33 个回归脚本 88 项通过，含共享孵化、教程、
  调度、架构、存档编码／迁移和各 feature。干净导出的原生 OSTB 71 帧与本批旧版一致；
  AMOLED 孵化流程／像素脏区／洗澡孵化控制器三项通过。
- 五个固件配置构建通过：Stick release 1,422,224 B、debug 1,438,048 B、
  OSTB 1,359,680 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。
  最终对象和头文件覆盖新源码；Stick／OSTB 仅链接所选家族，release Debug UI 门控有效，
  AMOLED 不引入横屏 UI，三份依赖锁不变。
- 本批生产变更仅 HatchScene.cpp／.h、MenuScene.cpp／.h 及新增 18 个 UI 文件；
  玩法、存档 schema、资源、协议与 AMOLED 源码未改。上一批记录的三个 AMOLED 既存
  源码断言失败仍属未处理边界，不能据此宣称全仓库测试全部通过。
- 横屏场景现已无直接 PixelRenderer／TouchUi／显示尺寸绘制分支。GameEngine 外层
  虚拟导航仍调用 TouchUi，Scene 保留通用轻点路由；RoomRenderer、资源绘制等共用
  表现工具继续原边界，AMOLED 仍为独立竖屏应用，不宣称全仓库 UI 完全隔离。
  未刷机，真实设备观感、输入、性能及跨设备无线仍待验收。


第十三批应用外层记录（2026-10-23）：

- 修改前完整源码、原十二套 fixture、源集与哈希已冻结在
  `.stick-local/artifacts/device-ui-application-20261023/`。独立旧版外层像素基线见
  `tools/fixtures/application_ui_parity.json` 及相邻 `.source.json`；旧 fixture 均未改。
- 外层复核确认底部导航以外还存在资源缺失提示及淡入淡出绘制，因此一并提取到
  所选 ApplicationUi。只读帧传入缺失原因／编号与过场阶段／进度／时长，原按钮样式、
  资源提示固定位置、字体缺失优先级和暗化公式不变。渲染顺序仍为场景→导航→资源
  提示→淡入淡出→提交；GameEngine 仅保留 PixelRenderer::bind 的运行时画布绑定。
- SceneInput 由编译选择家族，header-only 共用策略持有屏幕边界、原 32px 导航区与
  返回／选择／确认映射。Scene 只保留轻点生命周期和事件分发，坐标及虚拟目标编号
  已迁出。保持无触摸家族显式调用 onTouch 的原兼容语义，绘制仍按实际触摸可用性
  抑制。未新增触摸入口，原 TouchTap 的 10px／1000ms 边界和取消规则不动。
- 四配置各 165 帧、合计 660 帧与独立旧源码逐像素一致，覆盖导航开关／输入能力／
  剪裁、全部资源缺失原因及 0／1／25／999／65535 编号、缺失精灵无编号、四个过场
  阶段、1／300／65535ms 时长、进度夹取及组合遮挡。已检查两家族导出画面。
- 每配置 36 个真实输入／过场检查点与旧版一致；覆盖屏幕及三列分界、内容路由、
  取消／拖动／越界／超时／重复松手、时钟回绕、物理键／锁屏／资源警告／过场输入
  拦截、每次八个触摸事件上限、虚拟按钮与普通点击同步销毁、真实通讯录子页返回、
  过场 HOLD 500ms 和逐步更新上限、静止停绘及提交次数。四配置 ASan／UBSan 通过。
- 新增／迁出代码四配置严格语法检查通过。对 GameEngine 全文件启用 -Werror 时，
  release 的三个／debug 的两个 unused 诊断与冻结旧版完全一致：
  DEBUG_LIGHT_SOURCE_COUNT（release）、FULL_FRAME_BYTES、sceneLogName；
  未通过额外屏蔽或无关改动掩盖，也不把整个核心文件描述为零诊断。
- 原十二套 UI 合计 7,576 帧、33 个回归脚本 89 项、干净导出的 OSTB 原生 71 帧、
  Web 模拟器一项及 AMOLED 孵化／控制器三项通过。源码约束新增应用外层检查：
  GameEngine 不再直接布局／绘制，Scene 不含导航坐标／目标编号；UI 不引用引擎。
- 五配置构建通过：Stick release 1,422,368 B、debug 1,438,192 B、OSTB 1,360,112 B、
  AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。最终源码／头文件均被构建覆盖，
  家族源互斥，旧 TouchUi::drawNavigation 符号已移除，release Debug 门控有效。
  AMOLED 无横屏家族 UI，三份依赖锁不变。
- 本批仅修改 GameEngine.cpp、Scene.h、TouchUi.cpp／.h 并新增 12 个 UI 文件；
  原场景、玩法、存档 schema、资源、协议与 AMOLED 源码未改。未刷机。
- 原定横屏场景及应用外层的 UI 隔离代码已经收口。RoomRenderer、PixelRenderer、
  字体／资源等保留为共用表现工具，AMOLED 按原设计使用独立竖屏应用，这些不列为
  横屏迁移欠项。真实设备观感／输入／性能及跨设备无线仍待验收；第十一批记录的
  三项既存 AMOLED 源码断言失败仍未处理，不能宣称全仓库全部测试通过。

OSTB 洗澡页固定触摸工具（2026-10-23）：

- 按用户要求，`compact_touch/ShowerUi` 将右侧肥皂／刷子／冲水／退出改为四个固定
  48x44 按钮，间距 4px、中心纵坐标 48／96／144／192；图标与点击区域同源。
  不再循环滚动或插值，菜单中 B 键只切换高亮。重复点击当前工具不重启阶段。
- 肥皂选择可点击名称切换已有库存，再点 USE 确认消耗；退出沿用原未完成提示与
  默认取消，yes／no 可点击，弹窗阻断工具点击。搓洗继续沿用晃动／无 IMU 长按 A，
  本轮未加入触摸拖动搓洗。
- Scene 仅转发目标／操作；固定布局、命中与模式拦截在所选 UI，轻点生命周期复用
  TouchTap。拖动、超时、取消、按键、退出均取消点击；只在松手确认后执行，重复松手
  不重复消耗。干净退出由引擎同步销毁场景的路径通过 ASan／UBSan。
- 改前 release 两家族各 218 帧与原冻结基线一致。原 `shower_ui_parity.json` 与源记录
  保留，新增 `shower_ui_touch.json`／`.source.json` 记录有意改变的 OSTB release／debug
  各 218 帧；已检查导出图。Stick release／debug 各 218 帧继续匹配原基线。
- 洗澡四配置操作／像素、OSTB release／debug ASan／UBSan、两家族严格语法与家族
  源约束通过；另 21 个相关 UI／feature／原生 OSTB／洗澡规则／调度／架构脚本通过。
  OSTB、Stick release、AMOLED V1 lite 构建通过。玩法奖励、存档、资源
  与 AMOLED 独立 UI 不变，未刷机，触摸手感待实机确认。


OSTB 洗澡拖动与技能阅读调整（2026-10-23，接续固定按钮版）：

- 洗澡移除 HOLD A。肥皂／刷子选中后，在精灵区域按住拖动即可擦洗，工具跟随手指；
  按住不动不计擦洗，松手、移出操作区、取消、按键、切换工具或场景后立即停止。
  右侧四按钮、肥皂选择与退出确认沿用上一版，不增加道具消耗或改变阶段奖励。
  Stick 原晃动／无 IMU 按 A 输入保持，OSTB 不再由 A 或 IMU 驱动擦洗。
- 触摸追踪、工具位置和输入强度只在 compact_touch/ShowerUi；场景继续使用原阶段
  冷却、BathInteraction、BathService 与保存路径。没有触摸动作或视觉反馈的工具页
  停绘；泡沫下落、水滴、浮字及完成反馈继续按需更新。点击按钮期间的更新不会取消点击。
- 技能保持原 90px 左列及右侧属性／熟练度，点击区从 24px 改为 44px，技能间空出 6px。
  左侧移除返回项，顶部返回仍支持取消遗忘窗／从替换页返回回忆列表；B 只在技能中循环。
  首次点未选中条目只查看，再点当前条目执行，末行也遵守这一规则；遗忘默认取消。
- 描述复用共用 UTF-8 换行工具，按右栏宽度显示多行，停止自动横向跑马灯。超出描述
  窗口才允许上下拖动，并显示位置条；属性和左列不随描述移动。手势起点决定区域，
  横跨左列或顶部不会触发技能操作，切换技能重置阅读位置，静态阅读页停绘。
- 改前 OSTB 洗澡 218 帧、队伍／状态／技能 36 帧与已有 fixture 一致，相关源码和 fixture
  冻结于 .stick-local/artifacts/ostb-drag-moves-20261023/source。已检查新旧导出图：洗澡原 218 帧中
  有意改变 90 帧，新增 2 张真实拖动画面；队伍组仅 8 张技能画面改变，另增最长描述图。
  新期望另存 shower_ui_drag.json／party_ui_touch.json 及来源记录，旧 fixture 不覆盖；
  Stick 洗澡 release／debug 各 218 帧、队伍组 36 帧继续匹配原基线。
- 洗澡四配置真实操作／调度、OSTB release／debug ASan／UBSan，技能真实操作及
  ASan／UBSan、原生 OSTB 与两家族严格语法／家族源检查通过；覆盖拖动生泡／刷泡、
  静止／松手／取消停止、按钮按下期间更新、无额外消耗、弹窗阻断、技能间隙、默认 No、
  回忆与顶部返回、描述手势不触发操作，以及干净退出时真实引擎同步销毁场景。
  当前真实技能库的最长描述可一屏展示，超长内容的滚动上限按换行后的实际高度计算。
- OSTB、Stick release、AMOLED V1 lite 构建通过。玩法、存档 schema、资源、协议和
  AMOLED 独立 UI 保持，未刷机，触摸手感仍待实机确认。

本轮收尾验证：23 个相关回归脚本（含洗澡／队伍像素旅程、原生 OSTB、其他家族 UI、
feature、洗澡规则、调度、架构及家族源约束）通过；另做了像素范围复核，洗澡 188 张
非运动输入帧仅 HOLD A 区域改变，技能页顶部导航像素保持。已有全仓库其他失败记录
仍按前文保留，本轮不宣称全仓库所有测试通过。

OSTB 肥皂弹窗与食物顶部导航（2026-10-23）：

- 洗澡点肥皂打开横向三卡弹窗，参考微雪展示三种皂的图标、名称和实时库存。
  松手点选有库存卡片直接沿原 beginSoap 路径消耗一份、立即保存并进入拖动擦洗；
  去掉原循环名称和 USE 二次确认。无库存项置灰且不可选，右上关闭、点弹窗外或
  B 长按取消均不消耗；弹窗拦截底层工具，拖动／超时／取消／按键不转成误选。
- ShowerFrame 仅增加三种库存只读数组，库存查询仍由场景提供；界面不读写引擎。
  原 SOAP_SELECT 物理键选择／确认及 Stick 界面保持，玩法、奖励和存档格式不变。
- 食物页统一复用队伍 safePageHeader/safeBack：顶部左右 28px、上方 4px，
  返回圆心 (48,24)、导航栏 48px。列表从 y=52 开始，保持全宽和 44px 行高；
  绘制、点击、触摸滚动、物理键 reveal 与滚动上限共用同一布局，旧圆角位置不命中返回。
- 改前源码、fixture 和 OSTB 导出帧冻结于 .stick-local/artifacts/ostb-soap-food-20261023/source；
  原 OSTB 洗澡 220 帧、房间／食物 19 帧均先匹配已有期望。新布局单独记录新 fixture，
  保留原 Stick 及历史像素基线。验证和实机状态以本节后续收尾记录为准。
- 收尾验证：23 个相关回归脚本通过，洗澡两家族 release／debug 共 880 帧通过；
  Stick 洗澡每配置 218 帧、房间／食物 26 帧继续匹配原期望。OSTB 仅改变 14 张
  SOAP_SELECT 画面并新增 2 张缺货弹窗，房间组仅改变 13 张食物页画面，房间菜单不变。
  新期望另存 shower_ui_picker.json／room_ui_food_header.json 及来源记录，旧 fixture 未改。
- OSTB 洗澡 release／debug 与食物真实旅程 ASan／UBSan、四配置严格语法、原生全像素
  顶部安全区／点击检查通过；食物返回区域与队伍页像素完全一致，未滚动正文逐像素
  仅下移 10px。已检查导出预览及圆角遮罩。覆盖三种皂直接消耗及存档、缺货、库存变化、
  关闭／外部／B 取消、重复松手、拖动／超时／锁屏取消与弹窗隔离，食物末项滚动可见。
- OSTB release／debug、Stick release、AMOLED V1 lite 构建通过；debug 刷入脚本预演通过，
  未实际刷入设备，弹窗触感和圆角观感待实机验收。既有其他失败记录仍保留。

OSTB 探索地区列表点击（2026-10-23）：

- 仅在 compact_touch 家族接入地区选择页左侧条目点击，保留原 70px 左列、34px 行距、
  缓动与预览；点其他地区切换预览，再点当前地区走原 DEPART 流程，返回项直接返回。
  未解锁地区可查看原提示，出发仍经原引擎门控；不复制解锁、地图或存档规则。
- ExploreRouteLayout 提供原绘制与命中共用的列宽、行距、可见范围和行中心计算；
  点击区高 30px，相邻留 4px，按实际缓动位置命中，仅可点击真正绘出的条目。
  松手确认，拖动、超时、取消、物理键、离页不触发。Scene 仅委托目标和激活；
  内嵌菜单及行走、战斗、演出等其他阶段不接收地区触摸，Stick 输入保持原样。
- 改前完整源码、host 与历史 fixture 冻结于 .stick-local/artifacts/ostb-explore-touch-20261023/source，
  OSTB release 321 帧先匹配原期望。本批不重排界面，不更新任何像素 fixture；新增
  真实输入旅程覆盖六地图切换、同项出发、锁定门控、返回、行距、缓动、边界、重复松手、
  取消／拖动／超时／按键／离页、阶段隔离及真实引擎出发／返回的同步销毁。
- OSTB release／debug ASan／UBSan 通过，每配置 321／322 帧匹配原像素。
  两家族新 UI／共用绘制四配置严格语法通过；ExploreScene 全文件 -Werror 的既有
  有符号比较诊断与冻结旧源一致，未通过屏蔽告警或修改无关逃跑逻辑掩盖。
  OSTB release／debug、Stick release、AMOLED V1 lite 构建及 debug 刷入预演通过，
  尚未实际刷机；其他页面回归结果在收尾后记录。
- 收尾 21 个相关回归脚本通过；探索两家族 release／debug 共 1,286 帧保持原像素，
  新触摸真实旅程全部通过，其余菜单／家族界面／feature／原生 OSTB／架构／调度保持通过。
  旧 fixture、资源、存档格式、玩法规则和无线协议未改，尚未刷入设备。


探索内嵌菜单触摸修复（2026-10-23）：

- 根因：ExploreScene 内嵌 MenuScene 的绘制、更新和实体按键已有委托，但原始触摸
  没有转发；探索地区的 touchTarget 又有意拦截子菜单，所以探索中进入的队伍、状态、
  技能和背包均无法收到手势。此前只验证地区点击和子页实体按键，遗漏了组合路径。
- 现将子页原始触摸和导航能力交给 MenuScene，并在取消／物理输入／离页时同步清理
  子页触摸状态。开关子页取消未结束手势，地区命中仍被拦截，不让触摸穿透到地图。
  Scene 不添加坐标或设备分支，列表、描述滑动和弹窗交互仍属于各 UI 家族。
- 提取按键原有的子页输入收尾，供按键和触摸共同处理关闭、战斗药物结果和投食。
  继续使用原物品服务、投食演出与反击流程，避免只扣库存却停留在子页或不结算回合。
- 新增真实行走菜单→队伍→技能的原始触摸旅程；改前先在队伍卡片点击处复现失败。
  修复后覆盖技能选择、遗忘 No／Yes、弹窗隔离、返回栈、状态滑动、取消／按键／离页、
  关闭重进，以及行走用药、战斗用药／投食、重复松手不重复消耗或结算。
  改前相关文件及复现日志保存在 .stick-local/artifacts/ostb-explore-subview-touch-20261023/before
  和 repro.log；后续检查与构建日志位于同级目录。无像素 fixture 更新，未刷机。
- 收尾验证：20 个相关回归脚本通过（11 组其余 UI／原生检查、6 组 feature／flow、
  家族源约束、架构边界、渲染调度）。探索两家族 release／debug 四配置真实旅程及
  1,286 帧匹配原 fixture；OSTB release／debug ASan／UBSan 通过。未改像素基线。
- OSTB release／debug、Stick release、AMOLED V1 lite 构建通过；debug 刷入脚本预演
  通过，未连接串口或实际刷机。app 分别为 1,363,728／1,379,232／1,423,056／
  1,403,744 B，哈希见 firmware.json。实机点击手感待验收，既有其他失败记录仍保留。


OSTB 房间状态栏与菜单（2026-10-23）：

- 按最新用户要求，电池参照微雪相对文字的比例缩为 18×8（原 36×16），时间面板
  收为 (190,12,78,22)。沿用微雪电池轮廓、绿／红／未知电量规则；共享绘制器增加
  显式像素倍率，默认仍为原 36×16，其他调用的绘制不变。
- 左上角状态统一在 (28,12,68,H) 面板内纵向排列：一只 H=22，两只 H=42，
  两行相隔 20px，饱腹图与 HP 条保持原尺寸和队伍顺序。第二行覆盖房间左上方
  一小部分；本次用户要求取代此前“两只状态仅占顶部留白、横向排列”的限定。
  房间本身仍为 (28,39,240,161)、1 倍精灵与原锚点，不移动或缩放玩法画面。
- 房间菜单改为微雪式圆角三横线图标，矩形 (216,202,36,32)，中心从原 (255,221)
  向内移到 (234,218)，绘制和命中共用同一矩形。旧右侧区域不再打开菜单；松手、
  取消、拖动与切场景规则保持，输入仍经 HomeUi／主场景原服务。
- 菜单坐标和图标位于 compact_touch 家族，未在主场景加入设备布局。按当前尚未
  实测的 R56 圆角模型做预览，菜单已绘制像素满足 6px 内缩；圆角实际观感仍待实机。
- 改前相关源、host 和旧 fixture 冻结于 .stick-local/artifacts/ostb-home-hud-20261023/before，
  OSTB release／debug 共 373 帧先匹配旧期望。新画面只改变其中 171／172 帧，
  每个差异像素均核对位于状态栏、时间电池或菜单区域（含小房间模拟布局）。
  检查了单双精灵、昼夜、低／满／未知电量、提示、演出和圆角预览。
  新期望另存 home_ui_hud.json／来源记录，保留 home_ui_parity.json 原基线。
- 收尾验证：房间两家族 release／debug 共 746 帧及各 24 个共享操作检查点通过；
  Stick 两配置保持旧像素，OSTB 两配置匹配独立新期望。OSTB 两配置 ASan／UBSan、
  改动绘制代码四配置严格语法、原生房间与新旧菜单位置命中检查通过；预览见
  room-full-1.png／room-full-2.png，均来自真实渲染并叠加暂定圆角遮罩。
- 扩展回归 19 脚本中 18 通过。当前工作区探索像素一项未通过，检查期间其他工作
  修改了 ExploreMapGenerator／ExploreTileSemantics／ExploreRouteUiContent。
  用冻结源仅替换本轮三个绘制文件，探索四配置 1,286 帧及语义与改前完全相同，
  且匹配原 fixture；不能把当前整棵工作树的探索像素项标为通过，也未更新其 fixture。
- OSTB release／debug、Stick release、AMOLED V1 lite 构建与 debug 刷入预演通过，
  未实际刷机。额外尝试 V2 lite 被正在拆分的 ExploreScreens.inc:332 缺少
  drawExploreRouteMapForegroundLayer 声明阻断；将本轮电池头文件换回冻结原版做
  同一翻译单元语法检查仍出现相同错误，已记录 v2-before-renderer-syntax.log。
  未修改这项其他工作，不能宣称本轮四固件全部通过。


OSTB Debug 页面触摸适配（2026-10-24）：

- compact_touch 的 Debug 根页及六个分类统一复用队伍 safePageHeader/safeBack：
  顶部左右 28px、上方 4px、48px 栏高；顶部返回依次关闭数字窗、返回 Debug 根页、
  返回主菜单。MenuScene 只委托导航能力，Debug 页去掉底部返回／选择／确认三键。
- 各级列表隐藏末尾“返回”项，正文保持全宽，条目为 44px 高、6px 间距；支持松手直接
  点击及上下拖动，实体 B 仅循环可见条目并自动露出选中项。手动滚动不会被旧自动居中
  拉回，静态页面停绘；拖动／超时／取消／按键／离页不触发条目，间隙和旧底部区域不命中。
- 切换精灵／设置时间改用独立数字点击区与确认／取消按钮，顶部返回可取消，弹窗阻断
  底层列表。仍调用 DebugUiBase 原数字约束和语义动作；公共状态值绘制查询提取为共用
  helper，Stick 画面／按键流程及 release Debug 门控保持。
- 改前完整源码与原 fixture 冻结于 .stick-local/artifacts/ostb-debug-ui-20261024/before，OSTB debug
  原 271 帧先匹配旧基线。审查真实导出图后，新期望另存 final_ui_debug_touch.json 及来源：
  160 张 Debug 画面有意改变，111 张成长画面保持，新增 2 张滚动／末项图；原 fixture 未改。
- 两家族 release／debug 共 766 帧匹配各自期望，release 各 11、debug 各 68 个原检查点
  及新增触摸断言通过；覆盖弹窗数字／确认／取消、时间约束、返回栈、无返回行、滚动保持、
  实体键 reveal、间隙／旧底部／取消／超时不误触与触摸随机对战时同步销毁场景。
  OSTB debug ASan／UBSan、两家族四配置改动代码严格语法通过。
- 扩展回归 20 个脚本中 19 个通过（其余 UI、六组 feature／flow、原生 OSTB、家族源、
  架构和渲染调度）。探索像素回归四配置仍不匹配既有 fixture；用本轮冻结旧源仅替换
  五个 Debug／菜单文件，两家族 debug 各 322 帧在改前／改后完全一致，均有同样 6 张
  既有差异，见 explore-isolation.json。未覆盖探索 fixture，也不宣称全仓库测试通过。
- OSTB release／debug、Stick release／debug、AMOLED V1 lite 构建通过，Debug 刷入脚本
  --dry-run 预演通过；app 分别为 OSTB 1,386,384／1,402,400 B、Stick 1,445,744／
  1,461,616 B、AMOLED V1 1,426,304 B，哈希见 firmware.json。未访问串口或实际刷机，
  触摸手感及实机圆角观感仍待验收；预览见同目录 preview.png。


OSTB 探索选择页上下滑切换（2026-10-24）：

- 左侧 70px 列表增加松手竖滑：上滑下一项、下滑上一项，24px 起算，每次一项，
  首尾停止。沿可见地区、可见锁定地区、原返回项顺序切换，不进入隐藏地区；滑到返回
  仅选中，滑动不会出发或退出。切换继续调用原 AREA_CHANGED 预览与缓动路径。
- 可从左列条目间隙开始；右侧预览、离开左列、横向拖动、超时、取消、实体键、离页
  和非 SELECT 阶段不切换选项。原点击查看／再次点击出发与点击返回保持。
  TouchTap 和手势状态仅位于 compact_touch，ExploreScene 只委托原始触摸及取消；
  内嵌菜单继续接收自身手势，Stick 不携带滑动状态。激活前清理手势，支持同步销毁。
- 改前完整 src、host、fixture 及失败复现位于 .stick-local/artifacts/ostb-explore-swipe-20261024。
  新增上／下滑、长距离单步、首尾、返回不执行、可见锁定项、间隙、跨列、折返、重复
  松手、超时／取消／按键／离页／阶段切换断言；改前首先在上滑切下一项处复现失败。
- 两家族 release／debug 四配置真实旅程各 36 个原检查点及新增断言通过，1,286 帧与
  改前相同；release 的 321 张共同画面与冻结 debug 一致，debug 各 322 帧直接比对。
  OSTB release／debug ASan／UBSan、两家族四配置 UI 严格语法通过。旧探索 fixture 每个
  配置同样有 6 张已有地图差异；本轮没有更新像素 fixture，详见 validation.json。
- 其他 20 个回归脚本中 18 个通过。房间像素旅程与原生 OSTB 房间边界检查未通过：
  本轮开始前菜单已由用户改为 (236,192,36,32)，两项仍按旧点击位置／黑色房间边缘断言。
  HomeUi.cpp 与本轮冻结源字节一致；冻结旧源分别复现相同断言，见 home-existing-failure.json
  和 native-before.log。未改回用户按钮位置，也未为本轮滑动修改房间测试或 fixture。
- OSTB release／debug、Stick release、AMOLED V1 lite 构建及 Debug --dry-run 通过；
  app 分别为 1,387,008／1,402,992／1,445,712／1,426,464 B，哈希见 firmware.json。
  未实际刷机，真实触摸手感待确认；不宣称整仓库所有回归通过。


房间菜单新位置的测试预期更新（2026-10-24）：

- 用户明确要求同步当前 (236,192,36,32) 菜单位置的两项检查。房间点击旅程改点
  (254,208)，验证旧位置 (234,218) 不命中、新右缘 (271,218) 命中及两侧外缘不命中；
  取消后不激活、再次点击由真实引擎销毁房间并进入菜单的路径继续覆盖。
- 原生房间外围黑色检查仅豁免菜单覆盖矩形，矩形外仍要求全黑；同步更新原生房间
  点击位置和边缘断言。仅修改测试及期望，src 全部文件哈希与本轮起点一致。
- 已逐像素核对旧／新截图：OSTB release 186 帧中 138 张变化、debug 187 帧中
  139 张变化；全部差异均在旧 (216,202,36,32) 与新菜单矩形的并集内，其余各 48 张
  完全一致。审查预览后，新期望另存 home_ui_menu_position.json／来源记录；
  home_ui_parity.json、home_ui_hud.json 和原来源记录字节不变。
- test_home_ui_parity.py 两家族 release／debug 共 746 帧及各 24 个检查点通过，
  test_ostb_native.py 通过，OSTB debug ASan／UBSan 及 187 帧对比通过。
  证据与预览见 .stick-local/artifacts/ostb-home-menu-checks-20261024；未构建或刷入固件。
  本节修正此前探索滑动批次中记录的两项房间失败，其他既有地图 fixture 差异不在本次范围。


OSTB 探索选择页跟手拖动（2026-10-24，取代上版松手单步切换）：

- 原先 MOVE 不改变画面，只在 UP 后切换一项，所以拖动不跟手。现在左侧 70px 列表
  识别竖向拖动后按实际位移 1:1 更新原 areaAnimCursor；10px 内保留点击容差，拖动
  从按下时的插值位置开始。松手按最近行提交地区并沿原缓动吸附，可一次跨多项、首尾
  钳制，返回映射仍跳过隐藏地区；拖动始终不会出发或执行返回。
- 按住期间暂停自动居中，保持 1.5 秒后仍可继续拖动；停止移动且无预览动画时可停绘。
  右侧预览在松手提交后才切换，选项预览时钟继续走共享 helper。取消／跨列／横向
  手势／按键／离页／阶段切换不提交，并恢复按下时位置；中途按住原吸附动画后取消
  或超时松手会重新安排吸附，避免卡在半行。原点击和同步销毁场景路径保留。
- 手势状态与位移仅位于 compact_touch，ExploreUiBase 的游标更新改为可重写，并提取
  原预览时钟 helper；Stick 使用原更新路径，ExploreScene 无新增坐标或手势规则。
  与本轮冻结源码比较，src 仅四个 ExploreUi／ExploreUiBase 文件变化，其他运行源码
  字节不变，见 source-differences.json；用户手调的房间菜单位置保持。
- 改前失败复现、完整冻结源、导出图和结果保存在
  .stick-local/artifacts/ostb-explore-drag-20261024。真实帧验证上移 12px 时标记也上移
  12px、右侧像素不变，覆盖长按、折返、跨多项、首尾、取消、吸附中再按下与超时恢复；
  drag-preview.png 展示按下／拖动／吸附三个状态。正常点击、内嵌菜单、战斗道具和
  引擎同步销毁的旧断言继续通过。
- 两家族 release／debug 各 36 个原旅程检查点及新增触摸断言通过，原 1,286 帧与本轮
  冻结源一致；OSTB release／debug ASan／UBSan 和四配置 UI 严格语法通过。
  20 个相关回归脚本全部通过（含已更新的房间／原生边界检查），详见 regressions.json。
  原探索 fixture 每配置仍有相同 6 张既有霜晶窟地图差异；未更新该 fixture，不能据此
  宣称整仓库所有基线通过，详见 validation.json。
- OSTB release／debug、Stick release、AMOLED V1 lite 及 Debug --dry-run 构建通过，
  app 分别 1,387,360／1,403,360／1,445,760／1,426,464 B；哈希见 firmware.json。
  未访问串口、未刷机，真实设备触摸手感待验收。


OSTB 探索精灵活动接入（2026-10-24）：

- compact_touch 的 ExploreUi 持有共享 ExploreEcology／ExploreGrass 模拟，复用微雪已有
  栖息地、性格、活动、家庭组合与独立随机源，未复制或调整行为算法。六个地区最多
  出现三只环境精灵，包含巡游／观察／睡眠／躲藏、结伴／重聚、飞行及水中活动。
  环境活动不额外触发战斗、奖励或存档变更，原遭遇与稀有显形规则保持。
- ExploreRouteOverlay 是可选家族表现接口，共用路线内容仅委托草丛底层、环境精灵与
  角色遮挡；空接口时 Stick 沿用原路径。演员按共享地面深度排序，水中精灵位于桥面
  下，飞行精灵越过低矮遮挡；树木、草丛与原生向上梯继续使用共享语义。两个向上梯
  图块仍共用下格中心深度。影子复用 PokemonShadow::atGround，并纳入遮挡边界。
- 草丛绘制使用原素材，接触、拨开和回弹由共享模拟计算，地面与角色覆盖采样相同
  形变；领队／随行与环境精灵仅在落地时产生接触。素材于进图预热，环境精灵绘制
  只查驻留帧，不在绘制时解码。预载活跃池及家庭额外成员，保留可容纳的原地区预览
  固定缓存，避免切入活动后释放其他地区已加载的战斗素材。
- ExploreScene 仅新增语义生命周期委托：新地块重置、原遭遇后显形，以及菜单／内嵌
  页面／战斗／拾取／落穴等待时暂停。暂停时冻结模拟，静态页恢复首帧重置时钟，下帧正常推进；
  地图切换清理旧生态与草丛状态。没有新增设备坐标、触摸规则或状态变更入口。
- 改前 src、主机旅程、原 fixture 与微雪参考源码冻结于
  .stick-local/artifacts/ostb-explore-ecology-20261024/before。
  新 tools/explore_ecology_ui_host.cpp 使用真实引擎／场景／素材，覆盖六地区共享模拟
  初始姿态、家庭资源驻留、隐藏与遭遇后显形、五类暂停／恢复、换图重置、重复绘制
  不推进活动、不额外解码、GameState／地图指纹／玩法随机调用不变。
  桥面与两格向上梯另有独立精确像素断言；树木／桥面／高草／梯子的地面／飞行、
  前后位置及草丛接触／回弹形成 38 张专用画面。release／debug 均通过，期望存于
  explore_ecology_ui.json，由 test_explore_ecology_ui.py 持续检查。
- 原探索四配置各 36 个真实旅程检查点通过，共 1,286 帧；Stick release／debug
  共 642 帧与本轮冻结源一致。OSTB 各 94 张路线／提示背景有意增加活动与遮挡，
  所有战斗画面不变；focus-review-0..4.png 与 journey-review-0..4.png 已逐张审查。
  原 explore_ui_parity.json 字节保留，活动叠层另存 explore_ui_ecology.json／来源记录，
  每个 OSTB 配置仅新增 88 张期望，明确排除原有 6 张霜晶窟地图差异。正式像素回归
  仍逐配置报告这 6 张既有差异，没有将地图旧差异归入本功能或覆盖旧基线。
- OSTB release／debug 的专用场景及原旅程共四组 ASan／UBSan 通过，像素与普通构建
  相同；两家族四配置 UI 严格语法通过，ExploreScene 全文件诊断与冻结源相同。
  20 个扩展回归脚本全部通过，含房间新菜单坐标、原生 OSTB、家族互斥、架构与渲染
  调度；共享生态／草丛测试通过。详情见 final-regressions.json、sanitizers.json、
  journeys.json、scene-diagnostics.json 和 source-audit.json，不宣称全仓库基线通过。
- OSTB release／debug、Stick release、AMOLED V1 lite 及 Debug --dry-run 通过；实际
  编译清单确认新生态表现只进入 OSTB、未加入 Stick 家族。app 分别为
  1,420,032／1,435,744／1,446,032／1,426,464 B，哈希见 firmware.json。
  未访问串口、未刷机；实机密度、加载时间、帧率及遮挡观感仍待验收。


OSTB 探索弹窗、地图与战斗点击（2026-10-24）：

- 特殊遭遇、拾取、结束与结交确认不再使用旧 240×135 绝对坐标；由家族显式启用
  居中布局，结果／结束文字按实际宽度居中，结束奖励文字保持在框内。Stick 默认布局
  不变；AMOLED 保持独立控制器。
- 战斗命令在左右各 28px 内缩范围中分成四列，文字 x=42／102／162／222，选择箭头
  同步移动。点击区域使用同一布局，每列间隔 4px；点一下直接调用原战斗、背包、换人、
  逃跑动作，继续使用既有回合、物品与日志流程。
- 地图轻点调用原 WALK／beginAutoWalk，沿原路线行进；滑冰和落穴等待时不响应。
  ExploreScene 仅新增只读 routeBlocked 标记。侧菜单条目可点，地图外侧不穿透；特殊
  遭遇／结交选择、拾取／结果／结束弹窗可点，外部点击不触发行进。
- 触摸追踪、命中和临时上下文均在 compact_touch：与地区跟手拖动分开，按下与松手
  必须属于同阶段／菜单／忙碌上下文。拖动、超时、取消、实体按键、离页均不激活；
  战斗日志、HP／EXP、投食、换人演出期间不接受命令。分发前清理手势，调试结果／
  战败触摸返回允许同步销毁场景而不再访问旧对象。
- 改前源码、host、fixture 和清单冻结于
  .stick-local/artifacts/ostb-explore-route-battle-touch-20261024/before；四配置共 1,286 帧：
  Stick 643 帧与冻结源一致，OSTB release／debug 仅 135／136 张有意变化。六张对照图
  全部审查，普通战斗差异只在 y>=204 页脚；弹窗中心与命中另有断言和 R56 遮罩预览。
  新期望另存 explore_ui_touch.json／来源记录，原 explore_ui_parity.json 与精灵活动
  期望均保持。每配置 6 张原霜晶窟地图差异仍可见，未靠覆盖旧基线消除。
- 四配置各 36 个原检查点与新增真实触摸旅程通过，覆盖地图连续点击、七种取消／阻断、
  菜单／弹窗隔离、四命令执行、十种战斗忙碌与阶段变化、子页返回和引擎持有场景的
  同步销毁。OSTB 两配置 ASan／UBSan、四配置 UI 严格语法通过；场景严格检查与冻结
  版本诊断一致。38 帧生态两配置与 20 个相关 UI／feature／原生／架构／调度脚本通过。
- OSTB release／debug、Stick release、AMOLED V1／V2 lite 与 Debug --dry-run 构建通过；
  app 为 1,422,256／1,437,792／1,446,496／1,426,464／1,748,032 B，哈希见 firmware.json。
  未连接串口、未刷入设备，实际圆角与触摸手感待验收。


收尾并行改动补验：首次验证后，工作区另有地图／素材修改（含 ExploreMapGenerator、
ExploreTileSemantics、ExploreRouteGeometry、ExploreEcology、GameAssets，以及场景的
interiorPortalExit 调用），均保留，未纳入本次 UI 修复。最新四配置各 36 个原检查点与
新增地图／战斗／弹窗触摸断言继续通过，新增画面差异仅为 area-5 的六帧；对旧期望现为
每配置 area-3／area-5 共 12 张地图差异，所有本次弹窗／页脚期望仍匹配。
精灵活动专用回归在该组合下 release 出现画面差异，debug 还在原测试的 tile 1627
绘制断言失败；原先本次 UI 修复后的 38 帧两配置已通过，未为这批并行改动更新生态基线。
最新组合 OSTB release／debug、Stick release、AMOLED V1／V2 lite 与 Debug --dry-run
构建通过，app 分别 1,427,664／1,443,264／1,451,920／1,431,968／1,753,520 B。
证据见本任务目录 final-checks.json、final-test-explore-ecology-ui.log、final-firmware.json；
本次 UI 修改单独保存在 owned-after，源码时点见 final-source-manifest.json。未刷机。


OSTB 探索地图 HP／状态与紧凑菜单（2026-10-24）：

- 顶部左侧面板 (36,12,88,22/42)，真实队伍前两位纵向排列、行距 20px；HP 52×6，
  采用微雪绿／黄／红分段，濒死灰色，零最大 HP 和超出最大值均安全处理。异常状态使用
  原 12px 素材，正常不显示图标；首位濒死仍保留其行，不按当前行走领队重排。
- 地图名面板 (196,12,72,22)，文字居中，右侧留 28px。右下菜单完全复用房间
  (236,192,36,32) 按钮与三横线图标；公共几何／绘制位于 compact_touch/HudControls.h，
  房间原图和点击旅程未变。探索菜单 (188,36,80,152)，四行 36px、4px 点击间隙，
  高度随条目数确定；菜单与按钮不重叠，保留队伍／背包／结束／返回。
- ExploreScene 只提供 teamHud（HP、异常状态、濒死）和 menuAvailable；家族通过可选
  ExploreRouteOverlay::drawHud 接管路线顶部与菜单，Stick 沿旧路径。新按钮只在允许
  行走时显示和命中，滑冰／落穴等待屏蔽。沿用 routeTap 的松手确认、取消／超时／拖动／
  按键／阶段隔离，打开菜单不触发行进，菜单外／间隙不穿透地图；关闭沿原路径补偿移动时钟。
  原地图、精灵活动、遭遇、战斗、存档、资源和 AMOLED 实现均未修改。
- 修改前完整源码、host、fixture、构建清单冻结于
  .stick-local/artifacts/ostb-explore-hud-menu-20261024/before；本次源码另存 owned-after，
  source-manifest.json 记录时点。四配置各 36 个原检查点通过；新增原始触摸检查覆盖菜单
  松手／重复松手、八种中断、四个条目、子页返回、外部／间隙隔离和移动暂停恢复。
- Stick release／debug 各 321／322 帧与冻结源一致；OSTB 各 332／333 帧中，原各 94 张
  变化全部限定在新旧 HUD／地图名／菜单区域，原战斗／地区选择不变；新增各 11 张覆盖
  单只／双只、七种状态、HP 阈值／超最大／零最大、濒死首位及触摸菜单。8 张导出对照图、
  新状态图和 R56 近似圆角遮罩预览均已审查，见 review-*.png／preview.png／pixel-review.json。
- 原像素 fixture 字节保留，新期望另存 explore_ui_hud.json（93 张／配置）和
  explore_ecology_ui_hud.json（35 张），来源记录为 explore_ui_hud.source.json；明确排除
  已有 area-3／area-5 各 6 张路线截图及 3 张古瀑谷生态截图。当前实际流程断言均通过，
  整项探索像素测试仍报告上述旧地图差异，不能描述全仓库通过。生态两配置各 38 帧仅 HUD
  区域变化，详见 validation.json 与最终测试日志。
- 20 个其他 UI／feature／原生／架构／调度脚本通过，房间 746 帧保持现有期望；
  OSTB release／debug ASan／UBSan 和像素一致性通过；四配置 UI 严格语法通过，
  ExploreScene 全文件原有诊断与冻结源相同。详见 regressions.json、sanitizers.json、
  strict.json、scene-strict.json，新增源清单断言也已通过。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过；
  app 分别 1,428,976／1,444,640／1,452,064／1,432,896 B，哈希见 firmware.json。
  未访问串口、未刷机；R56 仍是近似遮罩，实机圆角与触摸观感待验收。


OSTB 战斗底部面板与我方精灵对齐（2026-10-24）：

- 用户要求底部面板增高 10px。compact_touch 明确传入 footerExtraHeight=10：面板
  (0,194,296,46)，四命令文字 y=209、16px 行高，在面板纵向居中；同一布局将命中范围
  同步扩到 y=194～239。战败返回提示和两行日志也按新高度居中。
- 我方名字／等级 y=161，HP／EXP y=180，异常图标同步上移 10px；投食起点随 HUD
  上移，目标坐标不变。我方精灵图地面基线直接等于 footerY=194，图像最下行在面板
  之前，受击／换人保持这条基线。敌方布局及精灵缩放规则不变。
- 只修改 ExploreBattleUiModel／ExploreBattleUiContent 与 compact_touch 的布局参数；
  Stick 保留原默认值。改前完整源码、host、fixture、构建清单冻结在
  .stick-local/artifacts/ostb-battle-footer-20261024/before，本次文件另存 owned-after。
  四配置各 36 个原旅程检查点通过，新增断言验证四命令的新面板顶边可点击、上一行
  不命中并可直接执行，覆盖攻击／背包／换人／逃跑。
- 两家族 release／debug 各 321／322 和 332／333 帧：Stick 643 帧相对改前完全一致；
  OSTB 各 174／175 张变化全部属于战斗／结交／debug 战斗，路线与选择画面不变。
  攻击、状态、HP、EXP、投食、日志、换人及结交的代表图与 R56 近似遮罩对照已审查，
  见 preview.png、review-*.png、pixel-review.json。新期望另存
  explore_ui_battle_footer.json／来源记录，之前所有 fixture 字节保留；四配置仍有
  同样 12 张霜晶窟／古瀑谷既有地图差异，不描述探索像素整项通过。
- OSTB release／debug ASan／UBSan 及像素一致性通过；两家族四配置 UI 严格语法、
  test_ui_family_sources／test_architecture_boundaries／test_render_demand 通过。
  详见 checks.json、sanitizers.json、validation.json 与最终探索测试日志。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过；
  app 分别 1,429,008／1,444,672／1,452,096／1,432,896 B，哈希见 firmware.json。
  未访问串口、未刷机，实机圆角与触摸观感待验收。


OSTB 探索精灵脚底与道路中心对齐（2026-10-24）：

- 青草径／溪桥坡／雾林径路线点本已位于双格道路中心，旧绘制却把图片中心放在路线点，
  脚底和影子因而偏向道路下沿。本次参考微雪 drawExploreRouteActor，按素材
  PokemonSprites::frameGroundOffsetY 对齐脚底；领队、随行与 Boss 采用相同锚点，
  飞行精灵仍在地面锚点上保留原悬浮偏移。高草园／霜晶窟／古瀑谷原已使用此锚点。
- compact_touch 明确传入 feetOnRouteCenter=true，共享 ExploreRouteUiContent 只消费
  布局参数；Stick 默认 false。生产源码只改 ExploreRouteUiModel.h、ExploreRouteUiContent.cpp
  和 compact_touch/ExploreUi.cpp；地图生成、路线坐标、移动、碰撞、生态模拟、战斗、
  资源、存档和 AMOLED 实现均未改，未将设备判断放入场景或玩法。
- 改前完整 src、构建清单和 host 冻结于
  .stick-local/artifacts/ostb-explore-route-anchor-20261024/before；当前完整源码哈希见
  source-manifest.json，本轮文件另存 owned-after。新增左右方向截图使用实际
  ExploreRouteGeometry::pathPoint，并验证双格道路中心线坐标。
- 两家族 release／debug 各 36 个原旅程检查点通过，最终共 1,316 帧；Stick 原 643 帧
  与冻结源一致，新增四张左右方向截图。OSTB 原各 51 张路线／弹窗背景有意改变，
  新增各两张左右方向截图；战斗及其余地区保持。全部变化接触表、代表对照和生态对照
  已审查，见 review-0～4.png、preview.png、ecology-review.png 与 pixel-review.json。
- 生态两配置各 38 帧、六地区十二只环境精灵的真实活动／暂停／显形／草丛／遮挡断言
  通过；每配置仅九张地区 0／1／4 的领队或 Boss 脚底变化，地区 2／3／5 完全一致。
  新期望分别保存到 explore_ui_route_anchor.json 和 explore_ecology_ui_route_anchor.json，
  来源记录为 explore_ui_route_anchor.source.json，所有原 fixture 保持字节不变。
  路线每配置原有 12 张霜晶窟／古瀑谷地图差异、生态每配置三张古瀑谷差异仍报告，
  不宣称像素整项或全仓库全部通过；具体名单见 validation.json。
- OSTB release／debug ASan／UBSan 通过，334／335 帧与普通构建完全一致；两家族
  四配置改动 UI 严格语法、家族源／架构边界／渲染调度检查通过，见 checks.json。
- OSTB release／debug、Stick release、AMOLED V1 lite 和 Debug --dry-run 构建通过；
  app 分别为 1,428,992／1,444,640／1,452,096／1,432,880 B，哈希见 firmware.json。
  未访问串口、未刷机，实机行走与桥面观感待验收。


OSTB 房间底部对话框及学习招式居中／点击（2026-10-25）：

- 房间对话原本固定在 Stick 的底边 131px，OSTB 改由 HomeUi 提供 x=28、宽 240、
  底边 y=232；普通提示高 28px、带选择高 45px。对话显示时隐藏菜单按钮，防止右下
  按钮遮字。HomeUiBase 默认位置保持原样，RoomRenderer、世界坐标和场景规则未改。
- 学习提示 (28,46,240,148)、遗忘／放弃确认 (28,52,240,136)、替换完成
  (28,56,240,128) 在实际 296×240 屏幕居中；是／否高 44px。遗忘选择列表仍为原
  90px 左列，条目高 44px、50px 行距，右侧描述换行，永久基本招式不可点击遗忘。
  compact_touch 显式提供布局选项，通用绘制只消费参数，Stick 保留原画面和输入。
- compact_touch/ProgressionUi 持有 TouchTap 和待学习对象／招式／弹窗状态快照；
  点学习提示是／否、遗忘条目、确认是／否，均返回原 MoveLearnAction。core 适配器
  统一调用原 GameEngine 学习／放弃／替换操作，未复制成长业务；MainScene／ExploreScene
  只协调 LEARN_MOVE、MOVE_REPLACED 的委托和后续流程。替换结果可点继续，先清除
  触摸再确认原替换事件。实体按键、离页、锁屏取消仍有效，不新增持续刷新。
- 改前完整 src、构建清单、host 与所有 fixture 冻结在
  .stick-local/artifacts/ostb-home-dialog-move-learn-20261024/before。改前两家族 debug
  原图首先匹配旧期望；release 按自己的旧期望核对，避免把 debug 边界／文字当作变化。
  四配置房间 746 帧、各 24 原检查点；成长 766 帧、release 各 11／debug 各 68 原
  检查点通过。Stick 共 755 帧原像素不变，OSTB 房间各 28 张／成长各 74 张有意改变。
- 新真实点击断言覆盖房间和探索两入口：空位学习、拒绝不改精灵、不可遗忘基本招式、
  行间隙、两种可替换槽、确认默认否／取消／替换、放弃新招式、替换事件继续、外部隔离、
  重复松手、拖动折返、取消、超时、实体键、离页及待学习招式变化。事件仍通过原引擎
  重置对应熟练度和排队后续提示；取消学习不改变原精灵。
- preview.png 与 review-home-0～2、review-final-0～6 接触表已审查。新期望独立保存为
  home_ui_dialogue_and_learn.json／final_ui_move_learn_touch.json，来源为
  home_ui_dialogue_and_learn.source.json；所有原 fixture 字节未改。详情见 validation.json、
  pixel-review.json、source-manifest.json 和 owned-after。未扩大到旧探索地图像素差异。
- OSTB debug ASan／UBSan 及 273 帧哈希一致性通过；四配置改动 UI／适配器严格语法、
  家族源 9 项、架构 13 项、渲染调度 2 项、原生 OSTB 渲染／电源／输入／存档检查通过。
  OSTB release／debug、Stick release、AMOLED V1 lite 和 Debug --dry-run 构建通过；
  app 分别为 1,432,032／1,447,760／1,454,224／1,432,880 B，哈希见 firmware.json。
  未访问串口、未刷机，实机圆角与点击观感待验收。


OSTB 探索精灵与精灵球落地点下移（2026-10-25）：

- 用户要求左右行进时稍向下，落在距道路底部约 1/3 的位置。compact_touch 显式传
  routeGroundBias=1/6，共享绘制按道路高度向下偏移；双格道路 52px 下移约 8.67px，
  单格窄路 26px 下移约 4.33px。所有方向使用同一偏移，避免转弯时画面跳动。
- 沿用素材 frameGroundOffsetY 脚底锚点，领队、随行、Boss、影子、遮挡和深度排序
  共用调整后的视觉地面。精灵球移除原独立的偏下落点，影子对齐相同地面；球体保留
  原来相对影子的高度与美术。环境精灵自身位置和生态状态不变。
- 生产源码仅改 ExploreRouteUiModel.h、ExploreRouteUiContent.cpp 和 compact_touch/ExploreUi.cpp。
  Stick 默认偏移为零，地图生成、路线／相机坐标、行进／碰撞／拾取规则、生态模拟、
  战斗、资源、存档和 AMOLED 未改。改前完整源码、host、fixture 与清单冻结于
  .stick-local/artifacts/ostb-route-lower-third-20261025/before，当前源码见 source-manifest.json。
- 两家族四配置各 36 个原旅程检查点通过，路线共 1,316 帧。Stick release／debug 共
  647 帧与冻结源一致；OSTB 各 107 张路线背景有意改变，战斗／选择／成长画面不变。
  生态 release／debug 各 38 帧、六地区十二只环境精灵活动／暂停／显形／草丛／遮挡
  断言通过，38 帧因队伍或精灵球落点改变。preview.png、review-route-0～6、
  review-eco-0～2 及 review-prior-progression.png 已审查。
- 审查后独立添加 explore_ui_lower_third.json 和 explore_ecology_ui_lower_third.json，来源为
  explore_ui_lower_third.source.json。路线叠层每配置 104 帧，其中 95 帧属于本批，另九帧
  是上一批学习招式点击／居中修改在探索入口的画面，已单独审查，本批改前／改后不变。
  生态叠层 35 帧；全部旧 fixture 字节保留。路线每配置 12 张霜晶窟／古瀑谷、生态
  每配置三张古瀑谷既有地图差异仍可见，不描述探索像素整项或全仓库全部通过。
- OSTB debug ASan／UBSan 与 335 帧一致性、四配置改动 UI 严格语法、家族源／架构边界／
  渲染调度检查通过，见 checks.json。实际旅程与像素记录见 validation.json，相关页面
  回归日志及结果见 test-*.log 和 regressions.json。
- OSTB release／debug、Stick release、AMOLED V1 lite 及 Debug --dry-run 构建通过，app
  分别 1,432,288／1,448,016／1,454,464／1,432,880 B，哈希见 firmware.json。未访问串口、
  未刷机，实机道路／桥面观感待验收。本批修改另存同目录 owned-after。
- 收尾相关页面／feature／原生回归 17 个脚本通过；探索实际脚本四配置均仅列出相同
  12 张既有地图差异，生态两配置仅保留上述三张旧地图差异；未覆盖这些基线。


OSTB 探索菜单返回后行进及微雪战斗特效迁移（2026-10-25）：

- 触摸菜单“返回”此前只关闭菜单，静止状态需要再点地图。本次由 compact_touch 返回
  独立 MENU_RETURN_WALK 语义，协调器先关闭菜单，再调用原自动行进；若原单步尚未
  完成，仅恢复自动标记，不重新起步或重复计步。菜单暂停时间仍补入插值时钟。
  队伍／背包子页回到侧菜单的导航保持，Stick 实体按键原返回行为保持。
- 参考微雪 HomeScreen.cpp 与 app/BattleTimeline.cpp，compact_touch 显式启用 moveEffects：
  攻击按六段 80ms 前冲／回撤，成功命中时抖动，步骤 2～5 播放四帧素材；共用
  GameAssets::battleFxForMove／battleFxKind，涵盖撞击／火／水／电／抓／咬及瞪眼／
  叫声／泼沙九类。按 296/368 缩放移动与特效，落点取实际绘制边界，HUD／页脚后绘制。
  命中资源缺失仍有圆环反馈；不改素材包、背景、精灵图大小及已确认的底部面板位置。
- ExploreBattleFrame 只增加招式 ID 与“对方能力确实变化”的只读结果。零伤害状态动作
  记录视觉起点，复用原日志期间的刷新；不延迟原 commit，不另算成功率，不增 RNG，
  不改伤害／HP／回合／日志时序。失败、未命中、无效或能力已到底的状态招式不播；
  混乱自伤只在自身受击，不向对方前冲。普通无状态静止战斗仍停绘。
- 生产只改六个 UI／协调器文件，未改微雪实现、玩法、存档、资源、地图和生态生成。
  另一个窗口正在修改地图，因此完整 before 源码／host／fixture 冻结后，isolated-after
  仅叠入本批六个生产文件；原始与隔离源清单及并行改动名单见 source-scope.json、
  source-manifest.json、owned.json。并行 GameAssets.cpp 与 ExploreValleyData.h 修改保留。
- 冻结旧源使用“返回后应行进”的新断言可重现失败，见 repro-menu.json。四配置各
  36 原检查点及新增真实返回／重复松手／取消／拖动／超时／暂停单步恢复断言通过。
  新特效检查通过真实 prepare／commit 路径，覆盖双方九类各四帧、36 素材非空、重复
  绘制不变更状态、特效结束清除、忙碌不接受命令及重复 apply 不重复结算；状态招式
  成功／能力封底／未命中／失败／无效结果也有检查。
- Stick release／debug 共 647 帧相对冻结源完全一致；OSTB 原各 18 张受击动画有意变化，
  各新增 72 张关键帧，其余地图／选择／战斗静止／成长不变。preview.png 与 review-0～5
  已审查，新期望独立保存 explore_ui_battle_fx.json 及来源，全部旧 fixture 保持字节不变。
  每配置 12 张既有霜晶窟／古瀑谷地图差异继续报告，不覆盖地图基线。
- 冻结生态 host 改前已输出六地区十三只环境精灵；隔离后 release／debug 各 38 帧与改前
  完全一致，活动／暂停／草丛／遮挡断言通过。实际 test_explore_ecology_ui.py 仍因
  runner 预期 actors=12 而失败，此处保留并行地图／生态适配差异，未据此改回生成器。
- OSTB debug ASan／UBSan 407 帧与普通版一致，四配置改动 UI 严格语法、家族源／架构
  边界／渲染调度及相关页面／feature／原生 17 个回归脚本通过。实际探索旧地图差异与
  生态输出计数失败保留，不宣称全仓库全部通过，详见 validation.json 和最终探索日志。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过；
  app 分别 1,519,808／1,535,488／1,540,544／1,518,496 B，哈希和构建时点见 firmware.json。
  这些构建包含当时工作区的地图改动；本批专属源码另存 owned-after。未访问串口、
  未刷机，实机特效流畅度与观感待验收。全部证据集中于
  .stick-local/artifacts/ostb-explore-resume-battle-fx-20261025。


OSTB 探索菜单“返回”保持暂停（2026-10-25，替代上一节返回即行进要求）：

- 按用户最新要求，compact_touch 改发 MENU_RETURN_PAUSE：关闭菜单、取消自动行进，
  同时冻结未完成的领队／随行单步。需要新的有效地图点击才继续，不重复计步或跳步；
  取消／重复松手不恢复，反复打开菜单不会重复补暂停时长。Stick 原按键行为保持。
- 仅四个生产文件变化：ExploreScene.cpp/.h、ExploreUiModel.h、compact_touch/ExploreUi.cpp。
  上一批战斗特效与当前地图生成改动保留；无新像素基线。新增 host 断言覆盖静止／
  自动行进／单步中返回、等待两秒不位移／不计步、随行暂停、重复菜单返回、取消触摸、
  点击恢复插值与恢复后实际移动。
- 两家族四配置各 36 原旅程及新增断言通过，原 1,460 帧与本批冻结源逐帧一致；
  OSTB debug ASan／UBSan 407 帧与普通版一致。四配置 UI 严格语法、家族源／架构／
  渲染调度检查及其他 17 个相关回归脚本通过。实际探索脚本仍报告每配置 12 张
  area-3／area-5 旧地图差异；生态 runner 仍预期 12 而 host 输出 13，未覆盖旧期望。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过，
  未访问串口、未刷机。冻结源、隔离源码、像素对照、构建哈希与完整验证集中于
  .stick-local/artifacts/ostb-menu-return-pause-20261025，见 validation.json 与 firmware.json。


OSTB 伙伴页顶部导航对齐队伍（2026-10-25）：

- compact_touch 使用同一 safePageHeader／safeBack：标题“伙伴”，返回圆心 (48,24)，
  顶部左右 28px、上方 4px、栏高 48px。touchNavigation=false，移除底部三按钮的绘制
  与点击语义，底部改为列表正文。原列表和操作内容保留，起点 y=52；各弹窗下移
  48px 避开顶部，绘制／点击／自动显示游标使用同一布局参数。顶部返回逐层关闭
  确认窗、操作窗，再返回电脑入口，重复松手不穿透。
- 移除原选择按钮后，长列表增加 TouchScroll 直接拖动：移动 1:1、松手保留位置，
  不误开条目；操作窗期间不滚动底层。实体键切项恢复自动显示游标，触摸取消／
  按键／离页清理手势。纯家族交互，共享 ContactsUiContent 只接受布局参数，默认
  保留 Stick 绘制；伙伴规则、存档、地图、资源与 AMOLED 实现未改。
- 两家族 96 帧／各 14 原检查点通过；新增顶部返回、确认窗逐层取消、底部原按钮
  区域不触发导航、拖动不激活、松手不回弹、20 条时末项可点击及实体键 reveal 断言。
  Stick 48 帧与 OSTB 六张电脑画面字节不变，OSTB 伙伴 42 帧有意改变；review.png 已
  审查，独立添加 contacts_ui_header.json／来源记录，原 contacts_ui_parity.json 不变。
- OSTB ASan／UBSan 48 帧与普通版一致；四配置 UI 严格语法、家族源／架构／渲染调度
  检查及最终 17 个相关回归通过。原生测试同步把伙伴页旧底栏预期改为顶部返回，
  新增安全栏绘制／命中检查。最初原生停绘断言及旧导航断言日志保留，最终原生通过。
  旧探索每配置 12 张地图差异、生态数量 13／预期 12 的已有失败仍保留。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 编译通过，
  未访问串口、未刷机。完整源码冻结、截图、检查日志、最终回归结果与固件哈希见
  .stick-local/artifacts/ostb-contacts-header-20261025／validation.json、regressions-final.json
  和 firmware.json。


OSTB 通讯录参考微雪改版（2026-10-25，替代上一节保留旧列表／操作内容的说明）：

- compact_touch 独立绘制微雪式头像列表：72px 行高、56px 头像画布、名字／羁绊、队伍旗、
  做客／不可邀标签、右箭头与分隔线；顶部显示容量并继续使用队伍安全导航栏。
  列表末尾和操作弹窗均无“返回”项，仅顶部返回逐层关闭确认／操作窗，再回电脑入口。
- 操作窗按共享能力显示状态／邀请／删除，宽132px、每项40px，点外部关闭不穿透。
  邀请和删除采用居中带名字的确认框及44px按钮，删除默认取消。列表继续1:1拖动、
  松手保留滚动；实体键显示当前游标，空列表不会产生隐藏返回项或除零。
- 仅 ContactsUiBase.cpp/.h 和 compact_touch/ContactsUi.cpp/.h 四个生产文件变化。
  shared base 仅增加返回项可见性策略，默认保留 Stick 旧行为；联系人能力、邀请／删除、
  实时队伍映射、保存与到达演出继续走原服务，无玩法／存档／资源变更，AMOLED 未改。
- 两家族各48帧、各14原检查点及新增空列表／末项／弹窗关闭／实际删除／实际邀请同步
  切场景检查通过。Stick48帧、OSTB六张电脑画面不变，伙伴42帧有意变化；review.png 和
  review-states.png 已审查，新增独立 contacts_ui_amoled_style.json 及来源，旧两份基线不变。
  host 在启动完成后启用动态头像资源，匹配真实引擎启动行为，未修改生产资源加载逻辑。
- OSTB ASan／UBSan48帧、只叠入四文件的隔离源码对照、四配置严格语法、家族源／
  架构／渲染调度检查及17项相关回归通过。探索脚本仍报告每配置12张旧地图差异；
  生态仍输出13只而runner预期12，未覆盖这些基线。
- OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 最终构建通过。
  首次构建遇到并行探索消耗源码错误及微雪构建目录生成冲突，原日志保留；其他窗口
  修正后重试通过，未代改探索文件。未访问串口、未刷机，实机观感待验收。源码快照、
  范围清单、验证、构建哈希和完整日志见
  .stick-local/artifacts/ostb-contacts-amoled-style-20261025。
