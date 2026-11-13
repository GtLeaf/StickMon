2026-11-13 周期集市：用户确认每3～4个游戏日开放一天，当前按3/4间隔交替（玩家第1/4/8/11天），不新增日历页，仅入口今日/倒计时。奇珍小铺8位、全解锁16件：优惠3+3、糖果1、六选营养剂共2、六选洗点树果共3、特殊恢复2、稀有轮换1、盲盒1；心鳞不固定，耿鬼首次招呼每档送1，满包/总写入失败待领取。糖果移出日常购买，盲盒糖果额外不占固定库存。17道具追加，培养战斗外/恢复可手动战斗；家族背包/出售已接。SAVE9/schema8追加39B，保留raw8冻结2480B前缀及schema1..7。SaveManager任意有效副本落盘算成功，图鉴失败保持重试，避免回滚已提交交易。主机全流程/迁移/部分写入故障及独立审查通过，闭市摊主对话释放修复。未构建固件/刷机，缺工具链/设备；规格specs/market-stalls/periodic-market.md，证据.stick-local/artifacts/market-season-20261113/，详见verification.md含既有测试限制。

2026-11-13 变隐龙神秘盲盒＋滚动货架（主机验证，未刷机）：保留三件普通商品，新增
第4行盲盒300金币／游戏日1盒，购买成功直接发放完整奖励；25/25/15/15/12/6/2权重对应
甜甜蜜3／高级伤药3／黄金喷雾2／活力碎片1／心鳞1／已解锁进化石1／糖果1，锁定档位
和糖果限额满回退甜甜蜜3，满栈拒绝、不截断。首次建货架混合游戏日＋真实领队身份保存
抽签，买前不公开结果，扣款／奖励／额度同次保存，失败回滚且不重抽。MarketShop保留
3槽存档数组、原预留2字节改boxBought/boxRoll，raw version8布局不变，codec schema7在
care后追加2字节，旧schema默认0；旧当期只补抽签，不恢复商品。四行72px裁剪y216..440，
滚动0..64、捕获拖动至UP、返回保留位置，取消／锁屏不误选。新增48px图标Kind末尾追加，
清理原图低alpha杂点后主体40×39px（旧预览24px过小），旧84帧／每字库1234字逐项保持，
只追加盲盒组3字。规则／完整App ASan/UBSan、编码与迁移、相关耿鬼／吉利蛋旅程、架构13／
家族10（2跳过）通过；资源23项及新图标字形2项通过，原Outside/Caves源缺失的2项不能跑。
修正旧codec测试复用失败encode写出长度0导致越界、更新schema3测试扩展长度；审查领队
选择问题RED→GREEN已修复。已看货架／盲盒确认／失败／结果，未构建固件或刷机。
规格specs/market-stalls/kecleon-ui.md，证据.stick-local/artifacts/market-mystery-box-20261113/。

2026-11-13 变隐龙圆角面板对齐（主机验证，未刷机）：商品／确认／结果页统一y208、
x4／宽360／高236／圆角14，和耿鬼相同；商品三行仍y216／72px。确认页图标／名称／
正文下移到面板内，持有y346、金币y368，错误占名称下方y252；结果图标／文案相应下移，
底部按钮保持。面板外返回边界同步至208，跨边缘拖动仍不触发；镜头按新面板顶定位，
不改刷新或动画时长。几何对照先复现168≠208再通过；变隐龙规则ASan/UBSan及完整App
购买／回滚／返回／离开旅程通过。已看首页、确认、保存失败、成功、金币不足五页，
无文字／按钮遮挡；未构建固件或刷机。证据.stick-local/artifacts/kecleon-rounded-panel-20261113/。

2026-11-13 变隐龙铺名徽章（主机验证，未刷机）：奇珍小铺移到左上角，复用耿鬼
28px浅米圆角徽章（x23／y16，随原hudOffset进出）；首页面板去铺名与×及旧关闭热区，
面板和商品行位置保持，点击面板外仍按原动画离开。徽章绘制提取到MarketStallMotion
共用，耿鬼1171张实际导出帧逐字节相同。变隐龙规则／真实App旅程（含旧×区域无操作、
购买与存档失败回滚、返回／离开）、耿鬼规则／完整旅程、motion ASan/UBSan、架构13／
家族10（2跳过）通过。外部Essentials源复核仍因文件缺失未执行，使用提交的招式数据。
已看新首页，未构建固件、未访问设备。证据.stick-local/artifacts/kecleon-title-badge-20261113/。

2026-11-13 摊位过渡优化（主机验证，未刷机）：用户要求保留当前刷新限制；未改33ms
限流、V1/V2主循环或传屏。三摊面板／镜头／金币徽章统一smoothstep，招呼360ms／展开
420ms／关闭520ms与终点不变；关闭原曲线相同。MarketView保留原地图缓存，超出缓存
的可见行仅补边缘地面，不再因camera-window整图回退；385空边回退、动态帧、遮挡、
昼夜保留，无新增全屏缓存。三视口／两字节序／日夜／八帧／原始空白动态边缘逐像素、
内存复用与分配失败、ASan/UBSan通过；三摊规则与真实App旅程、架构13／家族10（2跳过）
通过，独立审查无生产缺陷。20ms应用回放仍主要40ms出帧，最大展开位移变隐龙40→23、
耿鬼30→17、吉利蛋40→23px，三摊展开全命中缓存；非真机帧率。导出图已检查，工具链
仍缺，未做固件构建或刷机。规格specs/market-stalls/panel-motion.md，证据
.stick-local/artifacts/stall-motion-fix-20261113/。

2026-11-13 三类学习正式接入（主机验证，未刷机）：用户确认远程／近身／自身学习原型。
MarketTutorPerformance::learningStyle复用既有特训分类（包括对敌STATUS与402／479例外），
新增LEARN_REMOTE／LEARN_MELEE／LEARN_SELF，替换旧通用IMITATE；5600ms、学习中、老师紫色／
所选伙伴青色、示范→初试→纠正→成功，底部无招式名，轮流出手且相向。保存／回滚、400ms
禁跳、取消／暂停、结果离开不变；三套4800ms特训与回忆不变。规则ASan/UBSan、完整应用、
11特训档位、学习合法候选三类／对敌变化／自身四体型、完整56帧与一次付款／退出、吉利蛋、
架构13／家族10（2跳过）通过；独立审查无生产缺陷。合法学习测试用85十万伏特，当前486
电球表项不标战斗支持，不改玩法。无资源／schema变化，工具链仍缺，未构建固件或刷机。
证据及新动画.stick-local/artifacts/gengar-learning-live-20261113/，规格见gengar-ui.md。

2026-11-13 三类特训正式接入（主机验证，未刷机）：用户确认远程／近身／自身原型。
MarketTutorPerformance::trainingStyle按所选招式：STATUS全归自身（包括对敌控制），伤害
CONTACT归近身，其余远程；402种子炸弹／479击落只在表现层覆盖为远程，不改战斗flags。
远程中性青光、第二靶缩小；近身前冲打实心靶后退回；自身光点聚集与稳定光环。均4800ms，
学习原5600ms示范模仿、保存／回滚、400ms禁跳、暂停／取消、结果离开不变。无资源／schema
或数值变化。规则ASan/UBSan、完整AmoledApp旅程、11档位、公开UI三类／对敌变化／两个例外、
自身四体型、吉利蛋、架构13／家族10（2跳过）通过；独立审查无生产缺陷。预览与对照证据
.stick-local/artifacts/gengar-types-live-20261113/。云环境工具链仍缺，未构建固件、未刷机。

2026-11-13 学习／特训动画正式接入（主机验证，未刷机）：用户选学习技能=示范模仿、
特训=练靶，招式类型后续拆。新增纯绘制MarketTutorPerformance.h；Ui保存成功后分别进入
TEACHING5600ms／TRAINING4800ms，失败回原确认／选招页，回忆仍直接结果。前400ms禁跳、
之后完整轻点跳结果，锁屏／熄屏暂停与取消手势、不重复扣费，结果离开保持。学习标题
“学习中...”，实际选择伙伴相向；问号上移y252避大体型头部，底部无招式名。学习示范／
首次没做好／再示范／成功；特训两次蓄力命中靶环，统一通用特效不按招式属性分支。
耿鬼规则ASan/UBSan、真实应用完整学习／特训及保存／暂停／回忆末项、11档位、学习
400ms／取消手势／跳过、4×48帧特训全程与底部空白、吉利蛋、架构13／家族10（2跳过）
通过；初始朝向像素＋后续固定方向元数据与画面检查，不称全程方向像素证明。独立审查
无阻挡问题。新文字复用现有字形，无资源／schema／数值变更。工具链仍缺，未做固件构建
或设备访问。实际流程动画与证据.stick-local/artifacts/gengar-integrated-20261113/。

2026-11-13 特训面对面修正（主机验证，未刷机）：特训演出不再用ICON_0，改用市集
walkingAnimation方向帧与flipX，左耿鬼朝右、右伙伴朝左；150ms切帧，frameGroundOffsetY
地面锚定，最大80px／2倍缩放。删演出底部招式名，其余保存／计时／输入不变。四种伙伴
（皮卡丘／伊布／卡比兽／暴鲤龙）各24帧共96帧方向像素与底部空白检查、耿鬼完整规则／
应用旅程／11档位、架构13、家族10（2跳过）通过，导出图已看；脚底位置另经视觉检查。
独立审查无生产问题。工具链缺失，未重新做固件构建或刷机，无资源变化。
证据.stick-local/artifacts/gengar-facing-20261113/，规格specs/market-stalls/gengar-ui.md。

2026-11-13 耿鬼交互更新（主机验证，未刷机）：本期秘技教授确认标题／正文与三类结果
逐行居中；结果按钮“离开”执行520ms收起并结束交谈，不回服务首页。特训选招浅绿选中，
默认第一可用槽，底部返回／动态价格特训；取消独立确认，保存成功才播放原面板2400ms
耿鬼＋伙伴轻跳／紫光／星光，前400ms禁跳、之后轻点可跳，锁屏／熄屏暂停且不重复结算。
结果跨60／90分别提示熟练／精通，否则“用得更熟练了”，不显示数值；错误提示改占标题区。
V1/V2触摸驱动回放复现无后续中断时120ms伪造UP；改按住期间持续读取传感器，成功空接触
才松手，瞬时I/O失败保留手势，避免慢拖变点击。真实控制器持续读取与触摸仍待实机；持续
I/O失败保持按压／禁止浅睡至有效样本或应用取消，故障恢复未扩展。两驱动回放、真实
AmoledApp旅程、11组费用／熟练档位与结果像素、耿鬼规则ASan/UBSan、吉利蛋／变隐龙、
存档迁移、架构13及家族源10（2跳过）通过；独立SaveCodec主机SIGBUS在归档HEAD同样复现，
未改存档代码。当前云环境缺少ESP-IDF／PlatformIO，V1/V2 lite Debug与Stick构建尝试受阻，
不能称固件构建通过。无资源／schema变更，未访问串口。证据与新图集
.stick-local/artifacts/gengar-update-20261113/；规格specs/market-stalls/gengar-ui.md。

2026-11-13 微雪耿鬼子页统一（已构建，未刷机）：摊名徽章左移5px；全部服务页
固定y208圆角面板，切页不移动镜头。三类选人统一156×96横排卡／80px图标、无精灵文字。
忘记页仅两特殊槽，标题固定，名称＋威力（变化显示--）；教授确认删除熟练／金币变化。
回忆去顶部名字／持有，特训选招固定简题、熟练变化与单次费用，确认及结果同步适配。
改前主机11招可滚动，暂未复现实机滑不动；本次捕获列表拖动直至松手，真实旅程验证
顶底限位／越界／锁屏／返回位置、画面移动10451像素，末项387实际学习并读档一致。
规则ASan/UBSan、真实AmoledApp旅程、架构13／家族源10、V1/V2 lite Debug与StickS3
构建通过；已检查导出页，新文字均复用现有字形，无资源变更。证据
.stick-local/artifacts/gengar-compact-pages-20261113/；实体触摸仍待验收，未访问串口。

2026-11-13 微雪耿鬼摊位布局（已构建，未刷机）：左上秘技小屋／右上金币统一浅米色
圆角徽章，局部28px字高；首页去标题行和关闭按钮，顶边168→208，面板圆角14px。
本期秘技两只精灵横排，无名字／状态文字，不可学淡化且禁选，单只居中；招呼取消
计时自动展开，只完成点击才继续，首页点面板外退出。授课／特训／回忆及保存回滚不变。
规则ASan/UBSan、真实AmoledApp完整旅程（含一分钟等待、锁屏取消、单只／禁选、
边缘拖动）、架构13／家族源10通过，已检查原生导出画面；V1/V2 lite Debug及
StickS3构建通过。证据.stick-local/artifacts/gengar-layout-20261113/，未访问串口或刷机。

2026-11-11 工作区分批提交前检查：StickS3、微雪V1/V2 lite Release、谷仓16MiB Release构建通过；
存档codec／迁移、三个集市摊位真实旅程、构建配置／日志、音频续播、渲染调度、架构／家族、
外置内容／资源事务／安装器／发布进程、V2传屏／显示、古瀑谷5项与真实签名包校验通过。
AMOLED原生渲染83项中78项通过，出门过场／水桥／饥饿伙伴及两项配对基线共5项失败；
在提交前HEAD 11d6b77导出的独立源码和资源上复测，同5项均失败，不覆盖测试基线。
本轮仅主机检查与构建，未访问串口或刷机。证据.stick-local/artifacts/workspace-commits-20261111/。
根build-debug属编译缓存，新增/build-*/忽略；清除11月6日遗留且无进程占用的空index.lock。

2026-11-10 微雪古瀑谷切图底图错位（已修复并构建，未刷机）：古瀑谷所有子图共用探险
种子，而微雪底图缓存按种子／地区等参数匹配；换图只重置镜头／精灵／遮挡，未失效
底图缓存。旧代码真实森林A→B复现75672像素与新底图不同。AmoledApp收到mapChanged时
清除底图缓存有效标记，复用原内存，下帧按新地图重建；保留原冰裂失效路径。
两组种子、白天／夜晚、森林回环和高桥共44次真实切图，与冷缓存逐像素相同；
1.5倍／夜色、霜晶窟、传屏流水线及源检查通过，V1/V2 lite Release构建通过。
证据.stick-local/artifacts/amoled-valley-map-offset-20261110/；未修改地图、资源或存档。

2026-11-09 谷仓默认改为16MiB／9MiB资源（sdgoods-16m-single-v1），与微雪规划表几何一致；
微雪现有固件不迁移。物理32MiB检查、可选32MiB／24MiB资源保留。新包不接受15MiB，
安装器允许精确旧15MiB迁移。Release构建、23安装／3进程／13架构、2312旧事务中断点、
49表／28坏输入、实际LittleFS启动目录／签名／16MiB整包校验通过。无USB串口，未刷机。
产物.stick-local/releases/sdgoods/16m-20261109/，证据.stick-local/artifacts/sdgoods-16m-20261109/。

2026-11-08 谷仓发布包目录修正：旧15m/32m整包启动 Invalid content catalog，串口及Release host复现。
根因为教学生成器从原始280招式快照纳入已删除的7个PP/携带物技能（168/180/343/373/374/450/512）。
import_tutor_moves.py复用正式过滤策略，1895→1796条；CMake补MoveTutorData.inc依赖。
package对实际LittleFS解包后运行validate_sdgoods_catalog.py（真实ContentValidation.cpp），
test_sdgoods_room先过有效目录再测坏例；教学全引用及未知ID注入回归已覆盖。
默认15MiB/8MiB资源，FULL_STORAGE=ON选择partitions-32m.csv、32MiB/24MiB资源；
flash.sh --full-storage显式选完整表，缓存隔离布局，package/install/factory按精确表选择偏移和边界。
旧sdgoods-factory-{15m,32m}-20261108.bin停止发布，修正版名带catalog-fix；证据
.stick-local/artifacts/sdgoods-invalid-catalog-20261108/，实机验收见该目录verification.json。
最终发布文件集中到.stick-local/releases/sdgoods/catalog-fix-20261108/{15m,32m}/；两种
布局均定点安装、写后校验和30秒ready=1观察通过（保留NVS，未整包覆盖NVS测试），当前设备32m。

# StickMon 项目记忆

2026-11-08 谷仓孵化页去掉进度条：仅删除round_touch/HatchUi.cpp的条底与填充绘制，
保留顶部倒计时。onboard-fresh真实孵化旅程（点击／摇晃／锁屏／保存）与谷仓Release
构建通过，导出画面已检查；未刷机。证据.stick-local/artifacts/sdgoods-hatch-countdown-20261108/。

2026-11-08 微雪吉利蛋暖心小屋（已构建，未刷机）：点吉利蛋气泡→招呼→首页（亲密按摩／
伙伴茶会／关系评估，右上×或点面板外关闭）。按摩150：正式队员亲密+10／心情+10（封顶
255／100）；茶会200：队员或仓库本地伙伴羁绊+5（封顶100），已入队则同步伙伴记录；评估
免费，给建议并可直接预约。每游戏日全摊2次、每只1次（两服务共用，ivPacked＋metAt绑定）。
补充规则：未邀请过的伙伴（羁绊仍为10且未同行）首次邀请必定成功，茶会会破坏该保底，
故禁用并提示“先邀请它吧”。确认即saveState，失败回滚；成功后面板缩回底部对话框播放
头像轻跳＋爱心/茶杯演出，点击跳过，奖励已提交不重复。规则src/game/MarketCareService.h，
界面src/ui/common/MarketCareUi.h，存档SAVE_VERSION 8／schema 6追加MarketCareState（编码21B），
旧档默认空；save_schema1_fabricator与market_tutor_host伪造旧档同步扣除21B。字库zh16/zh32
1222→1234，谷仓round-zh16同步重生成（原本已落后于前两摊位），安装须带资源。
tools/test_market_care.py规则ASan/UBSan＋真实AmoledApp旅程通过；耿鬼／变隐龙／存档／
架构／家族源回归通过；test_market_scenes微雪第179行既有失败未变。V1/V2 lite Debug与谷仓
构建通过（谷仓新目录借用已有littlefs venv），StickS3因沙箱不能写~/.platformio未构建。
V2 app SHA256前缀3d437f1b8b65，固件.stick-local/firmware/amoled-v{1,2}/market-chansey-care-debug-20261107/，
规格specs/market-stalls/chansey-ui.md，证据.stick-local/artifacts/market-chansey-care-20261107/。

2026-11-07 Release配置统一（已构建，未刷机／发布）：用户确认市集不开放，日常日志关闭，
采用-Os。V1/V2/谷仓共用firmware/common/build_profile.cmake，IDF加载前设置Release
SIZE／系统及bootloader WARN，并强制缓存trace／ROOM_DIAGNOSTICS／HANG_DIAG关闭；
Debug恢复-Og／INFO。保留断言、看门狗与既有回滚保护。Platform::logf/logLine在Release
静默，新增errorf/errorLine保留存档／资源／硬件等失败；M5直接启动／音频模式日志也接入。
市集沿用DEBUG_FEATURES门禁，三家族真实Release旅程验证隐藏／直接入口拒绝／普通探索可用。
谷仓GCC14 -Os触发八点ADC std::sort数组边界告警，改有界插入排序，采样／去极值／平滑
不变；真实电池／充电趋势-Os测试及ASan/UBSan通过。配置3／门禁3／
存档迁移和codec／音乐续播／架构13／家族10／OTA3通过。驱动全套在音量49%旧断言失败，
冻结修改前同样复现，未改音量策略或旧断言，不称整仓全通过。四目标Release构建通过，
实际参数和日志等级已核对；组件与SHA256位于.stick-local/firmware/<目标>/release-profile-20261107/，
为未签名构建快照，谷仓安装仍须sdgoods_release.py签名配对资源银行流程。
证据.stick-local/artifacts/release-profile-20261107/verification.md；未访问设备／串口，实机待验收。

2026-11-07 微雪V2探索随机冻屏修复（已刷入带检测的Debug版）：用户反馈捡物品／进入下一张
图前画面冻结。串口确认主循环停止、无DMA超时与看门狗；地图生成约23.8万次主机调用无卡死。
新增可选STICKMON_HANG_DIAG（默认关）检测器＋coredump复现：explore_lcd卡在
spi_device_acquire_bus(portMAX_DELAY)←CO5300 draw_bitmap，主循环等其结果。根因为LCD SPI
中断在core0、流水线worker在core1，IDF 5.5.4 spi_bus_lock中断收尾与另核acquire交错时
中断不唤醒等待者，总线锁永久无主。修复：explore_lcd改为固定core1的面板任务，显示启动、
逐条draw_bitmap、开关屏与亮度（AmoledPlatform::setBrightnessWriter）全部经它执行，SPI中断
随之装在core1，渲染独占core0；锁屏动画同步传屏前先收回在途探索帧；等待探索帧限3秒、
DMA超时均先保存再重启，不再永久冻屏。中间版曾把worker固定core0（帧率降），已被取代。
V1单核同步传屏不受影响。test_amoled_explore_pipeline（新增面板调用线程／排序检查）、
v2_display、source、ci_build_scope、架构检查通过；test_amoled_debug／expedition_departure
两项为未改动文件的既有断言失败。仅写OTA0并校验，存档与CST820校准正常；夜间约4.5分钟、
白天（游戏06:00～08:32）各一轮长草园探索无冻屏。同为白天对比原版：帧间隔23.5→22.0ms
（约42.6→45.5fps），最差p95 99→70ms；随机路线非固定基准。证据
.stick-local/artifacts/amoled-pickup-freeze-20261107/，固件
.stick-local/firmware/amoled-v2/explore-lcd-core1{,-diag}-debug-20261107/（设备为diag版）。

2026-11-07 探索战后音乐续播（未刷机）：共享AudioManager在EXPLORE切入普通／特殊战斗前
保存最早尚在播放队列内的ADPCM块游标，返回EXPLORE时seek并恢复，避免原先每战从头。
保存循环跳过量与8kHz插值前样本，扣除已预取未播块；接口只能报告块数，恢复粒度为块
（当前探索每块2041/16000秒，可能重复不足128ms，非逐采样精确暂停，DMA另有缓冲）。
战斗内锁屏／静音／切普通或特殊战不覆盖游标，探索自身静音／挂起也保留；首次恢复提交
失败可重试，返回房间／集市／NONE及显式stop清除，游标仅内存态，不改存档／UI／资源。
新增test_audio_manager_explore_resume.py：真实AudioManager＋ADPCM解码与不中断PCM逐块比较，
队列2／3，普通／特殊战、连续遭遇、循环尾／非对齐循环及8kHz插值、静音／挂起／省电、
失败重试及新探索重置，ASan/UBSan通过；原实现同回归在PCM比较处失败。房间静默、
微雪音乐上下文4、混音器契约、架构13项通过。AMOLED V1/V2 lite Debug、StickS3 Debug、
谷仓独立build-explore-music-resume-debug（Debug/trace ON）构建通过。
证据.stick-local/artifacts/explore-music-resume-20261107/，匹配微雪／谷仓组件保存于
.stick-local/firmware/<目标>/explore-music-resume-debug-20261107/；谷仓按sdgoods_release.py
配对资源银行流程安装，禁止直接使用原始flash_args。未访问设备或串口，未刷入，实机听感待验收。

2026-11-07 微雪V2刷机串口冲突防护：用户flash.sh --variant lite --debug失败于
esptool flash_set_parameters／StopIteration；两次失败均已连上S3并启动stub，尚未写入。
发现PID22814的capture_v2_serial_20261107.py自21:55持续读取cu.usbmodem2101，
与22:01／22:02失败重叠，竞争读取是主要嫌疑，未停止其他诊断任务或实机重刷证实。
flash.sh现用lsof在构建前／刷入前／显式擦除后检查占用，显示持有进程并退出，不自动
杀进程；烧录速率明确115200（此前IDF默认460800，较早同速曾刷成功，不能断言速率
为根因）。真实占用路径提前退出、六项mock进程检查及bash语法／diff通过，无串口
读写或刷机。证据.stick-local/artifacts/amoled-flash-serial-20261107/。

2026-11-07 微雪变隐龙／耿鬼摊位新增面板外点击返回：展开后上方市集区域0≤y<168
作为独立TouchTap目标，按下／抬起均在外侧才触发；子页复用原返回按钮路径，结果页
回服务首页，首页以原520ms收起动画退出交谈。面板内部空白、跨边缘拖动、回忆列表
滚动、动画／提交期间不触发，不穿透为市集行走；招呼仍点击跳过，右上×和原返回按钮
保留。test_market_shop／test_market_tutor真实应用旅程覆盖所有子页、确认不扣费、
结果不重复交易、边缘小位移／拖出／滚动及动画退出，规则ASan/UBSan与V2 Lite Debug
构建通过。证据.stick-local/artifacts/market-stall-outside-back-20261107/；未刷机。

2026-11-07 摊位金币配色按用户最新要求统一面板：变隐龙／耿鬼右上金币改浅米色底
RGB(248,248,224)、深蓝灰字RGB(40,56,80)，保留原符号／字号／位置／随面板动画；
共用drawCoins仅backdrop样式换色，普通商店仍原金色。真实秘技旅程（字形mask和
面板配色断言）与V2 Lite Debug构建通过，预览已看；本轮未刷机。证据
.stick-local/artifacts/market-stall-coin-palette-20261107/。

2026-11-07 微雪变隐龙／耿鬼摊位面板动画与关闭按钮（V2已刷入）：共用
src/ui/common/MarketStallMotion.h，缓动由ease-out改为三次ease-in-out，招呼框360ms
从底部升起，打开420ms由招呼框顶边y328长成面板至y168；关闭下沉280px，单独改为520ms smoothstep（峰速1.5倍均速，原三次曲线峰速3倍导致中段跳帧）；金币随面板
滑入滑出，招呼时隐藏。首页（货架／菜单）底部「离开摊位」改右上「×」（绘制中心
(336,194)，命中(304,168,64,52)），拖离不关闭；子页底部「返回」按用户要求保留。
货架三行56→72px，耿鬼菜单三行52→68px。LEAVE文案常量保留未用，避免“摊”字形变化。
test_market_shop／test_market_tutor（规则＋真实应用旅程、单调平滑、锁屏暂停）通过；
test_market_scenes微雪第179行对话断言改动前同样失败。V1/V2 lite Debug构建通过，
V2 app SHA256前缀ab3cd67900db；固件.stick-local/firmware/amoled-v{1,2}/market-stall-panel-debug-20261107/，
证据.stick-local/artifacts/market-stall-panel-anim-20261107/。沙箱禁止直接开串口，
经用户终端运行.stick-local/scratch/flash_v2_stall_20261107.sh：同MAC28:84:85:55:56:90／16MB，
分区表与构建一致，otadata启动ota_0；资源分区摘要一致跳过，仅写OTA0 0x20000并
verify_flash通过，无备份，NVS／触摸校准未写。25秒启动ELF1bb2f95ee…匹配，原档species6
Lv51、CST820校准加载，无panic。V1仅构建。实机动画流畅度与「×」手感待用户验收。
刷机日志.stick-local/artifacts/market-stall-panel-flash-20261107/。
用户反馈收起仍略卡后，收起单独改520ms smoothstep并再次仅写V2 OTA0（资源一致跳过），
verify通过，启动ELF2c615b0b8…匹配、原档与校准加载、无panic；固件
.stick-local/firmware/amoled-v2/market-stall-close-debug-20261107/，日志同名close-flash目录。

2026-11-07 谷仓单击熄屏省电第一档（已构建，未刷机）：熄屏时背光0后再发DISPOFF＋
SLPIN（SDK驱动无sleep接口，经项目NativePanel按QSPI命令格式补发）；亮屏发SLPOUT，
下一次提交满120ms后在面板关闭状态写完整一帧，再DISPON和背光，避免露出旧画面；
120ms内再次熄屏会先补足间隔。共享引擎新增STICKMON_LOCKED_UPDATE_MS（默认0保持原
节拍），谷仓设1000：熄屏后核心更新／输入每秒一次，不做视觉更新；进入熄屏当下立即
跑一次更新并安排场景（孵化IMU等即时响应）；场景按≤100ms子步补齐，超过2秒的停顿
不补。主循环熄屏时每轮等40ms（按键去抖30ms），熄屏期间暂停电池ADC采样（亮屏后
充电趋势本就重置）。sdkconfig.defaults开CONFIG_PM_ENABLE（未开浅睡／tickless），
熄屏esp_pm_configure锁80MHz，亮屏恢复240MHz；新目录构建才会生效。
谷仓新增locked-standby真实房间旅程（60秒核心更新60次、不出帧、饥饿精灵熄屏中
吃到食物、游戏时间推进、子步补到+900ms、亮屏重绘），完整圆屏78项、Debug、调度、
架构、家族源及Stick／OSTB房间release/debug像素回归通过。谷仓release构建
build-locked-standby通过，app1496144B，SHA256
4d1bcdea07e3ca0da7f7d7ea2f9ef1076860cfc4f82c0010ad416ac611af589e；首次构建需pypi装
littlefs-python，沙箱下复用本机已有venv的site-packages。StickS3 pio构建因沙箱不能写
~/.platformio未执行。未访问设备、未刷机；实机唤醒观感、80MHz下外设与电池续航收益
均未测量。证据.stick-local/artifacts/sdgoods-locked-standby-20261107/，固件
.stick-local/firmware/sdgoods/locked-standby-20261107/。

2026-11-07 变隐龙奇珍小铺（微雪已接入，主机验证，未构建固件／未刷机）：点变隐龙气泡转向→
招呼（点击跳过不误买）→三格货架→单件确认→成功页。每游戏日独立种子：稀有位心之鳞片600
（未解锁用甜甜蜜）、补给位最高已解锁药九折、特价位进化石900／糖果1800（共用每日限购），各1件，
价低于商店买价高于卖价。规则src/game/MarketShopService.h，界面src/ui/common/MarketShopUi.h，
存档升v7/schema5追加MarketShopState；确认即saveState，失败回滚。字库1217→1222需带资源。
tools/test_market_shop.py规则＋真实应用旅程通过，耿鬼旅程及存档/OTA回归通过。
证据.stick-local/artifacts/market-kecleon-shop-20261107/。

2026-11-07 微雪耿鬼秘技小屋界面精简（已构建／主机验收，未刷机）：右上金币复用
商店UiCommon::drawCoins的金色C符号和32px数字，配无边框黑底托；主菜单只保留图标＋
本期秘技／回忆技能／技能特训，删除副标题和价格，52px行内居中，不可用置灰且点后
提示1.8秒。学习页删除精灵名字下副标题、技能效果说明及预览价格，保留属性／分类／
威力／命中与16px右侧可学状态；费用仍在确认页明确显示。300ms缓出同步推进面板从
底部滑至y168和镜头上移，离开反向收起后才结束交谈；动画中阻断触摸，锁屏／熄屏
暂停，onWake重置服务时钟避免追赶。真实AmoledApp旅程覆盖动画误点、暂停恢复、
离开、金币与商店像素一致及原授课／保存失败回滚／特训／回忆，规则ASan/UBSan及
商店／架构／家族32项通过；V2 Lite Debug最终构建通过，预览已检查。zh16/zh32仅新增
“饥”（1216→1217），旧字形逐项不变，安装须带资源。证据
.stick-local/artifacts/market-tutor-ui-20261107/；本轮未访问设备，实机动画手感待验收。

2026-11-06 谷仓进化动画圆屏适配（代码／主机验证完成，未构建固件或刷机）：原家园进化
用StickS3 240×135坐标画在360圆屏左上（约21%被圆裁掉），家园／探索只提示“A继续”且
无触摸入口，谷仓无A/B键会卡在进化页。现round_touch/ProgressionUi覆写renderEvolution／
renderEvolutionCancelled：三入口（家园／探索／背包进化石）统一走360原生面、去顶栏；
精灵1x水平居中x=180、脚底y=220，同心圆光晕背景圆心(180,168)，光环／放射线只在精灵
上方与两侧，文字y=240，胶囊“继续”(110,290,140×36)。时序沿用EvolutionSequence 4s。
触摸：动画中按住任意处1s取消（屏幕边缘进度环，短按不取消，取消后松手不误继续），
完成／已取消后点任意处继续，完成前点击无效；scene update轮询长按。删除ItemFeedback
旧进化导航与itemProgressionTap。StickS3／微雪绘制不变（基类仅改virtual）。
圆屏home-notice／learn-move／bag-flow新增进化断言及surface／menu-fixed／explore-round／
battle-round／status-round／team-moves／explore-select回归通过（非ASan的-O0快速构建），
StickS3／compact_touch语法检查通过。帧与before源码位于
.stick-local/artifacts/sdgoods-evolution-round-20261106/；实机手感待验收。

2026-11-06 谷仓一键刷机提速（已实机验证）：install改用sdgoods_flash_install.py，
复用一次ROM／stub连接，对八区先读最多4KiB前缀；不同直接写，相同须完整设备MD5
匹配才跳过，两份资源独立检查，不用本地刷机缓存。变化区走esptool原生write_flash，
观察真实写后MD5调用，逐项核对地址／长度／摘要，缺校验／串口错误即失败；保留签名、
MAC／32MiB／精确布局、扇区保护与配对绑定，仅全部通过才复位，取消末尾重复verify。
新ROM SPI容量检查先加载stub再读ID；首次顺序错误在写入前失败，已修正且回归覆盖。
同MAC ec:da:3b:67:10:5c、cu.usbmodem1101完整flash.sh由此前328.41秒降至118.05秒
（资源未变的代码更新场景，约64%），5区摘要匹配跳过，两个app＋resmeta三写后校验通过，
跳过25198016B，写3033728B；资源两次摘要仍各约39秒。app1484096B，SHA256
2a30b4a550eaea477f73c547b03240d55f4119ca9631c6279bf60448bdfde0e8，签名Release包
.stick-local/firmware/sdgoods/flash-release-20261106-022458-70685/。
22秒串口确认LittleFS A／restored=1／hatched=1／species4／save ok、触摸参数加载，
无panic；打开采集触发USB_UART_CHIP_RESET。启动保存音量17，期间另有多次音量变化，
本任务未发送触摸／音量指令；不将其描述为音量始终0。无备份／整片擦除，串口释放。
智能安装16项与进程3项、实际构建／签名打包／dry-run／语法／diff通过；首装全变化与
单槽损坏由主机模拟验证，本轮未额外做完整资源改写。证据
.stick-local/artifacts/sdgoods-flash-speed-20261106/verification.json。

2026-11-06 谷仓最大音量再降低30%（已构建，未刷入，取代此前70%上限）：用户将
“降低80%”纠正为“降低30%”，以已接受的70%为基准，最终原PCM幅度70%×70%=49%；
AudioPolicy::MAX_OUTPUT_PERCENT=49。滑杆0～100保留，50对应原幅度24.5%、100对应49%，
默认值／保存值／设置页实时音量同步及抗锯齿保持。真实音频驱动ASan/UBSan（101档、
正负极值、限幅、I2S样本、静音／PA／挂起／欠载）与谷仓Release构建通过。
证据.stick-local/artifacts/sdgoods-volume-cap49-20261106/verification.json；未访问设备或
刷机，实体听感待确认。14%只是中途被用户纠正的临时构建，不能作为最终音量方案。

2026-11-06 谷仓孵化／探索教程修正（已构建，未刷入）：孵化移除擦／戳按钮及照料
文案，改为摇晃设备或点击蛋；复用微雪HatchShakeDetector的滤波／连续峰值／静止
重置规则，摇晃沿用wipeCount、点击沿用pokeCount，保留计时／初始精灵／存档格式。
两种照料触发1秒蛋晃动，点击另显示700ms爱心；离页／锁屏关闭IMU，恢复后重测，
IMU不可用仍可点蛋。菜单与首页共用38×38三横线绘制与命中几何；探索教程改为
“点击上方打开菜单”，战斗提示保留。真实圆屏ASan/UBSan新档／摇晃去重／取消拖动／
锁屏／IMU失败／菜单往返／跨进程孵化与教程／重置／完整菜单旅程通过，画面已检查。
Stick release/debug共1320帧与原基线一致；共享检测／架构／家族源／调度27项及谷仓
Release、StickS3 release、微雪V1 lite构建通过。证据
.stick-local/artifacts/sdgoods-hatch-touch-20261106/verification.json；未访问串口或刷机，
真实摇晃手感待设备验收，音量／触摸校准未操作。

2026-11-06 谷仓设置页音量突增防护／滑块去毛刺（已构建，未刷入）：启动默认和
存档恢复原已统一；尚未抓取用户设备上突变现场，不能断言实机根因。连续设置页
改从HAL当前生效音量对齐页面／保存值，避免入页重套旧值或默认值；不重复写驱动
或试听。setSettingValue先限0～100再保存，谷仓音量／亮度文字仅选中，滑杆才改值，
避免旧文字点击加10并试听。保留默认5和70%输出上限。圆屏滑块内外圆使用4×4
整数覆盖率混合实际背景，普通／按下均平滑；其他家族默认旧绘制。
ASan/UBSan真实引擎验证旧状态50／实时5、23、0入页不放大，反复入页无试听，
文字／滚动不改值，横拖设置／HAL／驱动／存档一致，越界255限100；双字节序／
裁剪／内存guard与圆周对称和中间覆盖通过。原设置导航及完整圆屏fresh/restore
跨进程旅程、架构13／家族10、谷仓Release构建通过；实际页与前后放大图已检查。
证据.stick-local/artifacts/sdgoods-settings-audio-20261106/verification.json；本轮未
访问串口或刷机，实体听感待验收。编译期间已有并发孵化改动，重新配置纳入新
RoomControls.cpp后通过；不将编译缓存未纳入新文件视为设置逻辑问题。

2026-11-06 谷仓升级学技能改为参考微雪（代码／主机验证完成，未构建固件或刷机）：
原圆屏沿用240方屏“学习新招式？yes/no→左右分栏→再确认”，战后画在240页面，
道具入口用240坐标画到360圆屏，左列表被圆边切掉。现新增round_touch/ProgressionUi：
LEARN_MOVE／MOVE_REPLACED走原生360面；满位直接列出新招式／技能2／技能3三卡，点卡
原地展开详情、再点收起，底部单按钮“替换”／“放弃学习”立即生效（用户确认不二次确认）；
有空位直接学会，仅显示“新招式／继续”（用户确认）。共享MoveLearnState与学技能规则
不改，基类三函数改virtual。learn-move旅程及其余圆屏模式／跨进程存档通过，Stick／OSTB
学技能等111帧与改前一致；final_ui ostb-debug、explore_ui_parity在改前源码同样失败。
证据.stick-local/artifacts/learn-move-round-20261106/verification.md。

2026-11-06 谷仓最大音量缩至原幅度70%（已构建，未刷入）：用户选择70%，设置／
存档仍0～100，AudioPolicy::MAX_OUTPUT_PERCENT=70，最终PCM一次缩放（mono×
slider×70/10000），50映射原35%、100映射原70%，避免先取整使低档重复。只影响
谷仓最终扬声器输出；原音量保存值保留，0继续静音，实际听感待设备确认。
实际Audio驱动ASan/UBSan通过，含101档单调、正负满幅界限、双声道一致、越界限幅、
真实I2S样本以及静音／PA／欠载／挂起回归；谷仓Release构建通过。
证据.stick-local/artifacts/sdgoods-volume-cap-20261106/verification.json，未访问串口或刷机。

2026-11-06 谷仓新档默认音量改为5%（已构建，未刷入）：BuildConfig新增可覆盖的
STICKMON_DEFAULT_AUDIO_VOLUME（共享默认仍50，范围0～100）；谷仓CMake单独设5，
PlayerSettings与HAL使用同一默认值，驱动初始化仍先静音。已有存档按保存值恢复，
不将设备当前0强改为5，也不改存档结构。谷仓主机编译参数同步5，ASan/UBSan真实
onboard-fresh断言状态／实际音量均5；既有fresh/restore完整旅程确认旧档0与实际静音
跨进程保留，谷仓Release构建通过。日志.stick-local/artifacts/sdgoods-default-volume-20261106/。
本轮未访问串口、未操作设备或刷新先前已安装包。

2026-11-06 谷仓一键刷机100%静默排查／修复（已实机完整复核）：原flash.sh在资源A
0x00a8ce84与资源B最后100%后各约61秒无输出，随后正常继续；原整轮347.95秒成功。
esptool百分比在发送最后压缩块前打印，12MiB镜像末块14753B展开7942524B（主要空白），
尾块完成约22秒，之后资源摘要校验仍耗时；子进程stdout缓冲又将完成／校验信息延后。
tools/sdgoods_release.py新增run_install_command，esptool开启无缓冲输出，100%明确提示
尚未写完／校验，每5秒静默报告当前阶段／总耗时，保留原错误码和输出；未改写入区域／
设备身份／布局检查／完整8区域校验。新增test_sdgoods_release_process.py三项通过。
改进版再完整跑flash.sh耗时328.41秒（构建增量，未声称Flash加速），36条等待提示，
8区域digest通过，同MAC ec:da:3b:67:10:5c／cu.usbmodem1101，最终app1482944B，SHA256
d32db3a326c8ee2b4e6bcd92f04c3e2645db30913bee72727d619661780e7a94。
20秒启动确认原档restored=1/hatched=1、LittleFS A、ready=1、save ok、音量0，未见panic；
无备份／整片擦除，原NVS/PHY/userdata和触摸参数保留，串口已释放。最终包
.stick-local/firmware/sdgoods/flash-release-20261106-014053-59361/；前后计时日志、
原工具冻结与verification.json在.stick-local/artifacts/sdgoods-flash-stall-20261106/。

2026-11-06 新增谷仓一键脚本firmware/sdgoods/flash.sh：默认签名Release，串联build.sh、
sdgoods_release.py package/inspect/install；安装要求显式--port/--mac，支持--debug、
--build-only、--dry-run、--private-key。构建缓存按模式与sdkconfig.defaults摘要隔离；
发布包／日志分别落.stick-local/firmware/sdgoods/与artifacts/sdgoods-flash-日期/，
任一步失败停止，复用现有完整配对安装和存档／校准保护，不使用应用内USB update。
bash语法、非法参数／路径空格／异目录调用／模式／dry-run无写入检查通过，实际
--build-only构建／签名／LittleFS一致性／inspect通过；本任务未访问串口或重复刷机。
验收日志.stick-local/artifacts/ui-release-flash-20261106/flash-script-build-only-final.log。

2026-11-06 用户授权后谷仓最新工作区签名Release已刷入，同MAC ec:da:3b:67:10:5c，
串口变为/dev/cu.usbmodem1101。新build-ui-release-20261106启用I2S_ISR_IRAM_SAFE，
debug/trace/room_diagnostics均0；配对完整install的8区域digest全部通过，无备份／整片
擦除，NVS/PHY/userdata未写。app1479984B，SHA256
d2602a5c6a4473bf25951dc52c0a829ff215612801f194215289912943cf5c98。
20秒串口确认LittleFS A挂载、ready=1、原档restored=1/hatched=1、species1/261、save ok、
音量0/PA OFF；打开监视触发一次USB_UART_CHIP_RESET，未见panic。日志同时出现6次
MonsterAI stuck mode=3 pos=100.7,99.4 target=81.0,78.0重试，本刷机任务未诊断是否永久
卡住，不将启动正常描述为寻路问题已解决。串口已释放，实体面板／触摸／听感待用户确认。
包.stick-local/firmware/sdgoods/ui-release-20261106/，证据
.stick-local/artifacts/ui-release-flash-20261106/verification.json。

2026-11-06 谷仓音频卡顿修复（代码完成，未构建固件／未刷机）：根因是sg_audio任务
持mutex_跨阻塞i2s_channel_write（约16ms）且放锁后立即重取，主循环queuedPcm／
playPcm／setChannelVolume被饿住，音乐补块延迟→欠载；欠载时又立即关PA／I2S放大断音。
现Audio.cpp只在混音时持锁，I2S写在锁外；queuedPcm／playing读原子计数（每次混音器
变更在锁内发布）；AudioPolicy::OutputGate欠载保持输出静音约300ms（19块）再关，
静音／锁屏立即关；DMA 6→8描述符（120ms）；谷仓CMake定义
STICKMON_MUSIC_QUEUE_TARGET_BLOCKS=3（共享默认仍2，其他目标不变）；
sdkconfig.defaults加CONFIG_I2S_ISR_IRAM_SAFE=y（存档写Flash时DMA清零而非重放旧块，
已有build目录sdkconfig里为not set，需新-B目录构建才生效）；main.cpp锁屏时
setMusicSuspended，避免挂起期间每tick重开音乐。主机模型（真实AudioManager＋
PcmMixer＋模拟DMA，66ms节拍周期停顿）无断音最大停顿110ms→270ms。驱动假件
ASan/UBSan（锁外写、无锁计数、欠载保持／关闭、失败恢复）、hardware、房间全旅程、
音乐静默／混音器契约通过。既有问题未改：sdgoods_driver_fakes.h在Linux GCC触发
misleading-indentation；PcmMixer负数左移UBSan告警。证据
.stick-local/artifacts/sdgoods-audio-stutter-20261106/。实机听感待验收。

2026-11-06 谷仓队伍页HP条增加与探索HUD相同层次的2px白色外沿＋1px黑色细描边，
原8px圆角填充／颜色阈值／血量比例／用药反馈插值保持，轮廓总高14px。仅改圆屏
TeamUi绘制；外置内容ASan/UBSan背包选目标／回血／复活／战斗用药旅程与队伍技能
旅程、架构／家族23项通过，满血和1HP导出画面已检查，边框不挡数值或邻卡。
未构建固件／刷机，证据.stick-local/artifacts/team-hp-outline-20261106/verification.md。

2026-11-06 谷仓探索侧菜单末项由“返回”改为“关闭”，仅圆屏文案调整，原关闭后
暂停地图行为不变，其他设备／返回导航不改。20px圆屏字库补“关／闭”且原字形
逐字节不变，16px资源不改；架构／家族23项与diff检查通过，未构建或刷机。

2026-11-06 谷仓拾取提示顶部跳变修复（代码／主机验证完成，未构建固件或刷机）：
menuAvailable仅WALKING且无移动锁时为真，圆屏绘制误用其隐藏菜单并将地图名面板
84px变128px，PICKUP期间出现双重跳变。现触摸路线顶部固定保留菜单／标题几何，
原phase／滑冰／落穴等输入门控保持，提示期间不可打开菜单。真实resolvePickup伤药
前／中／后顶部逐像素一致，点菜单不穿透，点提示恢复后菜单可用；外置内容完整圆屏
ASan/UBSan旅程／跨进程存档及架构／家族23项通过，拾取导出画面已看。
本批只改round_touch绘制与回归，不改共享玩法、微雪／Stick、音量／存档／触摸参数；
证据.stick-local/artifacts/explore-pickup-header-20261106/verification.md。

2026-11-06 谷仓独立资源三批实现及首版安装：项目自有 32 MiB 表、两份 3 MiB app
与两份 12 MiB 只读 LittleFS 固定配对；图片／音频／字体与 47 张内容表外置，
原 sg_stickmon／sg_touchcal NVS 和保存 schema 不变。内容等价／错误输入／资源扩展、
事务失败模型与完整圆屏旅程／跨进程存档通过，StickS3／微雪 V1/V2／谷仓构建通过。
ancient_valley／mist_forest／valley_region 三个既有遮挡断言在冻结修改前代码同样失败，未覆盖基线。
用户明确首版无线上用户，本轮停止扩展升级故障验收，首版通过 ROM 完整 install 交付；
应用内 USB update 仍有 ACK 超时，统一 VFS 驱动后仍复现，坏语义包未传完，回退未实测，
不得描述为稳定 OTA。无备份、无整片擦除，8 个写入区域独立 digest 通过；同设备
MAC ec:da:3b:67:10:5c 运行签名 Release A，debug=0／trace=0，市集入口隐藏，
restored=1／hatched=1、species1／261、save ok，原触摸映射加载，15 秒无 panic／错误／循环重启。
Release app1479440B，SHA256 5ae520272841c2911d2e8f672ed99ed3d35886692bc54d3059164b655602f479，
LittleFS used4218880／12582912B；实体面板／音频／手指／真实断电未验收。串口已释放。
规格 specs/resource-storage/，证据 .stick-local/artifacts/sdgoods-resource-storage-20261105/；
匹配发布包 .stick-local/firmware/sdgoods/resource-ab-release-production-20261106/。

2026-11-06 谷仓房间双精灵卡住修复（代码完成，未构建固件／未刷机）：真实房间AI长时
压测（双精灵、随机idle、注入1～15秒主循环停顿）复现：①双人聊天TALK在总超时(9秒)后
进入SETTLE→CONVERSATION，下一帧又因总超时回到SETTLE，永久循环，两只面对面不动且不再
跑各自AI，仅idle／弹窗等打断；正常帧不会超时，需主循环停顿≥约2.7秒（如按住电源键／
存档阻塞）触发。现CONVERSATION不再被总超时打断，并加超时15秒强制结束兜底。②主精灵沿
走动区边缘时0.75纵向速度使轨迹偏离已校验直线，每帧出界→重规划→不计卡住，永久停在
WANDER／DASH；现出界时改沿规划直线前进，重规划不重置卡住计时。③主精灵去食盆时仅按X
判断“去床”而忽略睡着的队友，直线穿过睡眠队友后被间距卡死；现需X/Y都接近床。④访客
IDLE用taskUntilMs=0比较，开机超2^31ms(约24.8天)后访客永久不动；现0视为无期限。
修复前压测单只卡住175/200、两只同时卡住11/90，修复后0/200（含停顿注入）。门口出发
ASan/UBSan、首页像素一致、圆屏完整旅程通过；dual_actor／pair_interaction既有静态检查
失败与本次无关。证据.stick-local/artifacts/room-stuck-fix-20261106/。

2026-11-05 谷仓探索选择页静态预览版已刷入：核对同MAC ec:da:3b:67:10:5c、
现行OTA0 0x20000／3MiB及OTA选择，仅写3104192B Release app，写入hash通过，
未备份或写资源／分区表／NVS。首次写入后USB断连，用户重新接入后继续采集启动，
确认已运行目标ELF7d53a2ff1…，无需重复刷入。原档restored=1／hatched=1，
species1与261资源就绪、save ok、首帧509ms／精灵592ms；原右侧渐进补偿启用。
当前设备恢复的保存音量为45（不是此前历史0），本轮未发送音量／设置操作。
30秒应用启动无panic／错误／循环重启，串口已释放；真实预览观感与手指操作待用户
验收。固件仍为explore-preview-fit-release-20261105，manifest已标记flashed；
证据.stick-local/artifacts/explore-preview-fit-flash-20261105/verification.md。

2026-11-05 谷仓设置调音量仍无声（代码／构建完成，未刷入）：定位为内嵌资源仅含
市集BGM，缺少其余音乐、菜单音效与精灵叫声；实际设置页滑杆会传到驱动，试听因
缺失叫声失败。新增可选只读SMRA音频层，通过resources标签映射，内嵌资源优先且
外部仅接受.smonaudio／.smoncry；坏包或缺分区继续原内嵌画面。构建额外生成
2185355B音频包（5首BGM／15音效／64叫声）；需同时安装应用与音频，单刷app无效。
实际设置页复现修复前失败，64叫声／15音效／5音乐解码播放、静音、坏索引拒绝与
内嵌数据隔离ASan/UBSan通过，完整圆屏交互与跨进程存档、驱动、架构13／家族10／
房间静音回归及Debug／Release构建通过。实体扬声器听感待安装后确认。
分区区别已核对：今天设备启动日志为OTA0 0x20000／OTA1 0x320000，各3MiB；
resources 0x620000／4MiB。工程编译另复制SDK表（factory launcher 3MiB＋16动态
OTA槽），不能套用生成flash_args或称整机只有3MiB；本轮未改分区、未访问设备。
证据.stick-local/artifacts/sdgoods-audio-resources-20261105/verification.json；冻结包
.stick-local/firmware/sdgoods/audio-resources-{debug,release}-20261105/，均flashed=false。

2026-11-05 谷仓探索选择页按宽度决定轮播（完成，未刷机）：实际精灵帧宽总和＋
原12px间距≤312px时整组居中、各成员只绘一次，不滚动；保留24px圆屏侧余量。
超宽组保持原全宽轮播，绘制／刷新调度／预加载避让共用PreviewLayout判断，资源
就绪后的静态组不周期重绘或轮播唤醒。精灵尺寸、池成员、稀有剪影和出发交互不改。
ASan/UBSan轮播宽度临界值／静态与超宽切换／像素对照／多成员停驻6秒、完整圆屏
旅程与跨进程存档、Debug探索、架构／家族23项及独立谷仓Release构建通过；
超宽组500ms为15帧（14局部）／8核心tick，导出圆屏静态与滚动画面已检查。
青草径活跃池随种子变化，测试包含静态两成员及超宽组，不保证该地图永远不滚动。
证据.stick-local/artifacts/explore-preview-fit-20261105/verification.md；独立
build-preview-fit-release产物3104192B，SHA256
09b7539df6385860ecbe7e4b2b51f9742b82bd0622c91c85092dfb8cea0ad77f，冻结固件
.stick-local/firmware/sdgoods/explore-preview-fit-release-20261105/。
未访问设备／串口，音量0、存档和触摸参数未操作；实机观感待验收。

2026-11-05 谷仓双精灵房间静止排查：用户反馈两只不动，后确认重启正常。
串口实际运行debug=0/trace=0，恢复species1与261、音量0/PA OFF、饱腹28；
精灵缓存就绪、预加载与save正常，未捕获卡住时的角色任务／坐标，根因尚未确定。
串口打开未主动设置复位，但收到USB_UART_CHIP_RESET启动；随后esptool chip_id
核对同MAC ec:da:3b:67:10:5c也执行了RTS复位。未写Flash、未备份、未重置存档。
新增默认OFF的STICKMON_ENABLE_ROOM_DIAGNOSTICS，独立于Debug/trace，便于
非Debug问题复现；开启后每5秒输出RoomBoard/RoomEngine/RoomState/RoomActor，
含按键/锁屏/调度/淡变/阻塞弹窗/任务/坐标/路线/期限/HP/饱腹/睡眠。普通构建无日志。
build-room-diagnostic已以debug=0/trace=0/room_diagnostics=1构建成功，未刷入；
真实房间AI/夜间进食/64种双精灵出门回归及架构13/家族10通过。
证据.stick-local/artifacts/sdgoods-room-still-20261105/。用户已恢复后停止设备操作，
不将重启恢复等同根因修复；后续若复现应先抓取现场状态，再决定复位或更新。

2026-11-05 发布前暂时隐藏市集入口：ExploreDestinations使用现有
STICKMON_ENABLE_DEBUG_FEATURES，Release目录不含市集且visible／unlocked拒绝，
共享与AMOLED启动入口同步防护；Debug保留第一项。原六区ID／解锁／存档不变。
三家族Release真实应用验证隐藏与直接启动拒绝、普通探索可出发，Debug原市集旅程通过；
市集13项、圆屏探索轮播通过，StickS3／微雪V1/V2 Lite／谷仓四目标Release构建通过。
证据.stick-local/artifacts/market-release-gate-20261105/；冻结产物
.stick-local/firmware/<目标>/market-release-hidden-20261105/，V2含五件套。未刷机。

2026-11-07 耿鬼秘技小屋（微雪已接入，未刷机）：点耿鬼气泡转向→招呼→菜单（本期秘技／
回忆／特训）。每游戏日独立种子从队伍候选抽一招（Essentials Gen6 TutorMoves 兼容表
src/game/MoveTutorData.inc＋升级表，排除可回忆招，无等级上限），共享规则
src/game/MarketTutorService.h，界面 src/ui/common/MarketTutorUi.h。存档升 v6/schema4 追加
MarketTutorState；确认即 saveState，失败回滚。字库补7字需同步资源；谷仓外置表54/55。
tools/test_market_tutor.py 主机规则＋真实应用旅程通过；固件未构建，谷仓/Stick未接入。
规格 specs/market-stalls/gengar-ui.md，证据 .stick-local/artifacts/market-gengar-tutor-20261107/。

2026-11-04 精灵头顶聊天气泡放大（完成，V2已刷入）：共享MARKET_CHAT由14×14放大
50%至21×21逻辑像素，微雪约32×32px；头顶向上展开、保留2px逻辑间距，遮挡和脏区
同步，编辑器npc-editor.js预览同尺寸。底部按钮不改、无新位图。三家族真实应用
ASan/UBSan、说话时日／夜缓存与直接绘制逐像素对照、编辑器JS语法、四目标构建通过。
已看微雪前后对比和圆屏；只有对白头顶气泡区域变化。V2同MAC仅写0x20000，无备份，
写入hash与重启原档／CST820检查通过，串口释放。app2138224B（增16B），SHA256
36989922127de8e4f636f073c8a292dea9edd9f3bbe28f5ff59d400e85fb30d2；资源增量0B。
证据.stick-local/artifacts/market-head-chat-20261104/，匹配固件
.stick-local/firmware/<目标>/market-head-chat-debug-20261104/；其他目标仅构建。
本轮头顶观感由真实主机渲染验证，未声明实体面板或手指触摸验收。

2026-11-04 市集对白翻页三角放大（完成，V2已刷入）：共享点阵9×7按2倍绘为18×14，
保持红色／描边和640ms跳动；圆屏左移8px／上移5px留出完整跳动安全区。三家族真实
应用ASan/UBSan及四目标构建通过；真实渲染逐像素比较仅完整页箭头区域变化，Stick11帧
不变，微雪／圆屏图已看。V2同MAC仅写0x20000，无备份，写入hash与重启原档／CST820
检查通过，串口已释放；app2138208B／资源包与前版同大小，SHA256
000e8bc337c3cb35f971d298018d9961ec3e89f7cf1bc032dab2bbbc5e8ceef0。
证据.stick-local/artifacts/market-dialogue-arrow-20261104/，匹配固件
.stick-local/firmware/<目标>/market-dialogue-arrow-debug-20261104/；其余目标未刷入。
箭头观感由真实主机渲染检查，未声明实体面板或手指触摸验收。

2026-11-04 市集出门动画（完成，V2 已直接刷入自测）：市集入口改走各设备已有探索
出门流程，房间走向门口／主从穿门／渐隐至黑／市集渐显；临时 MARKET 目的地不进入
持久化地区数组，保留免费且不更新 lastExploredAt／探索计数，渐显期间暂停市集移动。
三家族真实应用 ASan/UBSan、原六组出门旅程、探索费用适配、架构13／家族10／调度2／
微雪源码5项与 StickS3 release／V1、V2 lite Debug／谷仓 Debug/trace 构建通过。
V2 同 MAC28:84:85:55:56:90，只写 OTA0 0x20000，无备份；写入与独立 digest 校验通过。
app2138208B（较前版增加112B），SHA256
65019f7299e2be7a20aa59064e3788ed24f5e70625b86df8ff6dbb311b9c0006；资源完全不变。
串口注入正常点击／摇杆验证房间出门后进入市集、出口回房、再次出发中途取消；原51级
喷火龙、金币1774、队伍1／药水5、探索步数0与 CST820 校准正常，无 panic。
证据 .stick-local/artifacts/market-departure-20261104/；匹配固件
.stick-local/firmware/<目标>/market-departure-debug-20261104/。V1／谷仓／Stick 仅构建；
双精灵同行由真实 host 验证，本机存档为单只；实体触摸和面板观感仍待用户验收。

2026-11-04 市集文字对话（完成，V2已刷入自测）：微雪V1/V2与谷仓启用共享
MarketDialogue控制器及底框；锁定按下NPC及真实组合，收尾当前边段后转向，
逐字／点击补全／翻页／末页关闭，锁屏与微雪熄屏待机保留进度并取消接触。
冻结导航／游客预算／预约，独立环境时钟维持水面与花朵，关闭不补走且原RNG保持。
Stick不启用。origin_asset/text/market_dialogue.json维护24段通用＋4段专属、
15种NPC标签，整段选择与近期去重，未知物种通用、坏资源／短读／内存失败内置兜底。
tools/generate_market_dialogue.py生成3831B版本化CRC资源；微雪显式暂存并验证，
V2 OTA指纹含对白。补充字库6216B，合计10047B（谷仓索引另加200B），无新位图。
对白4项（ASan/UBSan、386物种三上下文、UTF-8分页、资源失败／释放）、市集13项、
三家族真实场景／组合接话／锁屏待机／输入隔离／圆屏安全区、架构13／家族10／
微雪源5项通过；Stick11张基准帧逐字节一致，摊主与组合正式渲染画面已看。
StickS3 release及LittleFS、V1/V2 lite Debug、谷仓Debug/trace构建通过。
谷仓补测后app3130352B，SHA256 13b81688acbf2697856468fd966f3a6c58af067f2b8c627d72b9bd005af52406，
3MiB槽余15376B；未改分区。证据.stick-local/artifacts/market-dialogue-20261104/，
匹配固件.stick-local/firmware/<目标>/market-dialogue[-debug]-20261104/。
用户后续授权后已刷入微雪V2（MAC28:84:85:55:56:90／16MiB），仅更新OTA0
0x20000及resources 0x620000。最终Debug app2138096B，SHA256
83f666ff10ecef5143579510904979d82eee2bec49c88afc84e3d080ddb04588；资源SHA256
3faac38b30d333227c0045300bf25a7f5c91de029d8b7df88df4d0246b538608。
低0x320000及旧资源已备份（非整片Flash）；两轮写入各自低128KiB启动前回读一致。
补测修正底部对话镜头超出静态缓存时的空白条，越界回退完整绘制，微雪／圆屏
缓存与直接绘制逐像素对照通过；四目标重新构建，Stick11帧仍逐字节一致。
V2真实应用经串口注入触摸验证变隐龙摊主、大嘴鸥＋长翅鸥实际组合接话、转向、
逐字／补全／翻页／关闭、地图／跨区拖动隔离、熄屏冻结与恢复、正常离场回房；
25份状态记录及3张校验通过的设备软件帧，重启后ELF、51级喷火龙存档、金币1774、
队伍1／药水5及CST820校准正常，串口已释放。实体触摸／面板观感与稳定FPS仍待人工验收。
证据.stick-local/artifacts/market-dialogue-flash-20261104/；匹配最终固件
.stick-local/firmware/amoled-v2/market-dialogue-device-debug-20261104/。
V1／谷仓本批仅构建、未刷入；文件系统资源设备升级需同步资源。
维护见doc/市集接入与调整指南-2026-11-02.md，规格specs/market-dialogue/。

2026-11-04 微雪 V1/V2 PWR 短按改为保存后熄屏待机，再按恢复完整页面；
面板 display-off/on，暂停绘制/传输/音频/孵化 IMU，后台照料与孵化每秒更新，
PMU 每250ms轮询，触摸不唤醒。原呼吸锁屏保留，亮度/音量不变；非房间交互页
保留状态。共享实际应用一小时照料/回血/经验/跨日/孵化完成、失败保存、调度回绕
及原锁屏/显示/架构回归通过，V1/V2 Lite Debug构建通过。最终V2应用3307dd9abe50、
资源2e453e29f3a2已刷入MAC28:84:85:55:56:90并校验；低128KiB含NVS/OTA元数据
回读逐字节保留。diag standby复用PWR分支连续3轮成功，恢复耗时约130ms（另有
最多250ms按键轮询），无重启，原51级喷火龙存档正常。实体PWR/观感待用户确认，
电池侧耗电未测，USB调试保护仍阻止自动浅睡；不能据此宣称续航比例或面板深睡。
文档doc/微雪熄屏待机.md，证据.stick-local/artifacts/amoled-display-standby-20261104/。

2026-11-03 巨钳螳螂方向资源修正（未刷机）：212原图行序为front/left/back/up_left/down_left，
此前套用通用front/down_left/left/up_left/back，导致三个源方向与右侧镜像错误。
已设独立source_directions，重提取15张待机/行走帧并仅更新212.smonsp与对应预览；
正面/左后方/睡眠像素和既有Kind编号保持。15帧按元数据原固定SpriteCollab revision
重新配准影子，IoU均>=0.99，移除15项失效手工校准；其他物种包及影子记录逐项不变。
新增原图/提取/实际包解码回归在修复前15帧失败，修复后全部23项资源测试通过；
已看八方向待机/行走预览，AMOLED V1/V2 lite Debug、StickS3 Debug和谷仓Debug构建通过。
证据.stick-local/artifacts/scizor-direction-20261103/；匹配微雪/谷仓Debug组件在
.stick-local/firmware/<目标>/scizor-direction-debug-20261103/。未访问设备或刷机；
文件系统资源设备需同步更新精灵资源包，单刷app不能安装本次修正。

更新日期：2026-11-04。

StickMon 是运行在 ESP32-S3 掌机上的电子宠物游戏。同一仓库维护
M5StickS3、AMOLED 1.8 V1、AMOLED 1.8 V2，以及独立 SDGOODS 谷仓实验目标；
OSTB 3ST 自 2026-10-30 起暂停适配，仅保留本地文件，不再提交上传。玩法、存档、
资源格式和 ESP-NOW 协议必须共用，硬件差异只留在平台层与表现层。

## 开发原则

- 用户于 2026-11-04 明确要求：后续刷入前不再备份，直接刷入。核对设备与目标后
  更新所需分区并做写入校验，保留存档和触摸校准；不再默认读取旧固件、资源或 NVS 备份。
- 修改前先读工作区状态。仓库经常有用户未提交的功能，不得回退、覆盖或
  顺手格式化无关文件。
- 优先修改 `src/game/` 的共享规则或已有共享服务，不在场景或 AMOLED 工程
  中复制一份业务逻辑。
- StickS3 与 AMOLED 使用不同 UI 和输入状态机，但相同操作必须得到相同的
  GameState 结果。
- 当前 AMOLED 迁移仍在开发中：已通过编译不等于通过实机验收，尤其不能把
  V2、低功耗、音频和 ESP-NOW 联调描述为稳定能力。
- 项目默认使用 ASCII 代码与注释；已有中文 UI、文档和资源名称可保持中文。
- OSTB 的 `firmware/ostb_3st/`、`specs/ostb-3st-port/`、`tools/ostb_native_host.cpp`
  与 `tools/test_ostb_native.py` 已加入 `.gitignore` 并取消 Git 跟踪，后续不主动适配、
  构建或刷机，不强制添加这些文件。历史 OSTB 验收记录仅作参考；谷仓仍依赖的
  `src/ui/compact_touch/` 及其他共享代码继续版本管理，不能整体忽略或取消跟踪。

## 本地产物管理

- 瀑布生成必须遵守 `doc/essentials_map_analysis/Essentials地图生成规则.md` 中
  “2026-10-24 补充：完整上下游瀑布的落水衔接”：承接潭覆盖泡沫行及两侧，
  泡沫下一行保持开放水面，禁止出现横向干岸；保留高度和 8 帧动画语义。
  该补充来自 A/B/C 多房间桥洞预览，现已接入运行时 v24；原 v23 四图保留。
  峡谷两岸采用用户确认的 v6/v7 预览规则：画面左岸 `Caves:590`、右岸 `588`，
  放在 Layer 1，Layer 0 保留洞穴地面 `610`，透明部分朝外侧干地；不镜像、不旋转，
  不改成水面衬底或整片 `513` 平台。`Outside:1136/1138` 是独立细水岸，不能代替岩壁。
  保留原 A/B/C 结构、上游桥和下游封闭承接潭，不照搬 119 号道路的水路拓扑。
  `512/514` 仍适用于其原岩岛模块，不是本峡谷的岸壁；详见地图生成规则与
  `doc/essentials_map_analysis/Caves地形块语义.md`。v7 外观及 v8 随机配方已接入运行时 v24。

- 所有需要保留的临时文件、验收截图/导出帧、bug 排查日志、性能记录、冻结源码、
  临时脚本、设备备份和预览统一写入仓库根目录 `.stick-local/`，该目录由 Git 忽略。
- 验收按 `.stick-local/artifacts/<任务名>-<YYYYMMDD>/` 分组，源码快照放该任务的
  `before/` 或 `frozen/`，截图和日志与其同组。独立采集日志用 `logs/`，临时预览用
  `previews/`，设备备份用 `backups/`，零散试验用 `scratch/`，待清理资源用
  `unused-resources/`，手动固件导出用 `firmware/<目标>/`。
- 新工具的临时产物默认使用上述目录；手动命令显式指定输出或重定向日志时也遵守。
  不再向 `tools/out/`、根 `out/`/`output/` 或源码/文档目录散落验收产物，
  不对本地产物使用 `git add -f`；提交前检查暂存区。
- 真正参与构建/运行的资源、可维护的工具和经审查的 `tools/fixtures/` 回归基线继续
  版本管理，不能为了清理而移入本地目录。基线来源记录可引用本地证据，但日常测试
  不得依赖只有本机持有的快照；不得靠重生成像素/hash 基线掩盖回归。
- 构建系统管理的 `.pio/`、`build/`、`firmware/**/build*/` 和自动清理的
  `TemporaryDirectory`/系统缓存沿用原约定；手工保留的排查或验收文件仍必须集中。
- `build/` 只保留实际构建产物，不存放人工 `before/after` 对照目录或验收日志。
  OSTB 原生测试程序/对象文件仍在 `build/ostb-native/`，导出帧默认写入
  `.stick-local/artifacts/ostb-native-<YYYYMMDD>/`，可用 `--output-dir` 覆盖。
- 旧 `tools/out/` 已迁至 `.stick-local/artifacts/`，旧资源隔离区已迁至
  `.stick-local/unused-resources/`。详细规则见 `doc/本地产物管理规范.md`。
- 2026-10-26 的旧固件/Claw 构建缓存已归档到
  `.stick-local/unused-resources/2026-10-26/firmware/`；需要再构建时由原脚本重建。
  V2 发布包与签名统一输出到 `.stick-local/firmware/amoled-v2/<构建目录名>/`，
  打包与签名的 `--output-dir` 覆盖须一致，不再在固件 `build*/out/` 写导出包。

## 硬件与构建

### M5StickS3

- PlatformIO + Arduino-ESP32，240x135，M5Unified/M5GFX。
- 8 MB Flash，LittleFS 资源，Preferences/NVS 存档；运行时优先使用 PSRAM。
- `m5stick-s3` 是 release：关闭 Debug 菜单、trace 和渲染统计。
- `m5stick-s3-debug` 开启 Debug 菜单与 trace 日志。
- 构建：`pio run -e m5stick-s3` 或 `pio run -e m5stick-s3-debug`。
- 固件和 LittleFS 一起刷入：`bash tools/upload_firmware_and_fs.sh`。
- 只刷固件或资源分别使用 `pio run -e m5stick-s3 -t upload`、
  `pio run -e m5stick-s3 -t uploadfs`。
- 若上传口被占用，先检查串口监视器、Chrome Web Serial 等进程；不要用提高
  上传速度掩盖连接问题，当前上传速度有意固定为 115200。

### AMOLED 1.8 V1

- 工程：`firmware/amoled_1_8_v1`，ESP-IDF 5.5.x。
- SH8601 + FT3168，当前画布与触摸均为原生 368x448 RGB565；旧素材按资源规则缩放。
- 使用 SPIFFS 资源、PSRAM、NVS A/B 存档和共享 ESP-NOW 协议。
- 构建：在工程目录加载 ESP-IDF 后执行 `idf.py -B build build`。
- 刷入：`./flash.sh --port PORT`；应用、bootloader、分区表和 resources.bin
  必须来自同一 build 目录。

### AMOLED 1.8 V2

- 工程：`firmware/amoled_1_8_v2`，ESP-IDF 5.5.x。
- CO5300 + CST820，物理/逻辑分辨率与 V1 相同。
- 应用与渲染复用 V1 的 `AmoledApp`、`HomeScreen`；V2 只保留 `main.cpp`、
  `AmoledPlatform.*`、`TouchInput.*` 等板级差异。
- 构建：`idf.py set-target esp32s3 && idf.py build`。
- 当前状态是可编译、待 V2 实机验收，不能凭 V1 结果推断 V2 硬件正确。

### OSTB 3ST（暂停，仅本地）

- 工程：`firmware/ostb_3st`，ESP-IDF 5.5.x；这是独立设备，不是微雪。
- ESP32-S3，16MiB Flash、8MiB octal PSRAM 已实测；NV3023 原生横屏 296x240，
  offset (24,0)、MADCTL 0xe8，RGB565 framebuffer + DMA 分条提交，不使用 LVGL。
- 房间使用与 Stick 相同的 1 倍比例，完整 240x161 居中于 296x240 屏幕，
  原点 (28,39)，黑色留白；`STICKMON_ROOM_PIXEL_SCALE=1`，镜头固定。
  精灵、影子、食物和蛋均保持 Stick 像素尺寸；房间图与世界锚点共用居中偏移，
  房间图层裁切到其矩形，不能把边缘像素拉满屏幕。影子复用
  `PokemonShadow::atGround`，帧内偏移不能重复投影。时间与一/两只精灵的
  状态栏置于上方留白，操作提示在下方；菜单等其他场景仍使用完整屏幕。
  玩法坐标和存档不变。之前 2 倍原图跟随版已被此居中方案替代；
  当前居中版本已通过 host 检查，待独立实机观感确认。
- 左 A=GPIO40、右 B=GPIO39，低电平按下；中键=GPIO1，高电平按下且禁用内部
  上下拉。中键来自本机原厂固件反汇编并已实测，不能使用公开参考板的 GPIO0。
- 屏幕触摸已接入：共用音频 I2C SDA12/SCL11，0x15 实机应答、ID=0xb8；
  使用 CST816 兼容寄存器轮询，不猜测 INT/RST 引脚。首次五点校准（四角拟合、
  中心独立验证），B 跳过，开机按住 A+B 重做；校准单独存于 ostb_touch/cal_v1。
  已实现主精灵/食盆/蛋及菜单点击。主菜单共享 `MenuGrid` 图标网格，按宽度排列：
  OSTB 三列 88x72 卡片，八项一屏；AMOLED 保留两列。背包/商店使用可拖动的
  自适应网格，点条目进详情，独立使用/购买/出售按钮，背包保留原确认弹窗；
  房间/食物/电脑入口和设置采用圆形返回键与卡片/列表，食物和设置可滚动；
  亮度/音量支持横向滑杆，帮助可翻页，重置使用独立确认页、默认取消。
  电脑内的通讯/伙伴详情等未改子页继续保留返回/选择/确认。
  松手确认，拖动/锁屏/按键/切场景取消；
  最后一件道具用完/卖完后退回网格，静态页不持续刷新。无触摸设备继续用 A/B。
  校准使用最近五次采样中值，失败有提示、中心可重试；实机中心误差 2.60px、
  saved=1，用户已确认摸头/食盆/首版菜单/底部导航均正常。新网格版已通过 host、
  导出画面检查及 OSTB/StickS3/AMOLED V1/V2 lite 编译；用户已确认三列菜单、
  背包/商店详情返回及滑动均正常。应用更新后已实测加载原触摸校准与存档。
  房间/食物/设置/电脑入口首批子页已通过 host、导出画面及四目标构建，
  app-only 更新启动正常、原校准和存档加载成功；用户已确认本批显示、
  点击/滑动、亮度/音量滑杆、帮助翻页及电脑入口返回均正常。
  队伍列表已改为微雪式精灵卡片与操作弹窗，点击外部关闭且不穿透；
  首位提供状态/技能，其他正式成员按上下文增加首位/离队；访客不提供首位/离队，
  探索队伍不提供离队。用户明确要求状态/技能只加微雪式顶部返回栏，
  保留原内容排布，不重做页面；状态/技能均去掉底部选择/确认及翻页栏。
  状态保留原五页，左右滑动或点顶部标题翻页，上下拖动超出内容；
  指示器按最新反馈改为底部居中横排、覆盖在内容之上，不额外占正文高度；
  技能保留 90px 左列、右侧详情/熟练条和紧凑确认窗；按最新要求，技能点击区改为
  44px 高、间隔 6px，移除左侧返回项，仅保留顶部返回。描述自动换行，不再横向跑马灯；
  一屏放不下时仅描述区域可上下滑动，切换技能回到描述顶部。
  点条目查看、再点当前条目执行，遗忘仍弹窗确认且默认 No。
  最新用户要求仅顶部导航增加左右 28px 边距，正文保持完整 296px 宽；
  `TouchUi::safeHeaderBounds/safeBack` 只约束顶部，`pageBody` 为 (0,48,296,176)。
  顶部边距按用户最新要求从 16px 改为 4px，导航栏高 48px，正文同步上移 12px；
  队伍卡片为 280x76、普通 8px 边距；正文/弹窗/提示与点击不得继承顶部内缩，
  两侧可见内容必须正常响应；非触摸页保持原渲染。
  队伍改动已通过 host、导出图、四目标构建及 app-only 启动检查；用户已确认
  卡片/弹窗/外部关闭和原状态/技能页返回均正常。首位/离队状态变化有 host 覆盖。
  后续实拍发现屏幕大圆角会遮挡返回键/标题；菜单功能通过不代表圆角布局通过。
  安全区方案见 `specs/ostb-3st-port/safe-area.md`，暂定 R56+6px 余量，
  顶部关键内容范围 (28,4)-(268,44)，不作为正文的安全矩形；R56 仍未实测。
  最新要求已先实施队伍页面组并通过 host/遮罩预览/四目标构建；app-only 更新
  上一版 14e70afbe 被用户要求还原排布，当前按“原内容+顶部导航”修正，
  无底部按钮，原内容像素对比及四目标构建通过；app-only 更新 299a93cdb
  已加载原校准/存档并完成首帧，但用户拒绝正文 240px 内缩；
  当前按“顶部留边距、正文全宽”修正，host/遮罩图/四目标构建通过；
  全宽版 9b54722cf 后，顶部 4px 版 0946a8983 已通过 host/四目标构建、
  遮罩图与 app-only 启动检查，原校准/存档正常加载，实机观感待确认；
  2026-10-22 后状态页单独使用 (0,48,296,192)，正文背景延伸到屏幕底部；
  技能／队伍仍使用原 pageBody。状态切页为约 220ms 缓出水平滑动，旧页保留
  滚动位置、新页回顶，顶部导航和底部页点固定覆盖；结束／取消／离页释放
  PSRAM 临时图，静止后停止刷新。房间菜单复用队伍安全顶部导航，卡片
  (8,52,280,80)、(8,138,280,80)，返回圆心 (48,24)；食物／电脑暂不调整。
  本批 host／预览和四目标构建通过，尚未刷入设备，实机观感待确认。
  商店／背包后续按微雪当前样式改为日常／探索分组图标网格：背包三列，商店
  固定 64px 购买／出售侧栏、右侧两列；点图标进详情，左操作右返回，背包保留
  原使用确认。`ItemTouchUi::Grid` 共用绘制／点击／滚动／物理键 reveal 的几何，
  背包视觉顺序映射原源索引，商店购买两组映射 ShopService 分类索引，物品可见性
  和玩法不变。顶部复用队伍 safe header，金币位于返回与标题之间，正文不内缩；
  操作按钮为 (28,192,112,36)、(156,192,112,36)。host／预览／四目标构建通过，
  已 app-only 刷入 7973898ef：哈希验证通过，原校准和已孵化存档加载正常，
  位置仍为 (76,81)，2660ms 完成首帧；包含上批状态切页动画及房间导航，
  实机圆角、动画和操作观感待验收。随后状态页改为跟手预览：超过 10px 锁定
  横／纵方向，水平位移与手指一致，松手达到 74px 切页，否则 80～220ms 回弹；
  逻辑页在松手确认后才变化，回弹保留原滚动，可拖动中反向，复用临时缓冲。
  按住不动不定时刷新，完成／取消／离页释放；内存失败退化为松手切页。
  指示点下移 10px 至 y=232，胶囊 (122,226,52,14)，仍覆盖正文不占页脚。
  精确像素位移／反向／回弹／轴向隔离等 host、预览和四目标构建通过，
  本跟手版本已 app-only 刷入 2de9c19f8，哈希验证通过；1137ms 加载原校准，
  2152ms 加载已孵化存档，2510ms 完成首帧，启动未见异常；跟手／回弹与
  指示器实机观感待验收。其他页面的圆角适配继续暂缓。
  2026-10-23 电脑入口及设置（含帮助／重置确认）已统一复用队伍 safe header：
  顶部左右 28px、上方 4px、48px 栏高、返回圆心 (48,24)，点击区同步；
  列表从 y=52 开始，正文仍保持全宽。
  食物选择页现也复用同一队伍顶部导航，返回圆心 (48,24)，列表从 y=52 开始，
  绘制／命中／滚动／物理键 reveal 共用新布局。洗澡点肥皂打开三种皂的库存卡片
  弹窗，点有库存卡片直接消耗并进入擦洗，取消原 USE；缺货不可选，关闭／点外部／
  B 长按取消不消耗，弹窗阻断底层工具。
  蛋/商店/设置点击有 host 覆盖；洗澡按住拖动、探索地区列表点击／跟手拖动、地图点击行进与战斗命令点击已接入。
- 中键短按锁屏/恢复；长按由硬件切断电源，完全关机后也要长按开机。
  软件须在按下去抖完成时保存，不能等松手后才保存；保持按下期间暂停游戏，
  按住暂停及断电后精灵/坐标恢复已实测。软件深睡兜底尚未实测。
- 游戏使用独立 `game_nvs`（0x700000、64KiB），资源使用 8MiB SPIFFS。
  原厂 NVS 保留；已初始化设备升级不得重复擦除游戏分区。原厂完整备份留在仓库外。
- 无已验证 IMU，洗澡选工具后按住精灵区域拖动擦洗，松手或取消即停。主界面时间后使用共享微雪电池图标；
  电量按本机原厂 2.2.2 的 ADC2 CH6/GPIO17、12dB/12-bit 和原始查表估算，
  八次采样平均、后续 80/20 平滑、10 秒缓存，失败显示未知，不借用微雪 AXP2101。
  49f840171 已 app-only 刷入，实机三次原始采样 2456、按原厂曲线估算 94%；
  host/微雪电池回归/遮罩图/四目标构建通过，原存档/校准加载正常。
  底部页点与电池图标实机观感待确认；充电状态、完整放电校准和射频共存仍未验收。
  ES8311 初始化已验证，声音听感和跨设备 ESP-NOW 仍待验收。
- 加载 IDF 后在工程目录运行 `idf.py build`；升级只写 app 可保留存档与资源。
  Debug 更新使用 `bash firmware/ostb_3st/flash_debug.sh --port PORT`：独立
  `build-debug` 开启 Debug 菜单与 trace，只写 app；加 `--dry-run` 只构建验证，
  不访问串口。普通构建默认关闭两项调试开关。
  原生渲染/输入验证：`python3 tools/test_ostb_native.py`；DMA 验证：
  `python3 tools/test_rgb565_transfer.py`。详情见目标 README 与 `specs/ostb-3st-port/`。

### SDGOODS 谷仓（独立固件实验目标）

- 工程 `firmware/sdgoods`，ESP-IDF 5.5.x；默认引用同级 `sdgoods-esp32s3` SDK。
  构建 `bash firmware/sdgoods/build.sh -B build-native`，脚本只构建、不访问串口。
- 360×360 ST77916 QSPI，CST816 触摸。无 LVGL：PSRAM RGB565 framebuffer +
  共用 Rgb565Transfer + 两块 8 行内部 DMA 缓冲；引用原 SDK 引脚、底层驱动和 vendor
  表，不修改 SDK。圆屏主房间、主菜单／商店／背包／电脑／队伍／房间入口／伙伴／设置／状态／食物／孵化／社交与表面适配留在 `src/ui/round_touch/`；
  完整菜单复用 compact_touch 共享视图及原业务服务。主房间／孵化保持 360×360；
  主菜单、商店、状态与技能页、伙伴列表、探索选择／地图／战斗使用 360×360，其余菜单子页及洗澡／社交采用中央 (60,60) 的 240×240 原生像素 PSRAM 表面，
  不缩放整帧。PageProfile 提供页面几何；SceneSurface 负责组合和触摸映射，输入期绑定
  页面缓冲以支持状态页跟手缓存。移出页面取消触摸，分配失败拒绝页面输入并显示错误。
- 当前（2026-11-09）物理 Flash 仍为 32 MiB；自有 `firmware/sdgoods/partitions.csv`
  的地址范围限制在 16 MiB：双 3 MiB app、单 9 MiB 只读 LittleFS、768 KiB 预留 userdata。
  布局 `sdgoods-16m-single-v1`；两个 app 共用 resources，绑定在独立 resmeta，49 张 SMCD 表外置。
  `userdata` 不挂载、不格式化，改址不迁移旧 userdata。原 NVS 的存档／校准不变。
  构建后用 `tools/sdgoods_release.py package / inspect / install`；Release 必须签名。
  不用 `idf.py flash`、SDK 脚本、生成 flash_args 或 app-only。install 接受精确旧表、
  显式 MAC／32 MiB 校验，写入及擦除上限 16 MiB，不写原 NVS／PHY／userdata。
  `factory --output PATH` 生成 16 MiB 连续首装镜像（间隙 0xFF，整包写入会清除 NVS）；
  有存档设备使用定点 install。应用内 USB update 明确关闭，不提供共享资源回退。
  原 NVS 上用 `sg_` 前缀
  隔离命名空间，保持共享 schema / A/B / CRC。空档进入共享正式孵化，不跳过教程。
  有效未孵化旧档按共享流程恢复计时和照料次数（完整菜单已支持重置／继续孵化）；
  不可恢复坏档／更新版本档保留并停止进入游戏；保存失败不得重启。
- 主机检查 `python3 tools/test_sdgoods_room.py`：真实触摸、饥饿 fixture 的 AI 进食、
  跨进程恢复和错误路径；初始饱腹仍为共享默认 75，高于主动进食阈值 70。
  2026-10-25 已刷入原生 app（1,850,544 B，SHA256 前缀 d0f8748622），写入哈希通过。
  用户确认显示与触摸正常；USB 复位后恢复 species=1、stock=1、bowl=1、bites=3，
  两次启动保存均成功。TE／帧率／触摸精度／物理掉电仍待专项验收。
  原始低地址备份和实机证据见 `.stick-local/` 对应 sdgoods-native-flash-20261025 目录。
  同日后续已接入圆屏主菜单／食物选择：房间底部菜单入口，7 种食物库存列表与滑动，
  返回／取消不穿透，选择只改共享 selectedFood 并及时保存；顶部／内容／提示复用 RoundPage。
  ASan/UBSan、跨进程恢复、静态停绘、原菜单／食物页回归与四目标构建通过。
  新 app 1,825,248 B（SHA256 前缀 72d69f25d3）已 app-only 刷入 OTA0、哈希通过，
  原存档恢复成功，首帧计时 530ms；用户已确认新菜单／食物页显示、滑动和返回操作正常。证据见
  `.stick-local/artifacts/sdgoods-round-menu-20261025/`，更新前备份在对应 backups 目录。
  同日完整菜单已覆盖探索／战斗、队伍／状态／技能、房间／食物／洗澡、背包、商店、
  电脑／联系人／社交、设置／帮助／重置及孵化；音频、IMU、ESP-NOW 仍为平台桩。
  真实引擎 ASan/UBSan、完整触摸旅程及错误路径、跨进程／未孵化档恢复、页面回归和
  四目标构建通过。探索四配置各 36 检查点及全部像素与本批冻结源一致，旧 fixture 的
  每配置 12 张既有地图差异保留。新 app 1,852,768 B（SHA256 前缀 673f71311f）
  已仅更新同设备 OTA0、写入哈希通过；启动前低 128KiB 回读与备份一致，原分区／NVS／
  OTA 记录保留。启动恢复 species=1、stock=1、bowl=1、bites=3、satiety=66，保存成功，
  首帧计时 523ms，20 秒采集未见异常。用户已确认主菜单滑动、状态左右翻页、商店、
  设置滑杆及洗澡返回的显示／触摸正常；重置／孵化仅主机验收。
  补充孵化／公共叠层回归中 Stick 原基线通过，OSTB release／debug 分别保留
  24／40 张旧菜单叠层差异；648／672 帧及真实旅程与本批冻结源一致，未改旧 fixture。
  本批规范见 `specs/sdgoods-full-menu/`，证据和备份见 `.stick-local/` 对应
  `sdgoods-full-menu-20261025` 目录。
  同日按用户最新要求移除 Save／Restart 按钮及主菜单项，仅保留七个功能入口与返回；
  用户确认不移除设置中的“重置游戏”。删除闲置 UI 保存／重启请求通道，自动保存和
  独立实体键保存沿用原流程。圆屏真实引擎 ASan/UBSan 完整旅程、跨进程恢复、失败保护、
  返回取消及家族源检查通过，谷仓原生固件构建通过。app 1,852,432 B，SHA256 前缀
  a6a8200444；已 app-only 刷入同设备 OTA0、写入哈希通过，启动前低 128KiB 元数据
  与备份一致。启动恢复 species=1、stock=1、bowl=1、bites=3，保存成功，首帧计时
  502ms，20 秒采集未见异常；精简版界面尚未另行实机确认。证据见
  `.stick-local/artifacts/sdgoods-hide-save-restart-20261025/`。
  2026-10-25 硬件补齐：Audio 三通道混音／按需 I2S1 麦克风、Battery ADC1 CH7、
  common QMI8658 standby／按需采样、PeerRadio ESP-NOW 频道 6／RAM Wi-Fi 设置、
  电源按下保存／按住暂停／短按锁屏／长按保存成功才关机已实现。输入由应用在引擎更新前
  pollInputs，避免硬件切电前漏保存；UI 成功 Saved 已移除，错误反馈保留。
  用户明确要求保留实际设备音量 0，不试听、不上调；洗澡始终触摸拖动，Claw／微信排除。
  短锁屏保留社交链路，深睡／关机才关射频；省电配置仍是共享闲置调暗，不等于自动深睡。
  电池百分比为 3.3–4.2V 线性估算，非原厂放电曲线；ADC／校准错误显示未知。
  本批 app 2,408,976 B（SHA256 前缀 592ea0a29a）已只更新同设备 OTA0，写入哈希通过；
  启动前低 128KiB 回读一致，原存档恢复，音量全程 0／PA=OFF，IMU／ADC 初始化成功，
  首帧 523ms。假硬件／真实引擎 ASan/UBSan、相关页面像素与原 OSTB native 回归通过。
  用户已确认本版显示、触摸与短按锁屏／恢复正常；洗澡保持触摸拖动。
  通讯搜索、双机互联、长按关机、录音听感、放电校准与物理断电待实测，不能把 host
  结果描述为实机通过。证据见 sdgoods-hardware-20261025。
  2026-10-26 电量／正式首次开机：官方端点 3000–4280mV、四舍五入并限幅，替代首版
  3.3–4.2V；八次采样至少六次有效、去最高最低、80/20 平滑，失败未知且 2 秒重试，
  正常 10 秒缓存。ADC／校准幂等初始化并支持失败重试。房间／孵化显示百分比和 <20% 低电文字，
  SDK 无充电状态信号，不推测充电中，放电曲线仍待实测。空档恢复 180 秒共享孵化，
  不提前发物资或预完成教程；圆屏房间／菜单／探索／战斗有真实触摸提示，帮助文案同步。
  场景保存钩子改为 bool，孵化立即保存计时和照料次数，失败传播到保存／关机保护；schema 不变。
  独立空档／跨进程恢复／八步引导及完整菜单 ASan/UBSan、实际 Battery 假 ADC、存档／架构／
  调度检查通过。已有实机档必须保留，不用重置游戏验收首次启动，音量保持用户设定 0。
  孵化旧像素差异与冻结源 648／672 帧一致；探索旧测试在 ENDING 断言失败已在修改前源复现，
  不更改旧 fixture，不描述整仓测试全通过。详见 `specs/sdgoods-hardware/onboarding-acceptance.md`。
  本版 app 2,520,704 B、SHA256 前缀 12e89f337b 已只更新同设备 OTA0，写入哈希通过；
  更新前备份低 0x280000 完整覆盖旧应用，启动前低 128KiB 回读一致。原档恢复 species=1、
  stock=1 bowl=0 bites=0 satiety=68，音量全程 0／PA=OFF，电池 4365mV／100%，首帧 562ms。
  25 秒采集未见异常；用户已确认本版显示和触摸正常。首次孵化未重置实机档，独立 host 验证。
  当前产物用 `build-native/`，早期 LVGL `build/` 缓存已迁入 `.stick-local/unused-resources/2026-10-26/firmware/`。说明见目标 README。
  2026-10-26 主房间布局调整：移除底部加食物按钮和命中，直接点食盆喂食；新手提示为
  “点击食碗喂食”（现有字库支持）。底部仅留微雪风格 56×56 圆角三横线菜单，
  矩形 (152,286,56,56)；孵化原菜单几何保持。顶部电池图标 y=14，无百分比；
  下方依次为时间 y=32、HP y=54、饱腹度 y=76，数值与插值沿用共享规则。
  主房间错误保存提示位于 y=264。真实引擎 ASan/UBSan、完整触摸旅程／首次流程／
  跨进程恢复、家族源隔离和 Stick／OSTB release／debug 主房间像素回归、谷仓构建通过。
  app 2,520,736 B、SHA256 前缀 37bcacb36b 已只更新同设备 OTA0，写入哈希及启动前
  低 128KiB 回读通过；本批低 0x2a0000 备份完整覆盖上一版 app。启动恢复原档，
  音量 0／PA=OFF，首帧 474ms；布局实机观感待确认。证据见 sdgoods-home-layout-20261026。
  同日按新反馈收紧 HUD：时间 (147,21) 与后方电池图标 (195,25) 同行居中；
  精灵状态改为微雪原鸡腿图标 2 倍和 90×12 HP 条，首行 y=42、最多两行、行距 28，
  饱腹动画和绿／黄／红 HP 色阶沿用原语义。菜单缩为 38×38，矩形 (161,295,38,38)，
  绘制／命中同步。仅修改 round_touch，未改资源或共享规则；谷仓 ASan/UBSan 完整
  旅程、家族源检查与构建通过，导出图已检查。app 2,521,632 B、SHA256 前缀 97d98cda53
  已只更新同设备 OTA0，哈希／启动前低 128KiB 回读一致，原档恢复、音量 0／PA=OFF、
  首帧 457ms；本次布局实机观感待确认。证据见 sdgoods-home-hud-20261026。
  同日时间按用户要求放大为微雪原生 32px 字体，仅在时间绘制时启用 nativeText 后恢复；
  时间 (127,8) 与电池 (215,20) 合并居中。已核对微雪鸡腿为 28×28、HP 条 90×12，
  当前尺寸保持一致，菜单仍为 38×38。本轮仅改 round_touch/HomeUi.cpp；谷仓完整旅程
  ASan/UBSan、家族源检查与构建通过，导出图已看。app 2,524,032 B、SHA256 前缀
  1b2ff50d0c 已只更新同设备 OTA0，写入哈希与启动前低 128KiB 回读通过；原档恢复，
  音量 0／PA=OFF，首帧 589ms。构建包含当前工作区既有资源更新，本轮未编辑资源。
  实机大字观感待确认；证据见 sdgoods-home-clock-20261026。
  同日最新尺寸按用户指定改为时间 20px、鸡腿 14×14、HP 条 50×8；时间仅将现有
  五个 ASCII 字符格单独采样为 10×20，使用 1280 B 栈缓冲，不改全局字体状态。
  时间 (142,14)、电池 (200,20)，状态首行鸡腿 (144,42)、HP (166,45)，行距 20；
  菜单仍 38×38。仅改 round_touch/HomeUi.cpp；谷仓 ASan/UBSan 完整旅程、家族源
  检查及构建通过，导出图已看。app 2,524,256 B、SHA256 前缀 cab1d35a69 已只更新
  同设备 OTA0，写入哈希／启动前低 128KiB 回读通过；原档恢复、音量 0／PA=OFF，
  首帧 478ms。实机观感待确认；证据见 sdgoods-home-compact-20261026。
  同日 HP 条增加轻圆角：50×8 外框／底色半径 2、内部填充半径 1，其余布局保持。
  谷仓完整旅程 ASan/UBSan、构建和导出图检查通过；app 2,524,256 B、SHA256 前缀
  8871476ba1 已只更新同设备 OTA0，写入哈希／启动前低 128KiB 回读通过，原档恢复、
  音量 0／PA=OFF、首帧 489ms；观感待确认。证据见 sdgoods-hp-rounded-20261026。
  同日双精灵状态按用户要求改为横向：每组宽 72、组间 16，两组鸡腿 x=100／188、
  HP x=122／210，统一鸡腿 y=42、HP y=45；单只仍 x=144／166 居中。时间 20px、
  鸡腿 14×14、HP 50×8 轻圆角及菜单 38×38 保持。host 增加独立 hud-pair 双精灵
  画面 fixture，完整 ASan/UBSan／家族源／谷仓构建和单／双只导出图检查通过。
  app 2,524,288 B、SHA256 前缀 54457be19f 已只更新同设备 OTA0，写入哈希／启动前
  低 128KiB 回读通过；原档恢复、音量 0／PA=OFF、首帧 615ms。实机横排观感待确认，
  未为双精灵画面更改实机队伍。证据见 sdgoods-home-pair-20261026。
  2026-10-28 主菜单单独改为 360×360 原生表面：顶部返回＋菜单固定居中，双列
  108×72 卡片的视口 (60,52,240,308) 延伸至底部；中间两行原尺寸，上下边缘
  平滑收缩至最小约 88%，图标／文字整体采样、命中同步，末排可完整滚入中央。
  15,552 B PSRAM 卡片缓冲失败退化为原尺寸；子页及探索内队伍／背包仍用 240×240。
  真实引擎 ASan/UBSan 完整旅程、新缩放／边缘取消／静态停绘／内存降级检查，
  家族源／架构／调度与谷仓、Stick release、AMOLED V1 lite 构建通过；七张主要子页
  与上一批像素一致。app 2,534,256 B、SHA256 前缀 0a43a1e653；未访问串口、未刷机，
  实机手感及性能待验收。证据见 .stick-local/artifacts/sdgoods-main-menu-20261028。
  同日用户授权刷入后已只写同设备 OTA0 0x20000，哈希通过，启动前低 128KiB
  回读与本次备份一致。备份发现设备中间曾写 tama_meets 于 0x10000；原引导层／
  分区表和有效 OTA0 记录仍保留，核对实际布局后更新，未覆盖存档或 OTA 元数据。
  启动恢复原已孵化档，species=1、stock=4、bowl=1、bites=3、satiety=63；音量 0／
  PA=OFF，首帧 506ms、精灵就绪 592ms，25 秒未见启动异常。主菜单实机观感待确认。
  同日用户反馈缩放效果不好，已移除边缘缩放与 15,552 B 卡片缓冲；所有卡片在
  滚动中保持 108×72，顶部导航、视口及末排滚动余量保留。真实引擎 ASan/UBSan
  完整旅程、固定尺寸命中／拖动／返回／停绘检查、家族源和谷仓构建通过，七张子页
  与上一版像素一致。app 2,533,568 B、SHA256 前缀 5f3a18be2d 已仅更新同设备
  OTA0，哈希及启动前低 128KiB 回读通过，原档恢复、音量 0／PA=OFF、首帧 506ms。
  证据见 .stick-local/artifacts/sdgoods-menu-fixed-size-20261028；固定尺寸实机观感待确认。

  2026-10-28 商店改为独立 360×360 round_touch 视图，顶部复用主菜单返回＋标题；
  左侧购买／出售以 200ms 缓出沿圆弧切换，未选项缩至 75% 并变灰，金币固定左下。
  右侧双列商品不缩放，购买／出售分别记住滚动位置，详情和交易复用 ShopUiBase／ShopService。
  动画中阻断误点商品，取消落到目标状态，静止停绘。完整 ASan/UBSan 旅程、独立商店
  点击／动画／金币稳定／真实买卖／滚动恢复检查、家族源／架构／调度检查及谷仓、Stick
  release、AMOLED V1 lite 构建通过；76 张非商店导出帧与上一版逐字节一致。
  app 2,553,872 B、SHA256 前缀 a0c70c2142 已只更新同设备 OTA0，写入哈希通过，
  启动前低 128KiB 回读一致，原档恢复、音量 0／PA=OFF，首帧 519ms；25 秒未见启动异常。
  商店实机显示和切换手感待用户确认。证据见 .stick-local/artifacts/sdgoods-shop-arc-20261028。

  同日商店侧栏按反馈改为纯文字，无按钮底色／描边；圆弧角度从 30° 收至 20°，
  非选项中心位移约 65→44px。左侧文字区上滑切出售、下滑切购买，松手阈值 24px，
  横拖／短拖／取消／移出不切换；右侧只滚动商品，保持两模式独立滚动位置。
  金币固定矩形 (32,252,72,28)，上移 26px；不足万显示精确数值，万以上最多两位小数，
  达 100 万用整数万，统一向下截取／去尾零，点击显示精确金额 2 秒；不改交易数值。
  ASan/UBSan 完整旅程、手势隔离／取消、金额边界与字库、真实买卖／停绘、家族源／
  架构／调度检查和谷仓构建通过；76 张非商店导出帧与上一版逐字节一致。
  app 2,554,448 B、SHA256 前缀 5a8d005805 已只更新同设备 OTA0，写入哈希及启动前
  低 128KiB 回读一致；原档恢复、音量 0／PA=OFF，首帧 507ms、精灵就绪 593ms，
  25 秒未见启动异常。实机纯文字样式／滑动手感待确认。证据见
  .stick-local/artifacts/sdgoods-shop-text-20261028；固件对应 shop-text-20261028。
  2026-10-29 反馈选中颜色及滑动不灵敏后：商店购买／出售选中改纯白，未选中灰色；
  滑动起点区扩大到 (0,56,112,248)，起点拥有整次屏内手势，滑出起点区不再取消，
  保持商品区不穿透。松手阈值 24→16px，允许少量初始横向抖动，明显横拖／CANCEL
  不切换；右侧起滑仍只滚商品。真实引擎 ASan/UBSan 覆盖白／灰像素、长滑／扩大起点／
  跨区捕获／抖动／15–16px 边界及原完整旅程，家族源和谷仓构建通过；
  76 张非商店导出帧与上一版一致。
  app 2,554,336 B、SHA256 前缀 906de2353c 已仅更新同设备 OTA0，写入哈希／启动前
  低 128KiB 回读通过；原档恢复、音量 0／PA=OFF，首帧 501ms、精灵就绪 1599ms，
  25 秒未见启动异常，实机灵敏度待用户重试。证据对应 sdgoods-shop-swipe-20261028，
  固件对应 shop-swipe-20261028（任务始于 10 月 28 日，更新于 29 日完成）。
  同日按用户要求把商店灰色购买／出售点击区与文字几何分离：未选项扩为 112×56px，
  购买 (0,96,112,56)、出售 (0,196,112,56)，中间选中项／右侧商品／下方余额不重叠。
  文字尺寸、白灰颜色及滑动切换保持；圆屏外／动画中仍拒绝命中。真实引擎 ASan/UBSan
  增加文字外空白点击、取消及商品／余额边界检查，原完整旅程、家族源和谷仓构建通过；
  94 张导出帧与上一版逐字节一致。
  app 2,557,840 B、SHA256 前缀 cfb5fec039 已仅更新同设备 OTA0，写入哈希／启动前
  低 128KiB 回读通过；原档恢复、音量 0／PA=OFF，首帧 521ms、精灵就绪 607ms，
  25 秒未见启动异常。灰色项空白区点击实机手感待确认。证据对应 sdgoods-shop-hit-20261029，
  固件对应 shop-hit-20261029。

  2026-10-29 顶部“菜单／商店”使用原生 20px 更纱 A4 子集，侧栏购买／出售
  选中 20px、未选中 16px，停稳不再放大原 16px 点阵；具体格式和生成器见资源管线。
  真实引擎完整菜单 ASan/UBSan、字库生成确定性、覆盖率／字节序／裁切／坏字库回退，
  家族源／架构／调度、OSTB 原生检查及谷仓／Stick／OSTB／微雪 V1／V2 lite 构建通过。
  94 张画面中 69 张不变，25 张差异仅在顶部导航和商店侧栏文字；旧探索 ENDING 断言
  在改前字体代码上于 Stick／OSTB 均复现，未改基线。app 2,562,048 B、SHA256 前缀
  c49b592171 已只写同设备 OTA0，写入哈希和启动前低 128KiB 回读通过；原档恢复、
  音量 0／PA=OFF，首帧 518ms、精灵 604ms，25 秒无启动异常；字体实机观感待确认。
  证据对应 sdgoods-native-fonts-20261029，固件对应 native-fonts-20261029。

## 架构边界

- `src/game/`：纯玩法、数值和状态变更，不能依赖 Arduino、M5 或 ESP-IDF。
- `src/core/`：应用协调、存档、场景流程和跨平台会话。
- `src/platform/api/PlatformServices.h`：硬件服务边界。
- `src/platform/m5stick_s3/`：StickS3 平台实现。
- `src/scenes/`：应用场景协调；商店／背包／队伍／状态／技能／房间菜单／食物选择／电脑入口／通讯录／设置／主菜单、主房间、探索／战斗、洗澡、无线通讯、Debug、孵化与菜单公共叠层已委托家族界面；应用外层导航／资源提示／淡入淡出也已收口，
  不应在这里新增设备布局分支或复制可共享的业务规则。
- `src/core/features/`：商店／背包目录、队伍能力、技能目录／回忆流程、房间／食物和联系人能力只读查询与语义结果；不依赖绘制、输入或设备 SDK。
- `src/ui/stick/`、`src/ui/compact_touch/`：各家族的十一组菜单、主房间、探索／战斗、洗澡、无线通讯、Debug、成长、孵化及菜单／应用公共叠层绘制、输入和临时状态；
  Stick 与 OSTB 构建只编译所选家族。`src/ui/common/` 保留接口及通用表现；
  `UiProfile.h` 明确选择家族及顶部安全参数，不根据屏幕大小自动猜测界面。
  `MenuPageHost` 为 MenuScene 中的主菜单及队伍／状态／技能／房间／食物／电脑／通讯录子页共用只读状态／应用导航／通知；
  队伍／技能／食物选择、洗澡及联系人回调仍经 GameEngine 服务变更、保存或切场景。
  RoomFeature 只读库存与选项，无效食物索引归 0；Stick 总量 255 封顶，OSTB 显示完整总量。
  房间／食物的滚动与点击由各家族持有，Stick 食物文字跑马灯不进入 compact_touch。
  StatusUiContent 共用信息绘制，Stick IMU 滚动和 OSTB 跟手／
  回弹／临时缓存由家族单独持有。ContactsUiBase 共用通讯录选择／确认状态与语义动作，
  ContactsUiContent 保留 Stick 原内容绘制，compact_touch 使用微雪式头像列表／操作弹窗；家族各自持有滚动和输入。
  联系人能力复用 ContactRoster，状态页映射队伍实时成员；邀请加入后先关闭 action，
  再经 contactJoined 同步触发到达演出和切场景。OSTB 通讯录仅保留队伍式顶部返回，无底部导航或返回条目；Stick 保留原导航。
  SettingsUiBase 共用设置选择、帮助／重置确认与通知；独立 SettingsHost 共用硬件变更、试听、保存和导航。
  Stick 设置只有列表缓动，compact_touch 单独持有滑杆／滚动／触摸；入页音量分别取整到 10%／保留连续值。
  触摸松手清理先于返回／重播教程／重置成功的激活动作，切场景后不得访问旧界面。
  MainMenuUiBase 共用当前／上次游标、选择推进和教程回调，条目数及语义动作复用 AppSceneFlow；
  MainMenuHost 扩展 MenuPageHost，由 MenuScene 保留探索拦截、1100ms 提示和应用导航。
  Stick 主菜单单独持有选中缓动／电量状态（30 秒检查），compact_touch 单独持有网格滚动／触摸。
  MainScene 通过只读 HomeFrame 委托 HomeUi，相机、HUD、提示、触摸与饥饿插值在 UI；
  HomeUiContent 共用世界图层／阴影／食物／爱心／光照、对话、教程与 debug 边界，RoomRenderer 不变。
  角色活动、寻路、碰撞、访客、成长队列、操作效果、教程完成与位置存档仍由 MainScene 协调；
  共用成长提示经原 host 调用 core/ProgressionUi 应用适配器，再委托所选家族 ProgressionUi。
  适配器只传只读招式帧、执行原成长接口及叫声效果；绘制、视觉时钟与纯输入决策在 UI。
  主房间保持原 66ms 更新和通知到期时序。
  ExploreScene 生成只读 ExploreRouteFrame／ExploreBattleFrame，委托所选 ExploreUi；
  UI 持有区域／命令／侧菜单游标、确认和纯视觉状态，路线／碰撞／遭遇、回合、日志／
  EXP／投食／换人时序、资源预加载和保存仍由场景协调。相同的路线／战斗内容共用，
  家族显式提供布局参数；OSTB 地区列表支持点其他项切预览、再点当前项出发，返回直接点击。
  命中与绘制共用 70px 左列、34px 行距和缓动坐标，保留 4px 点击间隙；拖动／超时／
  取消／按键／离页不激活，子菜单及行走／战斗等阶段不继承列表触摸；行走／战斗使用
  独立点击追踪，阶段／菜单／忙碌状态变化即取消，弹窗不穿透地图。未新增顶部导航／
  安全区，AMOLED 竖屏控制器保持独立。
  探索地图数量／遭遇概率归 ExploreAreaCatalog，冷却／遭遇门控／地图重试参数归
  ExploreRunRules，拾取 ID／有序权重表／步数门槛／金币范围／物品映射归
  ExplorePickupRules；横屏与 AMOLED 都使用这些纯规则。随机调用和状态效果仍在
  原消费者中，保留各自的保底／满包／无效区域处理，不合并会话控制器。
  ExplorePartyService 共用健康／领队／随行查询、行走恢复、探索羁绊和返程异常清理；
  保存紧急级、一次结算标记、道具效果重置和返程时序仍由原调用方控制。横屏仅在
  本地访客 active 且 exploring 时允许其获得羁绊，AMOLED 仍排除访客，且随行额外检查
  speciesId；恢复不排除访客、不复活、不清异常。
  ShowerScene 经 ShowerHost 提供只读 ShowerFrame，ShowerUi 持有选择、工具／IMU、水滴、
  反应和浮字，ShowerUiContent 共用原绘制；场景保留阶段、泡沫规则、奖励与保存协调。
  Stick 保持原 IMU／无 IMU 按 A 输入，OSTB 采用固定按钮与按住拖动擦洗；
  INCOMPLETE 同步切场景立即结束更新。
  SocialScene 经 SocialHost 提供只读会话帧，SocialUi 持有菜单／房间游标、提示起点、
  房间数和等待点时钟，SocialUiContent 共用原显示；场景保留链路生命周期、会话动作和
  ACTIVE 导航。VisitSessionService／EspNowLink 不动，原 A/B、500ms 等待点和 66／200ms
  唤醒保持，没有新增触摸；同步切场景后立即返回。
  DebugUiBase 持有分类、光标、滚动和数字弹窗，经 DebugHost 查询状态及提交语义动作，
  MenuScene 保留原调试服务、导航和通知；release 无 Debug UI 实例或链接符号。
  HatchScene 通过只读 HatchFrame 委托 HatchUi，保留进度／计数、15 秒保存、恢复及完成；
  UI 管理相机／HUD／700ms 提示／视觉观察和语义输入，不改 HatchService。
  MenuOverlayUi 绘制公共底色／通知／教程，场景保留原可见性顺序、文本／期限和进度。
  ApplicationUi 接收只读缺失资源／淡出帧，绘制外层导航／资源警告／暗化；引擎保留
  原输入拦截、资源诊断、过场时钟和帧提交，仅 PixelRenderer::bind 属于运行时画布绑定。
  SceneInput 由家族选择通用触摸命中与按键语义，Scene 只保留 TouchTap 和回调分发。
  横屏场景及应用外层 UI 隔离代码已收口；RoomRenderer／PixelRenderer 等仍为共用表现
  工具，AMOLED 按原设计使用独立竖屏应用，真实设备验收另行完成。
- `firmware/amoled_1_8_v1/main/`：AMOLED 竖屏 UI、触摸状态机和 V1 平台层。
- `firmware/amoled_1_8_v2/main/`：V2 板级适配，应用代码通过 V1 工程复用。
- `src/presentation/`：Canvas565、像素绘制和可跨目标使用的表现工具。
- `src/brain/`：ESP-Claw 远程聊天桥接（开发中、未提交）。硬依赖 ESP-IDF、
  FreeRTOS 和外部 esp-claw 头文件，是 `src/` 下唯一的 ESP-IDF-only 目录；
  只被 AMOLED 固件编译，PlatformIO 的 `build_src_filter` 已排除该目录。
  它不拥有游戏规则，状态变更全部经 `HostAdapter` 回调委托给 `AmoledApp`，
  再走现有共享服务。
- `firmware/common/components/`：把外部 esp-claw 源码与 `src/brain` 包装成
  IDF 组件（`app_claw`、`stickmon_claw`），V1/V2 通过顶层 CMakeLists 的
  `EXTRA_COMPONENT_DIRS` 共用。

## ESP-Claw 远程聊天（开发中，未提交）

工作区有一批未提交的 ESP-Claw 集成（外部 AI agent 框架：WeChat iLink +
Coze Agent 远程读取/操作游戏状态），只接入 AMOLED V1/V2。

- `src/brain/`：`BrainBridge`（action 排队到 UI 任务执行、Snapshot、
  HostAdapter 回调和玩家优先仲裁）、`StickmonClawRuntime`（从 NVS 读凭据、
  WiFi STA、挂载 brainfs、启动 esp-claw、提供按需配置热点和微信二维码登录）、
  `StickmonCapability`（注册 `stickmon_get_context/get_inventory/say/eat/`
  `buy_food/start_expedition/return_home/invite_friend` 八个 LLM 工具，写操作
  带 RESTRICTED 标记）。
- 外部 esp-claw 源码不在仓库内：默认路径 `../espclaw/esp-claw-master`，
  可用环境变量 `STICKMON_ESPCLAW_ROOT` 覆盖；缺失时仅 warning，远程聊天
  不可用但固件仍可构建。
- 编译开关 `CONFIG_STICKMON_CLAW_ENABLE`（各工程 `main/Kconfig.projbuild`，
  默认 y）；凭据从 NVS 读取：`wifi` 命名空间 ssid/password，`claw` 命名空间
  `coze_token/coze_bot_id/coze_base_url`、IM 渠道列表 `im_channels`
  （逗号分隔 wechat/telegram/qq/feishu，旧存档缺失时按已存 token 推导）
  和各渠道键：`wechat_token/wechat_base_url/wechat_cdn_url/wechat_acct_id`、
  `telegram_token`、`qq_app_id/qq_app_secret`、`feishu_app_id/feishu_secret`（页面字段仍显示
  `feishu_app_secret`，内部键名遵守 NVS 15 字符限制）。
  门户删除渠道后点「保存配置」会清掉该渠道全部 NVS 键；`fillClawConfig`
  按渠道列表与已编译 cap（`CONFIG_APP_CLAW_CAP_IM_*`）生成
  `enabled_cap_groups`。
- 两个 AMOLED 工程分区表各新增 `brainfs` FATFS 分区（0xa30000、
  0x5d0000），flash 已用满 16 MB；`SPIFFS_MAX_PARTITIONS` 2→3。
- `tools/generate_claw_nvs.py` 生成 0x6000 完整 NVS 镜像并整分区覆盖，
  会清掉游戏存档：只用于新设备首刷，旧设备先备份 NVS 再写入。
- 2026-08-29 已处理的历史问题：PlatformIO 曾在 `build_src_filter` 漏排
  `src/brain` 导致 StickS3 构建失败（已加 `-<brain/>`）；两个 AMOLED 工程的
  `dependencies.lock` 曾写入 esp-claw 的个人绝对路径（已清理；本机构建时组件
  管理器会重新生成该 local 条目，属正常现象，不要再提交）。
- 远程动作经 `BrainBridge::enqueue` 排队到 UI 任务执行，默认等待 5000ms；
  槽位显式区分排队／执行／通知／完成，一次入队至多执行一次，执行中超时不提前
  复用槽位，结果通知完成前不释放给新请求。超时不代表动作未执行，不能据此自动
  重试购买／进食等变更；`requestId` 尚未构成跨重试幂等键，启用 Claw 前须先核查
  上游工具调用 ID 语义。队列 host 测试：`python3 tools/test_brain_bridge.py`。
- `电脑 -> ESP-Claw` 设置页（仅 AMOLED，`STICKMON_HAS_CLAW=1` 时编译）：
  启动按需 WPA2 热点 + HTTP 配置门户；页面二维码是运行时生成的 Wi-Fi
  配对码（`src/presentation/QrCodeGen.*`，字节模式/ECC L/mask 0/v1-5，
  带 4 模块静区），热点密码每次会话由 `esp_random()` 随机生成。golden
  测试：`python3 tools/test_qr_code_gen.py`。
- 连接状态日志：`src/brain/ClawStatusLog.*` 是 64 条环形缓冲（每条 63
  字节，超长按 UTF-8 边界拆成续行，不截断），由 `ClawRuntime` 用现有
  mux 串行化写入；设备页「连接/日志」双 tab 与门户 `/api/log?since=`
  共用同一份数据源，手机加入热点后设备页自动切到日志视图。
  host 测试：`python3 tools/test_claw_status_log.py`。

优先复用的共享服务包括：

- `ExperienceService`：经验、等级和 HP 成长；进化、学招式、存档和 UI 由调用方负责。
- `FriendshipService`：战后结交、保底、通讯录和邀请判定。
- `MonsterFactory`：新精灵的统一运行时状态。
- `MoveManagementService`：技能读取、遗忘和心之鳞片回忆。
- `BathService`：洗澡阶段奖励、肥皂消耗和按评分恢复 HP。
- `ItemInventory`、`ShopService`、`TeamRoster`、`HomeCare`、`CareTicker`。
- `VisitSessionService`：拜访会话；`AppSceneFlow`：跨固件语义场景与菜单顺序。

增加新流程时，先问“状态变更能否做成不依赖 UI 的共享服务”；可以的话先写
共享服务与 host 测试，再接 StickS3/AMOLED 表现。

多设备界面隔离说明见 `doc/多设备UI隔离.md` 与 `specs/device-ui-isolation/`。
已迁移商店／背包／队伍／状态／技能／房间菜单／食物选择／电脑入口／通讯录／设置／主菜单、主房间、探索／战斗、洗澡及无线通讯；修改这些页面时运行 `test_item_ui_parity.py`、
`test_party_ui_parity.py`、`test_room_ui_parity.py`、`test_contacts_ui_parity.py`、`test_settings_ui_parity.py`、`test_main_menu_ui_parity.py`、`test_home_ui_parity.py`、`test_explore_ui_parity.py`、`test_shower_ui_parity.py`、`test_social_ui_parity.py`、`test_final_ui_parity.py`、相应 `test_shop_flow.py`／`test_bag_feature.py`／
`test_team_feature.py`／`test_moves_feature.py`／`test_room_feature.py`／`test_contacts_feature.py`、`test_ui_family_sources.py` 和
原有 `test_ostb_native.py`。无线通讯另运行 `test_visit_session_service.py` 与 `test_esp_now_link.py`。
像素基线须经审查后才能有意更新，不能为了通过测试覆盖。
2026-10-22 商店／背包 17 帧、队伍／状态／技能 72 帧及 OSTB 75 张截图一致；
两家族真实场景操作结果一致，四固件目标构建通过，未刷机。
第三批房间／食物 45 帧与重构前像素基线一致（Stick 26／OSTB 19），两家族真实场景选择、
空库存、触摸拖动／取消、返回栈、停绘、孵化／空队伍拦截和洗澡入口通过；原 17+72 帧与
OSTB 完整 75 张截图仍一致。房间查询、源隔离、渲染调度、架构及四目标构建通过。
第三批 app 文件大小：Stick 1,412,256 B、
OSTB 1,350,608 B、AMOLED V1 lite 1,403,424 B、AMOLED V2 lite 1,724,800 B；未刷机。
第四批电脑／通讯录已完成实现，冻结旧源码生成两家族各 48 帧、合计 96 帧基线；
新实现 96 帧与基线一致，两家族各 14 组真实旅程通过，包含确认后联系人失效时保留旧提示并
延长 1400ms、加入后由引擎进入新主场景和 TEAM_ENTER_CROSS 到达演出。原 17／72／45 帧
及完整 OSTB 75 张截图仍一致，源隔离 8 项、独立 12 脚本 25 项回归及四目标构建通过；
回归包含存档编码／迁移；两家族 ASan／UBSan 通过，覆盖成功邀请后同步销毁菜单和新主场景到达。
当前 app 文件大小：Stick 1,416,160 B、OSTB 1,353,120 B、
AMOLED V1 lite 1,403,424 B、AMOLED V2 lite 1,725,024 B；未刷机，实机交互和性能待后续验收。

第五批设置（含帮助／重置确认）已隔离，累计十组。
冻结旧源码的 Stick 37／OSTB 38 帧共 75 帧与新实现一致，两家族各 20 组真实旅程通过；
包括入页音量精度、亮度／速度／省电、静音／试听、帮助重播、重置取消／失败／成功、保存标记及停绘。
两家族 ASan／UBSan 通过，覆盖引擎持有场景的触摸返回／重播教程／重置同步销毁；
触摸松手清理先于可能销毁界面的激活动作。旧九组 230 帧和完整 OSTB 75 张截图保持一致，
源隔离 8 项、架构／渲染／存档编码／迁移 17 项、四目标构建通过。
第五批 app 文件大小：Stick 1,414,160 B、OSTB 1,352,464 B、AMOLED V1 lite 1,403,424 B、
AMOLED V2 lite 1,725,024 B；未改存档 schema 或依赖锁文件，未刷机，实机交互与性能待后续验收。

第六批主菜单已实现隔离，累计十一组；冻结旧源码生成四组合 123 帧，
Stick release 28／debug 29、OSTB release 32／debug 34，新实现像素全部一致，
四组合各 24 组真实旅程在新旧实现上通过。四组合严格语法及 ASan／UBSan 通过，
覆盖引擎持有菜单时触摸进入商店／设置／探索／主场景、A／B 长按及 compact 直接返回目标的同步销毁；
debug 仅覆盖入口与战斗返回请求，子页未拆分。完整 OSTB 75 张截图及旧五批 305 帧／真实旅程一致。
源隔离 8 项及 AppSceneFlow／渲染调度／架构／存档编码／迁移 18 项已通过。
四目标构建通过：Stick 1,413,536 B、OSTB 1,351,024 B、AMOLED V1 lite 1,403,424 B、
AMOLED V2 lite 1,725,024 B；家族源互斥且已覆盖最终生产源码，依赖锁未变化，未刷机，实机交互与性能待验收。


第七批主房间已实现隔离，累计十一组菜单及主房间。冻结旧源码生成四组合房间 690 帧，
追加小房间留白 HUD／提示阈值 56 帧，最终共 746 帧（每家族 release 186／debug 187）
与旧源码逐像素一致，原 690 哈希未改；四组合各 24 组真实操作、严格语法与 ASan／UBSan 通过，
含引擎持有场景的菜单／探索邀请同步销毁，补齐小房间后 Stick 两组合内存检查再次通过。
修复探索邀请切场景后仍复位已销毁场景选项的旧问题，将选项复位前移。原菜单 428 帧／
真实旅程、六套 feature／flow 与 OSTB 75 张 PPM 不变；源隔离 8 项、居家静态契约 42 项、
共享居家活动／访客／成长／教程、架构／调度／存档编码及迁移通过。六处旧源码已失败的
居家静态断言经冻结源复现后适配原协调器语义，新旧均通过。
四目标最终 app 文件大小：Stick 1,414,736 B、OSTB 1,351,968 B、AMOLED V1 lite 1,403,424 B、
V2 lite 1,725,024 B；家族源互斥且最终源码／头文件已覆盖，依赖锁未改。未改存档 schema，
未刷机；探索／战斗／洗澡、SocialScene、Debug 子页及共用成长表现仍未拆分，实机性能待验收。

第八批探索／战斗已实现绘制与选择输入隔离。冻结旧源码四组合共 1,286 张完整画面
（每家族 release 321／debug 322）与新实现一致；各 36 组真实操作、严格 host 语法和
ASan／UBSan 通过。覆盖六地图、解锁／轮播／剪影、图层遮挡／蛇行连续帧、菜单暂停与
预加载调度、战斗状态／HP／EXP／换人／投食／确认和成长。独立在旧源码重现两处
同步销毁后访问：区域出发后写成员、调试逃跑日志结束后继续 update；新实现通过
引擎持有场景的真实销毁路径检查。原菜单 428 帧、主房间 746 帧、OSTB 75 张 PPM、
六套 feature、架构／调度／AppSceneFlow／存档编码及迁移回归保持通过。
四目标构建通过，app 为 Stick 1,417,936 B、OSTB 1,354,928 B、AMOLED V1 lite 1,403,424 B、V2 lite 1,725,024 B。
本批仅修改既有 ExploreScene.cpp／.h 并新增 16 个 UI／表现文件，未改玩法、存档 schema
或资源，未刷机。当前剩余洗澡、SocialScene、其余 Debug 子页、共用成长表现和
AMOLED 原竖屏应用未拆分，实机交互／性能待验收。
Stick／OSTB 只链接各自 UI 家族，最终源码与头文件依赖均已覆盖；AMOLED 未加入横屏 ExploreScene 或 UI。三份依赖锁文件哈希未变。

第九批洗澡已沿用现有骨架完成收口（2026-10-23）。原场景取自 Git
a430cd6c0deaa7ebfa9c9d76ae70284ad98d0731，独立生成四配置各 218 帧、共 872 帧基线；
新版逐像素一致，各 35 组真实操作及 ASan／UBSan 通过。旧版未完成退出 700ms 到期的
释放后访问独立重现，新版真实引擎持有场景的销毁路径通过。严格 host 语法通过，
仅允许原 GameState 匿名结构扩展。22 回归脚本 42 项、AMOLED 洗澡三项和干净导出的
OSTB 71 帧通过；原输出中的四张历史截图没有计入本批有效帧数。
四目标构建通过：Stick 1,419,312 B、OSTB 1,356,656 B、AMOLED V1 lite 1,403,440 B、
V2 lite 1,725,040 B。仅链接所选横屏家族，AMOLED 未引入 ShowerScene／UI，三份依赖锁未改。
本轮补齐基线／验收与文档，没有再改已存在的生产拆分源码；未改玩法、存档 schema 或资源，
未刷机。当前剩余 SocialScene、其余 Debug 子页、共用 ProgressionUi，AMOLED 继续独立
竖屏控制器；洗澡实机交互／性能待验收，OSTB 触摸拖动为后续增强。

第十批无线通讯已隔离（2026-10-23）。冻结原 SocialScene 的四配置各 210 帧、共
840 帧与新实现一致，各 32 组操作、严格 host 语法（仅允许既有匿名结构扩展）和
ASan／UBSan 通过。包含真实协议帧的主客握手／拒绝／超时／结束与召回、输入优先级、
静态停绘与 66／200ms 唤醒、真实引擎持有场景的长按／菜单返回及主客 ACTIVE 导航销毁。
既有九套 UI／共享服务／存档／无线协议及访客等 27 脚本 50 项回归、AMOLED PairFlow 三项和 OSTB 71 帧通过。
四目标 app：Stick 1,419,952 B、OSTB 1,357,488 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。
仅修改原 SocialScene.cpp／.h，新增 11 个 SocialUi 文件；会话服务、无线协议、玩法、存档和
资源未改。家族源互斥，AMOLED 未引入横屏 SocialScene／UI，三份依赖锁不变，未刷机。
当前剩余其余 Debug 子页和共用 ProgressionUi；AMOLED 继续独立竖屏应用，真实设备间
无线联调、交互／性能待验收。

## 存档兼容

- 当前 `Game::SAVE_VERSION = 5`，以源码为准；
  `doc/存档兼容与迁移规则.md` 可能落后，修改前必须同时读
  `GameState.h`、`SaveCodec.*`、`SaveManager.*`。
- 新写入使用 `SaveCodec` 显式小端字段编码，不允许直接把 C++ struct 布局当作
  新格式持久化。
- 快照带 schema、sequence 和 CRC；NVS 使用 `state_a`/`state_b` 双槽，读取
  sequence 最大且校验通过的记录，同时保留 `state` 兼容镜像。
- 当前支持 v1/v2/v3/v4/v5 读取和迁移（SaveCodec schema 1=v3、schema 2=v4、
  schema 3=v5，按 schema 兼容解码）。v5 在冻结的 1572 字节后追加每只精灵
  的照料额度记录，MonsterRuntime 仍为 64 字节。迁移必须先成功写入新格式，才能视为完成；
  写入失败时保留旧 blob 以便下次重试。
- 遇到高于当前固件版本的存档必须返回 `NEWER_VERSION` 并写保护，绝不能把
  默认状态覆盖回去。
- `GameState` 字段变化必须显式评估 schema 和迁移。不要依赖 padding 偷塞字段，
  不要仅修改 `sizeof` static_assert 让构建通过。
- M5StickS3 与 AMOLED 必须能读写同一种逻辑快照。平台只实现 blob 存储，
  不得各自定义存档结构。

## 绘制与性能契约

### StickS3

- 场景使用 `SceneUpdateResult` 与 `RenderDemand` 上报真实需求。
- `parked()`：无重绘、无定时更新，等待输入、切场景或外部状态唤醒。
- `frame()`：只重绘当前一帧，不自动安排下一次更新。
- `animate(delay)`：当前帧重绘，并按 delay 持续调度。
- `pollAfter(delay)`/`wakeIn()`：只安排逻辑检查，不代表需要重绘。
- 重绘请求与下一次更新时间是两个独立维度。静态菜单、设置和弹窗不得用
  持续动画掩盖漏掉的状态通知。
- 主界面、探索移动等动态场景可持续刷新，但路径规划、资源解码和全池扫描
  不能放进逐帧热路径。

### AMOLED

- 使用纵向脏区传屏；`AmoledApp::needsRender()` 与 dirty row 范围决定传输。
- 静止页面、暂停探索和静止洗澡页面不得持续全屏刷新。
- 页面切换/唤醒可全帧刷新，局部交互应只标记受影响行。
- 当前 AMOLED UI、脏区与触摸均使用原生 368x448 坐标；184x224 仅是部分
  源素材的历史尺寸，不应作为新的页面布局坐标。

## 关键玩法不变量

- 队伍上限为 2，仓库上限为 20；`activeSlot` 是当前出战位，不等于永久队序。
- 战斗中“替换”只改变本场出战精灵，不改变队伍顺序；队伍页的“首位”才会
  调整正式队序。
- 访客可临时进入队伍/房间，但 `Origin::VISITOR` 的邀请、喂食、首位和离队
  行为必须走现有访客规则，不能按永久成员处理。
- 游戏初始时间为 07:00，初始金币 1000。
- 普通粮每份 3 口；碗容量为一份。具体饱腹、口味和经验使用 `HomeCare` 与
  食物 profile，不要在场景中重复常量。
- 居家 HP 每个游戏内分钟恢复最大 HP 的 1%；饥饿为 0 时每 tick 只恢复 1 HP。
  濒死休息按游戏时间 60 分钟结算，并清除 major status。
- 精灵技能共 3 槽：第 1 槽不可替换，两个特殊槽可遗忘/替换。学新招式、
  升级、进化等队列事件必须由所有经验来源统一触发。
- 洗澡完成按 1/2/3 星恢复 45%/70%/100% HP；阶段经验和心情奖励统一走
  `BathService`。
- 照料经验统一走 `CareExperience`：每日按完整升级跨度的 40%/30%/20%
  固定预算，身份使用 ContactRoster 的不变量；当天升级/进化不重算。
  食物/摸头/洗澡/伙伴互动分别使用约 40%/20%/35%/5% 的额度。
- 区域按击败前一区域头目顺序解锁；探索精灵池缓存直到时间轮换或头目刷新，
  不应在每次进入菜单时重新解码。

## 资源管线

- 2026-11-02 市集已作为独立目的地接入探索列表首项；顺序在
  `src/game/ExploreDestinations.h::ORDER` 维护，不得为重排改变原六区持久化编号。
  正式工程为 `origin_asset/maps/market/market.stickmap.json`，通过
  `tools/map_editor/market_export.py --install` 生成 `MarketMapData.h`；修改位置、道路
  或入口后重新识别道路／保存路线并导出，不手改生成头。共享 `Market::Session`
  处理活动与预约；最多三组／六游客，常驻＋玩家同伴预留 2 只＋每组预留 2 只合计 ≤16；NPC 最多 38 种，菜单头像最多 6。
  本轮补齐“市集／前往／聊天”等原字体字形，共享中文 1172 字；谷仓 16px 1172 字、20px 64 字。
  市集 host、预览和 StickS3／AMOLED V1/V2 lite／谷仓构建通过，未刷机、待实机验收。
  详见 `doc/市集接入与调整指南-2026-11-02.md`。
  后续横屏／谷仓市集独立启用 `enableManual`／`freeWalk`：1× 地图与精灵、满屏地面、
  按下定位的四向摇杆和邻近 NPC 聊天气泡按钮；无右上菜单，走回出口离场。最终碰撞网格 `pedestrian` 只供玩家／同伴；
  NPC 范围和道路来访不变。AMOLED 后续于 2026-11-03 也接入同款摇杆与走出返回，
  1.5× 地图铺满 368×448，底栏移除，靠近 NPC 时底部显示72×72聊天按钮。
  市集 `bgm_market` 使用 Emerald 原 MIDI＋现有 Essentials 音色库，8kHz 原前奏／完整循环，
  流式插值至 16kHz；谷仓仅补入这首音乐，沿用音量 0 设置。源与生成器说明见接入指南。

- 2026-11-03 市集新增 NPC 专属变隐龙／袋兽／吉利蛋／信使鸟，名单在
  `origin_asset/market_npc_species.json`，不要混入收服／孵化和 ORAS 名单。图包 68 个，
  可养成仍 64 种；前三者与皮丘常驻，7 组结伴＋3 种单只来访，20 种 NPC。
  `Family::juvenile=0` 表示缺席同行，编辑器、ZIP、预载和两种模拟均支持；0 不加载资源。
  原 64 包与 1601 个 SpriteKind 字节／编号保持，新 Kind 在 STATUS 后追加。
  动画源及哈希在 `origin_asset/source_sheets/spritecollab-market/`，显式离线导入工具
  `tools/import_market_npc_frames.py`。本批资源增 25915B（含谷仓索引），谷仓 release
  app3066224B，距3MiB余79504B；四目标构建通过，未刷机，实机待验收。

- 源资源：`origin_asset/`；开发包输出：`data/packs/dev/`。
- StickS3 从 LittleFS 读取，AMOLED 从 SPIFFS 读取；资源格式和 pack 内容共用。
- 精灵图：`python3 tools/generate_pokemon_sprites.py`，输出
  `data/packs/dev/sprites/*.smonsp` 并更新 `PokemonSprites.h/.cpp`。
- 游戏资源：`python3 tools/generate_game_assets.py`，输出
  `data/packs/dev/game/{ui,battle,maps,hatch}.smonfx`。
- 字体：`tools/generate_font16cn.py`，共享 `zh16.smonfont` 与
  `ascii16-unscii.smonfont`。
- 谷仓原生顶部“菜单／商店／背包”和侧栏“购买／出售”单独使用
  `tools/generate_round_ui_fonts.py` 从更纱原 TTF 生成的 16px／20px A4 子集，
  输出 `round-zh{16,20}-a4.smonfont`；当前 16px 覆盖共享中文 1172 字，20px
  覆盖导航／指令 64 字，合计 167792 B；SMFN v2／flags=4，
  每像素 4-bit 覆盖率，高半字节在前，不改原 v1 单色字体。`textSmooth` 显式启用，
  按需加载并混合真实 RGB565 背景；缺失／损坏时回退原点阵。顶部标题原生 20px，
  导航组整体居中、返回矩形 (138,12,40,40)；侧栏选中 20px、未选中 16px，
  17–19px 仅用于动画。背包主体仍用中央 240×240，RoundSurface 组合后绘制物理顶部栏；
  顶部返回映射到页面坐标 (78,-48,40,40)，其余越界触摸取消。进出页面清空顶部栏，
  探索／战斗内嵌背包共用此路径，确认弹窗阻断顶部返回。商店余额固定 (32,80,72,28)，
  金额点击优先于相交的灰色项热区；音量仍为用户设定 0。
- 外部 Pokemon Essentials 通过 `ESSENTIALS_DIR` 指定，或放在 git 忽略的
  `external/pokemon-essentials`；禁止提交个人绝对路径和受版权保护的外部仓库。
- 地图使用运行时 tile atlas，不要恢复为整张地图位图缓存。
- 雾林径（区域 4）现使用 v25 七样式树种渐变表，维护源为
  `tools/mist_forest_layouts.py`／`tools/mist_forest.py`，生成
  `src/game/ExploreMistForestData.h`；保留 6～8 段行程和四向入口，
  不能绕过段号／总段数上下文退回旧围栏生成器。普通草每片 4～9 格、
  花每片 3～6 格、长草每片 3～5 完整两格植株，连片且避开道路／树根。
  两组有限细节配方由地图种子选择，不宣称无限随机；仅使用 Essentials 原图。
  原 774 个 Kind 保持编号，地图包为 715 帧；固件和地图资源需同步更新。
  树冠按整树地面锚点遮挡，普通草／长草复用接触反馈，鲜花只增加草地栖息语义。
  本轮专项、资源／地图／生态／草坪／原古瀑谷和架构检查通过，四目标构建通过；
  未刷机，实机待验收。详见地图生成规则 v25 节与
  `.stick-local/artifacts/mist-forest-integration-20261025/validation.json`。

### GameAssets 不变量

- `GameAssets.h::Kind` 与 `generate_game_assets.py::KIND_ORDER` 必须逐项同序；
  在枚举中间插入 Kind 后必须全量重生成四个包。
- 生成器与 `GameAssets.cpp` 两侧共同校验上限：768 帧、200000 data words、
  4096 palette words、384000 payload；修改时必须同步。
- `GameAssets.cpp::packSlotFor` 决定路由：UI 段与探索拾取球进 UI，
  `EXPLORE_TILE_*` 进 MAP，蛋进 HATCH，其余默认 BATTLE。
- 背景类 StickS3 资源为 240x135、LANCZOS 缩放、16 色量化；AMOLED 竖屏
  表现可裁切/重排，但不能修改共享 Kind 语义。
- 精灵 FRONT 的不放大名单在
  `generate_pokemon_sprites.py::ENEMY_FRONT_NO_UPSCALE_SPECIES`；修改会同时影响
  战斗、洗澡和探索预览，必须用真实设备或导出图检查像素清晰度。

## 验证

- 窄改动优先运行对应 `tools/test_*.py`，这些测试会按需编译相邻 `*_host.cpp`。
- 共享玩法/存档修改至少运行相关 host 测试；存档修改必须运行：
  `python3 tools/test_save_codec.py` 和 `python3 tools/test_save_migration.py`。
- 场景调度修改运行 `python3 tools/test_render_demand.py`；共享边界修改运行
  `python3 tools/test_architecture_boundaries.py`。
- 资源修改运行对应生成器测试，并重新生成受影响 pack；地图算法的 Python/C++
  顺序、PRNG 调用和指纹必须保持一致。
- 合并前至少构建受影响固件目标。触及共享源码时应构建 StickS3 和 AMOLED V1；
  若 V2 板级/共享 AMOLED 代码受影响，也构建 V2。
- 不能运行实机测试时必须明确记录。V2 显示/触摸、三目标 ESP-NOW、音频、
  PMU 深睡、电池/IMU/RTC 仍属于重点实机风险。

## Web 模拟器（2026-10-17）

- 当前源码树包含 `src/platform/web/`，不再需要切到旧 `web-simulator` 分支才能构建。
- `bash tools/build_web.sh --target sticks3|amoled-v2` 输出到 `build/web/<target>/`。
- 微雪入口运行真实 AmoledApp，复用 `ui/sources.txt` 和原生资源包；V1/Claw 不纳入网页试玩。
- 页面唯一编辑源：Stick 的 `web/index.html`、微雪的 `web/amoled-v2/index.html`。
- `python3 tools/publish_web.py --flasher <checkout>` 先验证两目标，再发布本地产物，
  不自动推送/部署。不要手改刷机站生成的哈希入口。
- 微雪 localStorage 前缀独立于 Stick；Web Locks 保证单写。失焦/隐藏暂停逻辑时钟，
  取消触摸不能合成为一次点击，关闭面板先保存再释放。
- PlatformIO 必须排除 `platform/web/`，否则会误编译 Emscripten 启动入口。
- 验证：`python3 tools/test_web_simulator.py`，以及原生 AMOLED 渲染、存档和三目标构建。
- 完整说明与未验证边界：`doc/微雪V2-网页模拟器.md`。

## 参考文档

- `specs/README.md`：现行规范与验收索引。短篇需求／设计／任务优先合并，相关子项
  收到同一主题；整体被替代的旧方案删除并更新引用，不保留过期的并行标准。仍有效的
  硬件依据、验收边界与未完成项应迁入现行文档；整理备份／截图／日志／冻结源码只放 `.stick-local/`。
- `doc/StickMon-开发计划.md`：总体玩法与资源方案，部分早期参数可能落后。
- `doc/ESP32-S3-Touch-AMOLED-1.8迁移与双版本兼容方案.md`：三目标架构与迁移状态。
- `firmware/amoled_1_8_v1/README.md`、`firmware/amoled_1_8_v2/README.md`：
  AMOLED 当前能力、构建和待验收项。
- 任何文档与源码冲突时，以代码、测试和最新实机日志为准，并同步修正文档。

第十一批 Debug／共用成长表现已隔离（2026-10-23）。冻结工作树四配置 764 帧一致，
release 各 11／debug 各 68 组操作、严格语法及 ASan／UBSan 通过；旧十套 UI 4,172 帧、
33 脚本 87 项、原生 OSTB 71 帧通过。四目标及 Stick debug 构建通过：release 1,421,360 B、
debug 1,437,152 B、OSTB 1,358,704 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。
源／头文件覆盖、家族互斥、release Debug 符号门控和三份依赖锁已审计；未改玩法、存档、
资源或 AMOLED 源码，未刷机。AMOLED 成长四项通过；扩展检查合计 26 项中三项既存源码
断言失败（旧日志／锚点／ROOT 赋值），源与测试哈希相对本轮起点未变，详见隔离文档及
`.stick-local/artifacts/device-ui-final-20261023/amoled-results.json`，不应描述全部测试通过。
剩余横屏 UI 为 HatchScene 孵化页和 MenuScene 公共底色／通知／教程叠层；AMOLED 保持
独立竖屏应用。真实设备观感／输入／性能和跨设备无线仍待验收。


第十二批孵化／菜单公共叠层已隔离（2026-10-23）。冻结源码四配置 2,640 帧一致，
正常与蛋资源缺失启动分别通过 Stick release 49／debug 51、OSTB release 50／debug 52 个
操作／调度检查点，覆盖完整房间、昼夜、缺失房间、保存失败／恢复和真实场景销毁。
ASan／UBSan、严格语法、旧十一套 UI 4,936 帧、33 脚本 88 项、OSTB 原生 71 帧和
AMOLED 孵化三项通过。五配置 app：Stick release 1,422,224 B、debug 1,438,048 B、
OSTB 1,359,680 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B；源／头文件覆盖、
家族互斥、release Debug 门控和依赖锁已审计。仅改既有 HatchScene／MenuScene 四文件并
新增 18 个 UI 文件，玩法／存档／资源／协议和 AMOLED 不变，未刷机。横屏场景直接绘制
已迁出；核心外层导航／通用表现工具及 AMOLED 独立应用保持原边界，真实设备验收待做。
上一批 AMOLED 三项既存源码断言失败未处理，不能标记全仓库测试全部通过。


第十三批应用外层已收口（2026-10-23）。导航、资源缺失提示和淡入淡出经 ApplicationUi
绘制；SceneInput 接管通用命中／虚拟按键，保持 TouchTap 原规则。冻结旧源码四配置各
165 帧共 660 帧一致，各 36 个真实输入／过场检查点和 ASan／UBSan 通过；旧十二套 UI
7,576 帧、33 脚本 89 项、OSTB 原生 71 帧、Web 一项及 AMOLED 孵化三项通过。
新／迁出代码严格检查通过，GameEngine 全文件既存 unused 诊断与冻结旧版逐项一致，
不能描述为全核心零诊断。五配置 app：Stick release 1,422,368 B、debug 1,438,192 B、
OSTB 1,360,112 B、AMOLED V1 lite 1,403,440 B、V2 lite 1,725,040 B。
源／头文件覆盖、家族互斥、旧导航符号移除、release Debug 门控和依赖锁已审计。
仅修改 GameEngine.cpp、Scene.h、TouchUi.cpp／.h 并新增 12 个 UI 文件，玩法／存档／资源／
协议／原场景／AMOLED 未改，未刷机。横屏隔离代码完成；共用表现工具与 AMOLED 独立竖屏
应用保留设计边界，实机验收仍待做；三项既存 AMOLED 源码断言失败仍保留记录。

探索共享规则首批已完成（2026-10-23），验收见 `specs/explore-shared-rules/acceptance.md`。
四配置真实横屏状态／随机调用、36 条 AMOLED 路线新旧一致，1,286 帧画面与五配置应用
编译通过；规则数值和随机调用位置不变，未刷机。物品测试五项及 AMOLED 路线旧基准一项
既存失败已单列，不能描述全仓库测试全部通过。

OSTB 洗澡页已按用户要求改为四个固定触摸按钮（2026-10-23）：肥皂／刷子／冲水／退出
始终同位，无滚动；菜单中 B 仅改高亮，重复点当前工具不重启阶段。肥皂名称可切换库存，USE
确认消耗；退出 yes／no 可点击，弹窗阻断工具点击。复用 TouchTap 松手／取消规则，
搓洗仍沿用晃动或无 IMU 长按 A，触摸拖动搓洗未实现。原 shower_ui_parity fixture 保留，
OSTB 有意视觉变化另存 shower_ui_touch fixture；Stick 两配置原像素不变。洗澡四配置、
OSTB 两配置 ASan／UBSan、严格语法／家族源检查及 OSTB／Stick／AMOLED V1 lite 构建通过；
未刷机，实机触摸待验收。详见隔离文档末节。


OSTB 洗澡／技能交互已按最新要求更新（2026-10-23）：去掉 HOLD A，肥皂和刷子选中后
按住精灵区域拖动，工具跟手；静止按住、松手、移出区域、锁屏／按键／切场景取消后均
不继续擦洗，右侧四按钮保持固定。输入采集与位置在 compact_touch UI，阶段冷却与泡沫／
奖励仍走原共享协调；静止工具页可停绘，泡沫下落及反馈仍按需更新。技能左列仅有技能，
44px 点击区、6px 间隔；描述按宽度换行，超出才手动上下滑动，无自动跑马灯，切换技能
重置描述位置；遗忘默认 No、基础技能锁定、回忆和顶部返回流程保持。
原冻结 fixture 与上一版固定按钮 fixture 均保留，当前 OSTB 画面另存 shower_ui_drag
和 party_ui_touch；Stick 洗澡两配置及技能画面保持原像素。已通过真实操作、OSTB 洗澡
两配置／技能 ASan／UBSan、严格语法及 OSTB／Stick／AMOLED V1 lite 构建；未刷机。
本轮 23 个相关回归脚本通过；洗澡非运动输入帧仅移除 HOLD A 区域，技能顶部导航
像素不变。既有全仓库其他失败记录不属于本次修复，仍保留。

2026-10-23 OSTB 肥皂弹窗／食物导航收尾：肥皂三卡直接点选，缺货禁用，取消不扣库存，
不再显示 USE。食物页复用队伍 safe header，列表 y=52、正文全宽，绘制与命中同步。
23 个相关回归脚本、OSTB 洗澡两配置及食物 ASan／UBSan、严格语法、原生顶部安全区
检查和 OSTB release／debug、Stick release、AMOLED V1 lite 构建通过。原 fixture 保留，
新期望另存 shower_ui_picker／room_ui_food_header；预览与源码冻结见隔离文档。未刷机。

2026-10-23 OSTB 探索地区触摸：左侧点其他地区切预览，再点当前项出发，返回直接点击；
命中沿用原列表缓动坐标和可见范围，区域检查仍经原引擎；子菜单与其他阶段不接地区触摸。
21 个相关回归脚本、两配置 ASan／UBSan、1,286 帧原像素和 OSTB release／debug、Stick
release、AMOLED V1 lite 构建通过。UI 四配置严格语法通过；ExploreScene 全文件原有
符号比较告警与冻结源一致。源码及验证见隔离文档，未刷机。

2026-10-23 探索内嵌菜单触摸修复：ExploreScene 原先只委托绘制／更新／实体按键，
未传递原始触摸，导致探索进入的队伍／状态／技能／背包无法点击。现按当前子页委托
原始手势、导航能力和取消；探索地区命中仍与子菜单隔离。按键与触摸共用子页返回、
战斗道具结果处理，保留一次消耗、投食演出和反击。验证详见隔离文档末节；未刷机。
探索子菜单修复收尾：20 个相关回归脚本、探索四配置 1,286 帧和 OSTB 两配置
ASan／UBSan 通过；OSTB release／debug、Stick release、AMOLED V1 lite 构建及 debug
刷入预演通过。仅补通输入协调，未更新像素 fixture，未刷机；实机触摸待验收。

2026-10-23 OSTB 房间 HUD 按最新要求改为 18×8 电池、左上角 (28,12) 单面板内双精灵
上下排列（行距 20px），取代原横向／仅占留白限制。菜单为 (216,202,36,32) 三横线
按钮，绘制命中共用矩形，向内避让右下圆角。房间原点／尺寸与精灵比例不变；电池
共享绘制器默认倍率保持微雪旧尺寸。新画面期望另存 home_ui_hud，旧 fixture 保留。
源码冻结、圆角预览及后续验证见隔离文档和 .stick-local/artifacts/ostb-home-hud-20261023；未刷机。
房间 HUD 收尾：四配置房间 746 帧、OSTB 两配置 ASan／UBSan、严格语法、原生房间
检查通过；18 个扩展回归脚本通过。探索当前像素测试受并行地图改动影响失败；冻结源仅
替换本轮三绘制文件后探索四配置 1,286 帧及语义与旧基线完全一致。OSTB release／debug、
Stick release、AMOLED V1 lite 编译通过；V2 lite 被另一路 ExploreScreens.inc:332 缺少
前景绘制函数声明阻断，原电池头也复现。未修其他工作、未刷机，不宣称全仓库全通过。

2026-10-24 霜晶窟接入批准的四类随机布局，共 384 组几何配方，地图算法版本 22；
洞内原素材上下梯替代边界亮缝，向上梯两格共用下格中心深度，洞内最终退出原地过场。
地图/移动/原生遮挡与无缓存/横屏流程检查及四目标构建通过，未刷机、待实机验收。
详见 `doc/霜晶窟随机布局与楼梯遮挡.md`；保留既有全仓库其他失败记录。
同日同步 AMOLED 遮挡文档并复验霜晶窟原生遮挡、地图 19 项、冰面 2 项和 V1／V2 lite
构建；BrainBridge 队列竞态修复通过并发 host 及 V1 Claw 编译，未启用或刷入 Claw。
`test_amoled_source.py` 既有板级额外源数量断言仍失败（实际 6／7、旧期望 5／6），
本次未修改该测试或源清单，不能描述全部测试通过。

随后 AMOLED ItemShop／Team／Computer／Explore 四页已从 `.inc` 改为独立编译单元，
HomeScreen 仅聚合公共页面头；TeamInternal 供通讯／成长余留页复用，Progression／Shower
增加窄声明头，三个 Controller／Presenter 不再依赖 HomeScreen 伞形头。源清单测试改为
精确板级路径集合及重复检查。release／debug 原生渲染各 80 项、控制器 11 项、Web 模拟器
及 V2 Web 构建通过；拆前后完整／局部帧逐文件一致。V1／V2 lite 与 lite debug、V1 Claw
构建通过，未改玩法、资源或 fixture，未刷机。随后按用户确认删除两条绑定旧日志的
Debug 源码断言（visitor-exit route-fallback/route-end、anchor/errorPx），保留其余生命周期、
坐标和无标签检查；`test_amoled_debug.py` 19 项通过。不描述全仓库测试全部通过。

AMOLED STORAGE/Contacts 已抽为 `ContactsController`：持有六个弹窗瞬态字段、只读能力查询、
action/confirm 语义结果和 STORAGE ViewModel；App 保留坐标、导航、debug 访客、实际玩法、
缓存／actor／toast／保存。邀请提示仍为 LOCKED→VISITING→TEAM_FULL，弹窗不含 BACK，
接受／拒绝／删除副作用顺序不变，Claw／AI_HOSTING／普通滚动未迁。新增控制器 host；
release／debug 原生渲染各 80 项、控制器 11 项、Contacts feature、Web 模拟器与 V2 Web、
V1／V2 lite 和 lite debug 构建通过，51 张拆前后帧一致。未改玩法、schema、资源或 fixture，
未刷机。

AMOLED HomeViewState 已抽为本地纯映射模块：主／副精灵 encode/decode、transient、
方向／帧、身份与相对期限集中；secondary 先解码临时值，房间校验后由 App 提交。
房间修复、homeRuntime、RNG 顺序与资源所有权仍归 App，未改 SaveCodec/schema。
App 重复 VISITOR 过滤已删除，SaveManager 统一剔除，AMOLED 仍无条件 activeSlot=0。
新增映射与真实 blob host：内存保留访客、快照无访客且 activeSlot=0；save codec/migration、
Home 36 项、release/debug 原生渲染各 80 项、Web 模拟器与 V2 Web、V1／V2 lite 和
lite debug 构建通过，拆前后帧一致。未改资源或 fixture，未刷机。

AMOLED Progression 已迁为 `ui/ProgressionScreens.cpp` 独立编译单元，三个公开 API 不变，
LevelUp／Evolution helper 为页面私有；TeamInternal 仍供通讯页，Shower／Battle 未迁。
release／debug 原生渲染各 80 项、Progression／控制器／源清单／CI scope、Web 模拟器与
V2 Web、V1／V2 lite 和 lite debug 构建通过，51 张拆前后帧一致。未改模型、玩法、资源或
fixture，未刷机。


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


2026-10-24 OSTB 探索选择页已从“松手单步切换”改为跟手拖动：左列按实际位移 1:1
移动，松手选最近项并吸附，支持跨多项；按住暂停自动居中，长按不取消拖动，预览在
松手后切换，拖动不出发／返回。取消恢复锚点，吸附中再按住后超时／取消也会继续吸附。
手势留在 compact_touch，shared base 仅开放游标更新重写与提取原预览时钟 helper。
四配置原 1,286 帧相对本轮冻结源不变、各 36 个旅程及新增触摸断言、OSTB 两配置
ASan／UBSan、严格语法和 20 个相关回归脚本通过；OSTB release／debug、Stick release、
AMOLED V1 lite 与 Debug --dry-run 构建通过。探索旧 fixture 每配置相同 6 张地图差异仍
保留，未覆盖基线；未刷机，实机手感待验收。证据见隔离文档与
.stick-local/artifacts/ostb-explore-drag-20261024。


2026-10-24 OSTB 探索地图接入精灵活动：compact_touch 复用共享 ExploreEcology／ExploreGrass，
支持地区栖息、巡游、家庭结伴、睡眠／躲藏、飞行及水面反馈，最多三只环境精灵；
草丛随接触拨动并回弹，地面／水中／飞行精灵按树木、桥面与洞窟梯子语义遮挡。
模拟状态归家族 UI，ExploreScene 仅协调地图重置、暂停、遭遇后显形与素材预加载；
新增可选路线叠层接口，Stick 使用原路径。菜单／内嵌页／战斗／拾取／落穴等待暂停活动。
入图预载活跃池及家庭成员，绘制仅查询已驻留精灵帧；路线优先固定素材时保留其他地区
原预览缓存。共享玩法、地图、遭遇随机调用、存档及 AMOLED 实现未改。
六地区真实场景、隐藏／显形、桥面／梯子精确遮挡、草丛接触／恢复等 38 帧在 OSTB
release／debug 通过；两配置新场景与原旅程的 ASan／UBSan、四配置 UI 严格语法、
20 个扩展回归脚本及共享生态／草丛检查通过。原探索四配置共 1,286 帧：Stick 642 帧
与改前一致，OSTB 各 94 张活动背景有意变化，战斗像素不变；各 36 个原检查点通过。
原 fixture 保留，审查后增加 88 张／配置的活动叠层期望，排除每配置原有 6 张霜晶窟
地图差异；这些既有失败仍可见，未描述为全仓库通过。新增专用测试为
test_explore_ecology_ui.py，源码冻结、逐帧审查与验证见隔离文档及
.stick-local/artifacts/ostb-explore-ecology-20261024。
OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 通过，app 为
1,420,032／1,435,744／1,446,032／1,426,464 B。未访问串口、未刷机；实际帧率与观感待验收。


2026-10-24 OSTB 探索弹窗／行走／战斗触摸修复：特殊遭遇、拾取、结束、结果与结交确认
按屏幕居中，结束／结果文字也按实际宽度居中。战斗四命令在两侧各 28px 范围内均分，
文字 x=42／102／162／222，绘制与命中共用布局；点命令直接执行原战斗／背包／换人／
逃跑动作。点地图调用原 WALK 自动沿路线行进；滑冰／落穴等待不触发，侧菜单与弹窗
独立命中。手势只在 compact_touch，松手前清理状态，拖动／超时／取消／按键／阶段与
菜单变化不执行，日志／HP／EXP／换人／投食期间不接收战斗命令。
四配置各 36 个原检查点与新增触摸旅程、OSTB release／debug ASan／UBSan、四配置 UI
严格语法、38 帧生态两配置与 20 个相关回归脚本通过；场景严格检查保留既有诊断。
1,286 帧对比冻结源：Stick 643 帧不变；OSTB release／debug 仅 135／136 张弹窗和命令
有意变化，普通战斗差异均在 y>=204 页脚。逐帧审查后另存 explore_ui_touch.json 及来源，
旧基线与生态期望未改，每配置 6 张原霜晶窟差异仍保留，探索整项不标为通过。
OSTB release／debug、Stick release、AMOLED V1／V2 lite 构建及 Debug --dry-run 通过，
app 分别 1,422,256／1,437,792／1,446,496／1,426,464／1,748,032 B。
未访问串口、未刷机；圆角和点击实机观感待验收。证据集中于
.stick-local/artifacts/ostb-explore-route-battle-touch-20261024。


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


2026-10-24 古瀑谷接入用户批准的 A/B/C/D 四布局，地图算法版本 23。
上／右／下／左入口分别选择 D／C／A／B；固定人工布局，不宣称随种子随机地形。
共用 JSON 生成 Python／C++ 地形，原 Caves 洞壁／地面／楼梯与原 Sea 四分块组合，
Outside 1136／1138／1144／1146 修复直岸和弯曲瀑顶岸唇；六处瀑布保留上下游。
北 524+532、南 507/508/509+516、左右 517/515 洞口按原洞壁组合；两格干燥接近区、
格心路线、纵向台阶、角色地面锚点及洞口原地退出在横屏与 AMOLED 同步。
新资源仅追加 Kind，原 413 编号不变；map 包 431 帧，应用和地图包需同步更新。
地图 19 项、资源 20 项、古瀑谷专用 4 项、生态／冰面及 AMOLED 原生渲染 80 项通过；
其他五地区 40 组图层／入口／路线与起点一致；四目标 release／lite 构建通过。
旧探索 fixture 每配置仍有 6 张既有霜晶窟及 6 张本次古瀑谷差异，生态 UI 旧地图
截图亦有差异，真实流程断言通过；未覆盖旧 fixture，不宣称整仓所有测试通过。
证据集中于 .stick-local/artifacts/ancient-valley-integration-20261024，详情见
`doc/古瀑谷布局与瀑布衔接.md`。未刷机，实机观感与性能待验收。


2026-10-24 OSTB 探索地图 HUD／菜单：左上角 (36,12) 面板按真实队伍顺序纵向显示前两只
精灵的 HP／异常状态，保留濒死成员，行距 20px；地图名面板右侧留 28px、y=12。
右下角复用房间 (236,192,36,32) 三横线菜单按钮；位置与绘制统一在 compact_touch/HudControls.h，
房间像素不变。探索菜单为 (188,36,80,152) 四行面板，36px 行距、4px 点击间隙，保留
队伍／背包／结束／返回；外部不穿透，滑冰／落穴不开放。新增只读 teamHud／menuAvailable，
布局与手势仍在家族 UI，地图／生态／玩法／存档及 AMOLED 实现未改。
四配置原 36 检查点与新增菜单／HP／状态断言通过，Stick 643 帧不变；OSTB 原画面各 94 张
变化仅限 HUD／地图名／菜单区域，新增各 11 张状态画面。审查后另存 explore_ui_hud 与
explore_ecology_ui_hud 期望，保留旧基线和每配置 12 张地图、生态每配置 3 张古瀑谷差异。
20 个相关回归、OSTB 两配置 ASan／UBSan、四配置 UI 严格语法通过；场景既有诊断与冻结源
一致。OSTB release／debug、Stick release、AMOLED V1 lite 及 Debug --dry-run 构建通过；
app 分别 1,428,976／1,444,640／1,452,064／1,432,896 B。未刷机，实机圆角／触摸待验收。
证据与预览见 .stick-local/artifacts/ostb-explore-hud-menu-20261024 和隔离文档末节。


2026-10-24 OSTB 战斗底部面板由 36px 增高至 46px，顶边 y=194；命令与战败返回提示
按面板纵向居中，点击范围同步增高。名字／等级／HP／异常状态／EXP 上移 10px，
我方精灵图底部从 y=212 调整至 y=194，与面板顶边贴齐，不再覆盖；投食起点同步上移。
compact_touch 显式传 footerExtraHeight=10，共享绘制按布局定位，Stick 默认坐标与 643 帧
像素不变。四配置各 36 原检查点及新顶边命中检查、OSTB 两配置 ASan／UBSan、四配置
UI 严格语法、家族源／架构／渲染调度检查通过。OSTB release／debug、Stick release、
AMOLED V1 lite 和 Debug --dry-run 构建通过，未刷机。新战斗截图另存
explore_ui_battle_footer.json，原 fixture 保留，每配置 12 张既有地图差异仍可见。
对照图与验证见 .stick-local/artifacts/ostb-battle-footer-20261024；实机观感待验收。


2026-10-24 OSTB 探索行走脚底对齐：青草径／溪桥坡／雾林径此前以图片中心对齐道路
路线点，导致左右行进时脚底和影子偏向道路下沿。compact_touch 显式启用
feetOnRouteCenter，共享绘制复用微雪的 frameGroundOffsetY 素材脚底锚点，领队／随行／
Boss 统一对齐路线中心；原已使用脚底锚点的高草园／霜晶窟／古瀑谷保持原像素。
只改三个 UI 文件，玩法／路线几何／碰撞／地图／生态模拟／存档／资源及 AMOLED 未改。
两家族四配置各 36 个原检查点及新增左右行进截图通过；Stick 原 643 帧完全不变，新增
四张左右截图；OSTB 各 51 张原路线背景有意变化、各新增两张左右截图。生态两配置
各 38 帧流程检查通过，仅各九张地区 0／1／4 的领队／Boss 锚点变化。OSTB 两配置
ASan／UBSan 与帧一致性、四配置严格语法、家族源／架构／渲染调度检查通过。
审查对照图后另存 explore_ui_route_anchor／explore_ecology_ui_route_anchor 期望，原 fixture
保留；路线每配置 12 张霜晶窟／古瀑谷、生态每配置三张古瀑谷旧地图差异仍可见，
不描述探索像素整项通过。OSTB release／debug、Stick release、AMOLED V1 lite 与
Debug --dry-run 构建通过；未访问串口、未刷机，实机观感待验收。证据见隔离文档与
.stick-local/artifacts/ostb-explore-route-anchor-20261024。


2026-10-25 OSTB 房间对话框／学习招式触摸：房间访客、邀请、告别和拜访中对话框改为
x=28、宽 240、底边 y=232；对话可见时隐藏右下菜单，避免遮字。HomeUi 家族提供位置，
共享绘制默认仍为 Stick 原坐标。学习确认 (28,46,240,148)、遗忘／放弃确认
(28,52,240,136)、替换结果 (28,56,240,128) 全部居中；是／否为 44px 高点击区。
遗忘选择行改为 44px 高、50px 间距，描述换行；基本招式不可遗忘。房间／探索两个入口
均委托家族触摸，点是／否执行原语义，替换后可点继续。取消／拖动／超时／按键／离页／
待学习招式变化取消触摸，重复松手不重复学习或误确认下一弹窗；确认默认否。
共享规则、地图、资源、存档及 AMOLED 界面未改。房间四配置 746 帧／各 24 检查点、
成长四配置 766 帧／release 11、debug 68 检查点及新增真实双入口点击断言通过；
Stick 755 帧保持原像素，OSTB 房间各 28 帧／成长各 74 帧有意变化。审查后新增独立
home_ui_dialogue_and_learn、final_ui_move_learn_touch 期望，原 fixture 全部保留。
OSTB debug ASan／UBSan 与 273 帧一致性、四配置 UI／适配器严格语法、家族源／架构／
渲染调度及原生 OSTB 检查通过。OSTB release／debug、Stick release、AMOLED V1 lite 和
Debug --dry-run 构建通过；未访问串口、未刷机，实机观感待验收。源码冻结、对照和验证
见 .stick-local/artifacts/ostb-home-dialog-move-learn-20261024 与隔离文档末节。


2026-10-25 OSTB 探索落地点微调：按用户新要求，从道路中心向下偏移道路高度的 1/6，
使脚底处于距道路底部约 1/3 的位置；双格道路约下移 9px，单格窄路约 4px。
compact_touch 显式传 routeGroundBias，领队／随行／Boss 的脚底、影子、遮挡与深度排序
使用相同视觉落地点；精灵球影子也对齐该落地点，转弯不突然切换偏移。仅修改三个 UI
生产文件，地图／路线／相机／移动／碰撞／生态模拟／资源／存档及 AMOLED 未改。
四配置各 36 个原旅程检查点、生态两配置各 38 帧活动断言通过；Stick 647 帧不变，
OSTB 各 107 张路线背景、生态各 38 张因落地点有意改变。审查截图后另存
explore_ui_lower_third／explore_ecology_ui_lower_third 叠层期望，保留全部旧 fixture。
另明确补入上一批已授权学习招式 UI 的九张探索画面，本批改前／改后这九张完全一致，
不归因于落地点调整；仍保留路线每配置 12 张、生态每配置三张既有地图差异。
OSTB debug ASan／UBSan 与 335 帧一致性、四配置严格语法、家族源／架构／渲染调度
通过；OSTB release／debug、Stick release、AMOLED V1 lite 及 Debug --dry-run 构建通过。
未访问串口、未刷机，实机观感待验收。源码冻结、对照与验证见
.stick-local/artifacts/ostb-route-lower-third-20261025 和隔离文档末节。
相关页面／feature／原生回归 17 个脚本通过；探索／生态像素两项仍仅报告上述既有地图差异，
详见本批 regressions.json 与 validation.json。


2026-10-25 OSTB 探索菜单返回／战斗特效：触摸侧菜单“返回”改为独立 MENU_RETURN_WALK
语义动作，关闭菜单后直接启动／恢复自动行进；暂停中的单步保留插值起点并继续，不
重复计步。外部、行间隙、取消／拖动／超时和重复松手不穿透，Stick 实体键语义保持。
compact_touch 显式启用 moveEffects，迁入微雪六段 80ms 的攻击前冲／受击抖动与四帧
命中特效，复用共享 GameAssets 的撞击／火／水／电／抓／咬／瞪眼／叫声／泼沙素材。
位置按真实精灵绘制边界，尺寸按 OSTB 比例；状态招式只在原提交结果确实改变对方能力
时播放，不延迟状态提交／日志，不改伤害／HP／回合时序；混乱自伤不前冲。
四配置各 36 原旅程与新增返回／取消／双方九类特效断言通过；72 张新特效关键帧、
OSTB 各 18 张旧受击帧已审查，独立新增 explore_ui_battle_fx 期望，原 fixture 字节保留。
Stick 647 帧不变，冻结地图的生态两配置各 38 帧亦不变；OSTB debug ASan／UBSan 的
407 帧与普通版一致，四配置 UI 严格语法及家族源／架构／调度检查通过。
相关页面／feature／原生 17 个脚本通过；探索每配置仍保留 12 张旧地图差异。另一个窗口
正在改地图：生态 host 本批改前已输出 actors=13，而 runner 仍预期 12，保留该失败。
OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过；
未访问串口、未刷机，实机观感待验收。冻结源／隔离对照／截图／验证见
.stick-local/artifacts/ostb-explore-resume-battle-fx-20261025 与隔离文档末节。


2026-10-25 古瀑谷区域 v24：用户要求把已确认森林与高桥写入项目。正式探索从森林 A
进入，六种森林按双向边界连接，E 通原四洞穴、F 通高桥；保留原山洞 A/B/C/D。
共 13 种物理样式／14 个导航节点（高桥 A 上下层共图）。高桥固定 A 下→B 登高→A 上桥→C 下行；
内部换图不额外计探索段，Boss／最终奖励／结束使用所选路径的 terminal 语义，避免段数上限截断高桥。
森林有两组确认配方，高桥四种骨架各四组细节，共 16 套完整组合；整次探险种子固定，重入不重抽。
这是有限配方池，不应描述成每个种子唯一或无限随机。维护源 tools/valley_forest_design.json、
tools/valley_bridge_layouts.py 和 tools/valley_region.py；生成 ExploreValleyData.h。
原素材桥取自 origin_asset/source_maps/meteor_falls_emerald.png 的 (304,288)-(336,320)，
其余图块来自 Essentials；未生成或重绘美术。新增 132 个配方、209 个 Kind，旧 565 个编号不变。
地图包 640 帧／2748 palette words／41794 B；两端上限 768 帧／4096 palette words。
地图／资源／区域／旧洞穴／生态／草坪专项通过；原生绘制检查 896 图、7168 动画帧及新物体遮挡；
Stick/OSTB/AMOLED 实际区域切图及高桥段数边界通过，四目标构建通过。
扩展 18 脚本 16 通过（含 AMOLED 原生渲染 80 项）；旧探索像素和 AMOLED 路线黄金记录仍有差异，
未更新 fixture。共同捕获的两家族各 323 帧相对任务前冻结源只有六张 area-5 地图变化；
冻结 OSTB 旧菜单动作断言未完成，当前行程完成，不宣称冻结流程全过。
详情 doc/古瀑谷布局与瀑布衔接.md 与 .stick-local/artifacts/valley-region-integration-20261025/validation.json。
未刷机，设备观感与性能待验收。


2026-10-25 OSTB 探索菜单返回行为修正（以本节为准）：用户明确要求点“返回”后保持
暂停，必须再点地图才行进，替代上一批返回即行进的交互。compact_touch 派发
MENU_RETURN_PAUSE，只关菜单并关闭自动行进；未完成的领队／随行单步冻结位置与
行走姿态，再次有效地图点击补回暂停时长并续走，不重复计步。反复开关菜单不会重复
补时，取消点击不恢复；战斗特效保留，Stick 按键行为不变。四配置各 36 原检查点及
新增真实触摸／暂停／恢复断言通过，1,460 帧与本批冻结源一致，无需更新像素基线；
OSTB debug ASan／UBSan 407 帧一致，严格语法／家族源／架构／调度及相关 17 回归通过。
探索旧地图每配置 12 张差异与生态 actors=13／预期 12 的已有失败仍保留。OSTB
release／debug、Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过，未刷机。
源码冻结／对照／日志见 .stick-local/artifacts/ostb-menu-return-pause-20261025。


2026-10-25 OSTB 伙伴页顶部／底部导航：compact_touch 复用队伍 safePageHeader，标题
“伙伴”、返回圆心 (48,24)、左右 28px 边距与 48px 栏高；移除底部返回／选择／确认
及其触摸拦截。列表从 y=52 开始，弹窗统一下移 48px，命中与绘制同步；顶部返回按
确认窗→操作窗→电脑入口逐层退出。长列表补上 TouchScroll 上下跟手拖动，松手保留
位置、不触发条目，实体 B 切换仍自动显示当前项。共享绘制仅参数化，Stick 原像素
与按键保持，伙伴规则／存档／地图及 AMOLED 未改。两家族原 14 检查点及新增点击／
取消／滑动／末项可达断言、OSTB ASan／UBSan 48 帧、四配置严格语法／家族源／架构／
调度和最终 17 个相关回归通过。Stick 48 帧、OSTB 6 张电脑画面不变，伙伴 42 帧
有意变化，审查后另存 contacts_ui_header 期望，原基线保留；原生旧导航预期同步更新。
探索旧地图 12 张／配置与生态 actors=13／预期 12 失败仍保留。OSTB release／debug、
Stick release、AMOLED V1 lite 与 Debug --dry-run 构建通过，未刷机。证据见
.stick-local/artifacts/ostb-contacts-header-20261025 和隔离文档末节。


2026-10-25 OSTB 通讯录微雪式改版（替代上节旧列表保留说明）：移除列表和操作窗的“返回”项，
仅顶部返回逐层退出；72px 头像行显示实时成员名、羁绊、队伍旗、做客／不可邀状态及容量。
操作窗点外部关闭且不穿透，邀请／删除使用带名字的居中确认框，删除默认取消；保留跟手
拖动与实体键自动显示游标。共用 ContactsUiBase 只增加家族返回项策略，Stick 默认行为不变，
业务仍走原联系人能力及引擎服务。两家族各48帧／14检查点、真实邀请切场景／删除、
ASan／UBSan、隔离源码像素对照及17项相关回归通过。探索旧地图基线和生态13／预期12
差异保留。OSTB release／debug、Stick release、AMOLED V1 lite 与 Debug dry-run 构建通过，
未访问串口、未刷机。证据见 .stick-local/artifacts/ostb-contacts-amoled-style-20261025。


2026-10-29 商店余额／背包顶栏：余额固定改为左上 (32,80,72,28)，金额点击优先，
保留精确／万字缩写和查看完整金额；购买／出售文字、动画和商品几何不变。
背包使用 round_touch 独立 UI，240×240 原生正文网格从 y=0 开始，上方物理返回＋
20px“背包”复用商店导航；使用确认、取消／拖动、末项处理仍走共享业务。
RoundSurface 在组合时绘制顶部栏，仅返回区域接受页面外触摸；每帧及页面导航清理，
探索内嵌背包可返回且不残留热区，确认弹窗阻断返回；新增背／包两个 A4 字形，旧八字逐字保留。
完整真实引擎 ASan/UBSan（含主菜单／探索背包、取消、确认、交易、余额和停绘）、
家族源 9 项／架构 13 项与谷仓构建通过；94 张原有帧中 73 张不变，21 张仅商店金额
及背包变化，另新增探索背包／返回两帧。app 2,562,768 B、SHA256 前缀 68087ac127
已只更新同设备 OTA0 0x20000，写入哈希通过、启动前低 128KiB 回读一致；原档恢复，
音量 0／PA=OFF，首帧 601ms、精灵 687ms，25 秒无启动异常。实机观感待确认。
证据对应 `.stick-local/artifacts/sdgoods-shop-bag-header-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/shop-bag-header-20261029/`。


2026-10-29 电脑／队伍／房间入口顶部统一：谷仓独立 round_touch 实现复用主菜单
顶部返回键 (138,12,40,40) 与 20px 标题，正文上移至中央 240px 表面起点；
绘制／触摸同步，队伍弹窗点顶部先关闭，取消不穿透，探索内嵌队伍同样支持。
状态／技能等子页及其他设备实现不改，追加电脑／队伍／房间六字 A4 字形，旧十字逐字保留。
真实引擎 ASan/UBSan 完整旅程、引导返回、弹窗／取消及探索返回通过，家族源 9 项／
架构 13 项与谷仓构建通过。96 张既有帧中 90 张不变，6 张仅入口／队伍弹窗／引导变化；
圆屏遮罩预览已检查。app 2,564,672 B，SHA256 前缀 a4efde8e7c 已只更新同设备
OTA0 0x20000，写入哈希通过，启动前低 128KiB 回读一致；原档恢复、音量 0／PA=OFF，
首帧 596ms、精灵 682ms，25 秒未见启动异常。实机显示／触摸观感待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-page-headers-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/page-headers-20261029/`。


2026-10-29 谷仓主房间底部消息：普通喂食／互动 toast 下移到 (76,264,208,28)，
升级提示改为底部 (76,270,208,58) 圆角面板，文案“点击继续”，整块面板松手确认。
绘制与触摸共用 ProgressionLayout，圆屏启用，Stick／OSTB 原布局及按键语义保留；
确认仍调用原升级确认流程，提示期间阻断房间／菜单，拖动、取消、移出及重复 UP 不确认。
真实引擎 ASan/UBSan 完整旅程、新增升级确认／存档检查、圆屏遮罩预览、家族源 9 项、
架构 13 项及谷仓构建通过；Stick／OSTB release 各 11 检查点和各 111 张原像素通过。
圆屏原 96 帧中 95 帧不变，仅普通食物 toast 下移；另增 3 帧升级／交互检查画面。
app 2,565,296 B，SHA256 前缀 a2f4f8e4cc 已只更新同设备 OTA0 0x20000，
写入哈希通过，启动前低 128KiB 回读一致；原档恢复，音量 0／PA=OFF，
首帧 585ms、精灵 671ms；25 秒采集未见启动异常，实际新提示观感／触摸待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-home-notice-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/home-notice-20261029/`。


2026-10-29 谷仓升级提示绿宝石风格：底部对话框 x=-4／y=270／w=368／h=98，
浅米白底与深蓝双层像素边框，侧边／底边延伸屏幕外，圆屏自然裁切。正文位于
中央安全区，移除“点击继续”，右下 9×7px 红色倒三角配深色描边；按 160ms 一帧
的 0／1／2／1px 位移循环，周期 640ms、不闪烁。整块可见对话框松手确认，
保留拖动／取消／移出防误触和原业务确认；普通 toast 与其他机型布局不变。
真实引擎 ASan/UBSan 完整旅程及五个动画相位检查通过：仅箭头位移、红像素数固定，
正文／框体不动；扩大触摸范围、确认持久化及防穿透通过。家族源 9 项／架构 13 项、
谷仓构建和 Stick／OSTB release 各 11 检查点／各 111 帧原像素通过。圆屏已有
99 帧中 98 帧不变，仅升级提示改变，另增五张箭头相位；圆屏遮罩与动画预览已检查。
app 2,565,488 B，SHA256 前缀 3c36ffb03d 已只更新同设备 OTA0 0x20000，
写入哈希通过、启动前低 128KiB 回读一致；原存档恢复，音量 0／PA=OFF，
首帧 471ms、精灵 558ms，25 秒未见启动异常。新对话框与动画实机观感待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-emerald-notice-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/emerald-notice-20261029/`。


2026-10-29 谷仓升级面板降低：顶部 y=270→286，下移 16px，可见白色高度
90→74px；正文和箭头跟随共用矩形下移，底边仍延伸屏幕外。点击区同步，旧白色
条带不再确认；保留 9×7px 小箭头、2px 浮动／640ms 周期和拖动取消。
真实引擎 ASan/UBSan 完整旅程、五相位／命中／保存确认、家族源 9 项／架构 13 项
及谷仓构建通过；104 帧中 98 帧不变，6 张仅升级提示／箭头位置变化，圆屏预览
确认文字和箭头完整可见。app 2,565,488 B，SHA256 前缀 ce26882cc3 已仅更新
同设备 OTA0 0x20000，写入哈希及启动前低 128KiB 回读通过；原档恢复，
音量 0／PA=OFF，首帧 579ms、精灵 664ms，25 秒未见启动异常。
面板新高度实机观感待用户确认。证据对应 sdgoods-dialogue-lower-20261029，
固件对应 `.stick-local/firmware/sdgoods/dialogue-lower-20261029/`。


2026-10-29 谷仓伙伴页导航／底部容量：round_touch 独立 ContactsUi 复用主菜单
返回键和 20px“伙伴”标题，240px 原生列表从物理 y=60 开始，数量固定在物理
底部居中 (180,324)，16px 灰字，不随列表滚动。数量位于正文外且不接受点击，
不会遮挡末项；返回仍按确认框→操作菜单→电脑入口逐级退出，取消不穿透。
SceneSurface 复制短页脚文本，开始帧／导航时清理，离页无残留；新增伙／伴两字
16／20px A4 字形，旧 16 字逐字保留。其他机型源和共享伙伴业务不改。
真实引擎 ASan/UBSan 全旅程、0／1／20容量／长列表末项／滚动与弹窗时页脚固定、
页脚输入隔离及逐级返回通过；家族源 9 项／架构 13 项和谷仓构建通过。旧 104 帧
中 100 帧不变，4 张仅伙伴列表／弹窗变化，新增 6 张容量／滚动／返回检查画面；
圆屏遮罩预览已检查。app 2,566,272 B，SHA256 前缀 84542b624f，已只更新同设备
OTA0 0x20000；写入哈希与启动前低 128KiB 回读通过，原档恢复、音量 0／PA=OFF，
首帧 565ms、精灵 650ms，25 秒未见启动异常。新布局实机观感待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-contacts-header-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/contacts-header-20261029/`。


2026-10-29 谷仓设置页顶部：新增 round_touch SettingsUi，设置／操作说明／重置游戏
复用主菜单原生返回键和 20px 标题，点击区域同步。240px 原生列表使用全高视口，
正文上移，绘制／命中／滚动／实体键 reveal 共用几何；帮助正文同步上移，保留翻页。
设置／滑杆／帮助／重置业务继续复用 SettingsUiBase 和 SettingsHost，返回逐层退出，
取消和拖出不激活，重置默认取消。新增标题九字的 16／20px A4 字形，旧 18 字逐字保留。
真实引擎 ASan/UBSan 全旅程、顶部取消／拖出、滑杆与纵向滚动轴向隔离、帮助／重置
末项可达、模式目标隔离／逐层返回通过，家族源 9 项／架构 13 项及谷仓构建通过。
上一版 110 帧中 101 帧不变，9 张变化仅在设置页面组，新增 4 张设置导航画面；
圆屏遮罩预览已检查。app 2,569,280 B，SHA256 前缀 4579c775f3，已只更新同设备
OTA0 0x20000，写入哈希及启动前低 128KiB 回读通过。原档恢复、音量 0／PA=OFF，
首帧 475ms、精灵 560ms，25 秒未见启动异常；新顶部实机观感待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-settings-header-20261029/`，
固件对应 `.stick-local/firmware/sdgoods/settings-header-20261029/`。


2026-10-30 谷仓成功提示精简：round_touch 房间不再绘制“已添加食物”，菜单通知入口
过滤“已切换伙伴”，避免快速切页后残留；隐藏的喂食提示不会挡住新手提示。仅调整展示，喂食／
伙伴切换／自动保存与共享业务保持，食物不足／食盆已满等其他提示保留。
真实引擎 ASan/UBSan 完整旅程、喂食／跨进程恢复与错误保护通过；家族源 9 项、
架构 13 项和谷仓构建通过。上一版 114 帧中 113 帧一致，仅喂食成功画面去掉提示；
伙伴提示在通知保存前按文本内容过滤；其他机型默认透传。
app 2,569,376 B，SHA256 前缀 8213ae2ac5，已只更新同设备 OTA0 0x20000；
写入哈希及启动前低 128KiB 回读通过，原档恢复、音量 0／PA=OFF，
首帧 595ms、精灵 679ms，25 秒未见启动异常。
新版实机交互待用户确认。证据对应 `.stick-local/artifacts/sdgoods-hide-success-toast-20261030/`，
固件对应 `.stick-local/firmware/sdgoods/hide-success-toast-20261030/`。


2026-10-30 谷仓状态页全屏／圆弧页点：新增独立 round_touch StatusUi，状态／初始蛋
顶部复用主菜单返回键和原生 20px 标题，移除 1/5 数字。采用原生 360×360 表面，
正文背景从 y=52 延伸到屏幕边缘／底部；文字保持圆屏安全区域 (48,64,264,248)，
五页原内容／数值共用 StatusContent，不缩放整帧。五个圆点按圆心 (180,180)、
半径 166、间隔 10 度沿底弧排列，中心约 y=336／343／346／343／336；选中青色
半径 3，其余灰色半径 2，无原胶囊背景。顶部和页点固定，文字仍支持上下滚动。
左右跟手／反向／回弹／90px 松手翻页保留，双页缓存 443,520 B，内存失败退化为
松手翻页；取消／离页释放。SceneSurface 按导航选择状态原生表面，返回队伍／伙伴
和探索内子页恢复 240px 及触摸映射。新增状态／初始蛋五字 A4 子集，旧 27 字不变。
真实引擎 ASan/UBSan 全旅程、五页固定顶栏／圆弧页点、取消／反向／回弹／标题
翻页／纵向长内容／低内存退化／表面恢复和蛋状态通过；家族源 9 项、架构 13 项
及谷仓构建通过。上一版 114 帧中 104 帧一致，10 张变化仅状态页面组，另增 9 张
状态检查画面；已检查圆屏遮罩预览。app 2,571,248 B，SHA256 前缀 80ba810cff，
已只更新同设备 OTA0 0x20000；写入哈希／启动前低 128KiB 回读通过，原档恢复，
音量 0／PA=OFF，首帧 563ms、精灵 646ms，
25 秒未见启动异常。全屏／圆弧页点和手感实机观感待用户确认。
证据对应 `.stick-local/artifacts/sdgoods-status-round-20261030/`，
固件对应 `.stick-local/firmware/sdgoods/status-round-20261030/`。


2026-10-30 谷仓状态简介口味排版：性格独立一行，移除同行喜厌缩写；来源上方
新增左右两列“喜欢X味／讨厌X味”，绿色／粉红沿用微雪，无偏好时灰色“喜欢 -／
讨厌 -”。同步内容高度计算，原顶部、圆弧页点及翻页交互保持。已核对“慢吞吞”
对应 Mild，是现行宝可梦性格中文名，保留名称与原性格 ID；口味使用现有共享
FoodTuning 数据，未改玩法、存档或其他设备界面。完整 ASan/UBSan 旅程、偏好／
中性画面导出检查、家族源 9 项、架构 13 项及谷仓构建通过。上一版 123 帧中
118 帧逐字节一致，5 帧差异仅状态简介及其拖动，新增 2 张口味检查画面。
app 2,571,584 B，SHA256 前缀 a46d7a1ce5，已只更新同设备 OTA0
0x20000；写入哈希／启动前低 128KiB 回读通过，原档恢复、音量 0／PA=OFF，
首帧 489ms、精灵 572ms，25 秒未见启动异常。
本版实机排版观感待用户确认；证据对应 sdgoods-status-flavor-20261030，
固件对应 .stick-local/firmware/sdgoods/status-flavor-20261030。


2026-10-30 谷仓队伍弹窗／技能全屏：队伍点击精灵的操作弹窗宽 132→88px，
行高仍为 40px，文字左边距 12px，移除左侧黄色指示条；绘制和命中共用新几何，
点外部关闭且不穿透。技能新增独立 round_touch 展示实现，继续共用 MovesFeature／
MovesUiBase 的招式目录、遗忘和回忆规则。原生 360×360，顶部复用主菜单返回键
及 20px“技能”，正文背景从 y=52 铺满屏幕；列表和详情位于圆屏安全区，保持
44px 行高／6px 间距、描述换行与独立滚动、点击查看／再点执行和默认取消的遗忘
确认。技能与状态均由 SceneSurface 在主菜单／探索上下文选择原生表面，返回恢复
240px 队伍／背包及触摸映射。A4 字库新增“技能”两字，34字，旧32字逐字节不变。
真实引擎 ASan/UBSan 完整旅程与双成员弹窗／外部关闭、原生导航／拖动取消／遗忘
取消、真实回忆目录／替换返回／列表滚动和两上下文表面恢复通过。家族源9项、
架构13项、调度检查及谷仓／StickS3 release／AMOLED V1 lite 构建通过；字库重生成
确定性通过。125张原有帧中121张一致，仅队伍操作弹窗及3张技能画面变化，
新增8张检查画面；圆屏预览已检查。app 2,572,240 B，SHA256前缀 64c57efa5c，
已只更新同设备 OTA0 0x20000，写入哈希／启动前低128KiB回读通过，原档恢复，
音量0／PA=OFF，首帧 485ms、精灵 568ms，
25秒未见启动异常；本版实机排版和交互观感待用户确认。证据对应
.stick-local/artifacts/sdgoods-team-moves-20261030，固件对应
.stick-local/firmware/sdgoods/team-moves-20261030。


2026-10-30 谷仓伙伴图标纵向居中：round_touch ContactsUi 使用共享
frameVisibleBounds 读取图标的实际可见范围，按缩放后的上下边界对齐条目纵向
中心，避免素材透明留白不对称导致精灵偏下。图标比例／尺寸、水平位置、文字、
条目尺寸和点击区域保持；不修改资源、共享规则或其他设备界面。
真实引擎 ASan/UBSan 完整旅程、伙伴滚动／弹窗／返回、家族源9项和谷仓构建通过，
单只／列表图标导出预览已检查。133张原有帧中125张一致，8张差异仅伙伴列表
图标及其叠加弹窗；未更改旧fixture。app 2,572,272 B，SHA256前缀
47c3dae636，已只更新同设备 OTA0 0x20000；写入哈希／启动前低128KiB
回读通过，原档恢复、音量0／PA=OFF，首帧 469ms、精灵
552ms，25秒未见启动异常。实机图标观感待用户确认；
证据对应 .stick-local/artifacts/sdgoods-contact-icon-20261030，固件对应
.stick-local/firmware/sdgoods/contact-icon-20261030。


2026-10-30 谷仓伙伴卡片／探索选择页：伙伴改为原生 360px 表面，沿用主菜单返回＋
20px 标题，单列视口 (48,62,264,246)，cell 264×72、间距 10、圆角 12；左侧精灵
继续按实际可见范围纵向居中，右侧名称／队伍旗／羁绊／现有状态与箭头，整张点击，
无黄色选中条或滚动缩放。容量固定 y=324，弹窗遮罩不覆盖页脚；操作／确认及返回
仍走原联系人服务，状态返回伙伴保持原生表面，返回电脑恢复中央 240px 表面。
探索 SELECT 改为原生 360px，顶部复用主菜单导航；背景仅在 y>=52 绘制、延伸到底，
中央保留原精灵自动轮播及稀有未遇见剪影，场景名 y=248；出发 (120,282,120,40)，
指示器中心 y=339。左右箭头点击区 (16,126,44,64)／(300,126,44,64)，只切场景，
边界置灰；中央支持横向滑动，不缩放。仅显示原规则允许的区域及下个锁定区域，
锁定不可出发，饥饿反馈沿用共享规则。独立 RoundExplore::Selection 由 compact_touch
探索 UI 组合；ExploreScene 只传入选择／行走的表面语义，行走／战斗仍为中央 240px。
新导航 A4 字库新增“探索”两字，旧 34 字逐字节不变。
完整圆屏 ASan/UBSan、0/1/20 伙伴／末项／弹窗与容量固定、探索点击边缘／取消／
横纵隔离／单精灵轮播／六区域／锁定／饥饿／出发后表面恢复及跨进程存档通过；
133 张原帧中 123 张一致，10 张差异仅伙伴和探索选择页，已检查圆屏导出图。
谷仓／StickS3 release／OSTB release／AMOLED V1/V2 lite 构建及 OSTB native 通过。
旧探索回归在 Stick／OSTB release 的 ENDING 结算断言失败，已用本批修改前冻结源
复现，未更改旧基线。音量保持 0，本批只改代码和构建，未访问串口或刷机。
app 2,575,296 B，SHA256 前缀 f443619a7c；证据见
.stick-local/artifacts/sdgoods-cells-explore-20261030，导出固件见
.stick-local/firmware/sdgoods/cells-explore-20261030。

2026-10-30 用户授权后，伙伴卡片／探索选择页固件已只更新同一谷仓设备
（MAC ec:da:3b:67:10:5c）的 OTA0 0x20000；app 2,575,296 B，SHA256 前缀
f443619a7c，写入哈希验证通过。更新前备份低 0x2a0000，完整覆盖旧应用，
启动前低 128KiB 回读与备份一致，原分区、引导元数据与 NVS 保留。
启动日志确认原已孵化存档恢复，音量保持 0／PA=OFF，首帧 506ms、精灵就绪
591ms；25 秒采集未见启动异常。串口已关闭，新伙伴卡片／探索选择页实机
显示与交互仍待用户确认。刷入及启动证据保存在本批 artifacts 目录，
firmware manifest 已记录刷入状态与备份信息。

2026-10-31 用户确认后接入青草径／雾林径 v26。青草径三风格、两片草甸与花簇，
15% 小围栏林地；维护源 tools/grass_path_layouts.py，经 tools/grass_path.py
编译 96 组方向／样例配方，运行时按种子选择，包含十张确认样例。
雾林采用最后确认的 S 弯／双路／草甸绕行／折线／横廊／三路方案，第 2 段保留；
Room 路线容量增至 3，6～8 段衔接保持。65 张组合图占用既有 75 个资源槽，
其余槽保留有效素材，后续 Kind 编号不移动。地图包与代码须同步更新。
17 张确认样例一致性、地图／植被／资源／草坪、Python/C++ 指纹、原生行程和
StickS3 release／谷仓／AMOLED V1 lite 构建通过；未刷机，实机体验待确认。
详见地图生成规则 v26 补充，证据位于
.stick-local/artifacts/grass-mist-integration-20261031/。

2026-10-31 谷仓探索选择页地图导航上移：地图名居中放在“探索”下方 y=72，
左右箭头与名称同行，点击区分别为 (66,58,44,44)／(250,58,44,44)，
绘制与命中共用几何；“大量出现”移至轮播下方 y=248，避免与地图名重叠。
完整圆屏真实引擎 ASan/UBSan 旅程、箭头边缘／边界／取消、横滑／锁定／饥饿／
出发后表面恢复及跨进程存档检查通过，圆屏预览已检查，谷仓构建通过。
本批只改圆屏布局和既有回归坐标，未访问串口、未刷机，音量与实机存档未触动。
构建包含工作区既有 v26 地图更新；app 2,656,864 B，SHA256 前缀 d0985fc430。
证据见 .stick-local/artifacts/sdgoods-explore-heading-20261031/，固件见
.stick-local/firmware/sdgoods/explore-heading-20261031/。

2026-10-31 用户授权后，探索选择页地图导航上移版已只更新同一谷仓设备 OTA0
0x20000，app 2,656,864 B、SHA256 前缀 d0985fc430，包含既有 v26 地图更新。
更新前备份低 0x2b0000，写入哈希通过，启动前低 128KiB 回读一致；原已孵化
存档恢复，音量保持 0／PA=OFF，首帧 493ms、精灵就绪 577ms。25 秒采集未见
启动异常，串口已关闭；新版地图导航与地图实机显示／交互仍待用户确认。
刷入证据和备份分别位于本批 artifacts 与 backups 目录，firmware manifest 已更新。

2026-10-31 谷仓探索选择页轮播下移 16px：精灵中心 y=156→172，裁切窗口
同步由 (60,90,240,140) 移至 (60,106,240,140)，底部留白更均衡且不新增裁切。
完整圆屏 ASan/UBSan 旅程、原探索交互／单精灵轮播检查及谷仓构建通过，已检查
圆屏导出图；142 张既有帧中 134 张一致，8 张差异均限于探索选择页精灵区域。
app 2,656,864 B，SHA256 前缀 eff3cf69d6；未访问串口、未刷机，实机音量未触动。
证据见 .stick-local/artifacts/sdgoods-explore-carousel-lower-20261031/，固件见
.stick-local/firmware/sdgoods/explore-carousel-lower-20261031/。


2026-10-31 谷仓探索地图圆屏适配：独立 round_touch ExploreRouteUi 使用原生 360×360
视口，顶部地图名 (116,19,128,32)，20px 更纱字；HP 不放精灵头像，双成员条分别
从 x99／187 开始，52×7，右侧各保留 16×16 异常状态位置，使用原 12px 图标；
单成员居中。底部菜单 (158,296,44,44)，右侧四项菜单 (236,88,92,184)，
队伍／背包／结束／返回，保留黄色选中条与文字，地图不加遮罩。打开菜单暂停，
返回后保持暂停，下一次点地图继续；菜单外点击不穿透，底部按钮可收起菜单。
移除 WALK 教程文字，包含首次探索；教程完成逻辑保留。地图按原像素尺寸绘制，
视口大于地图高度时居中并以场景底色补足，上下各24px，不拉伸或创造可行走地形。
行走／退出／拾取／挑战／结束提示使用360px；战斗和结算／成长页保留中央240px。
Scene 的 prepareSurface 在输入映射与帧绑定前同步表面，探索队伍／背包及其
原生状态／技能子页逐层返回时恢复正确坐标。业务规则和存档schema未修改。
完整圆屏 ASan/UBSan 旅程、圆边取消／右菜单边角／外部隔离、暂停恢复、嵌套
子页／战斗表面切换、首次提示隐藏及跨进程存档／错误保护通过；家族源10项、
架构13项、调度2项和谷仓／StickS3 release／AMOLED V1 lite 构建通过。
142张原帧中136张一致，6张差异只在探索地图／菜单及其教程，新增6张检查图；
圆屏预览已检查。A4子集56字，旧36字逐字节保留，重生成确定性通过。
app 2,664,448 B，SHA256前缀 47ee2e9b82；包含既有v26地图及探索轮播下移。
证据见 .stick-local/artifacts/sdgoods-explore-round-20261031，固件见
.stick-local/firmware/sdgoods/explore-round-20261031。实机显示与交互观感待用户确认。

2026-10-31 用户授权后，上述探索地图圆屏版已只更新同一谷仓设备
（MAC ec:da:3b:67:10:5c）OTA0 0x20000，app 2,664,448 B／47ee2e9b82。
更新前低0x2b0000备份覆盖原应用，核对设备32MiB、分区及原应用一致；写入哈希
和启动前低128KiB回读均通过，原分区／引导记录／NVS保留。启动恢复原已孵化
存档，音量0／PA=OFF，首帧465ms、精灵549ms，25秒采集未见启动异常。
串口已关闭，新探索布局实机显示／点击观感待用户确认；备份、刷入与启动证据
位于本批对应backups／artifacts目录，固件manifest已记录。


2026-10-31 谷仓探索上下留空／卡顿修复：RoundExplore 的 routeLayout 开启
extendMapEdges，在原416×312地图之外按原26px尺寸延展最外侧图块行，补齐
圆屏上下各24px；六区域导出图均检查通过。只扩展表现层，不改变地图／路径／
碰撞或精灵比例，不使用整图缓存。
卡顿定位到原生1倍图块仍执行通用缩放矩形的逐像素裁切和fill，以及WALKING
无条件持续重绘。Canvas565 增加原生1倍透明图块按行裁切／写入路径；保持
透明mask、裁切、RGB565字节序和缩放回退。探索菜单打开或触摸暂停时停绘，
生态动画同步暂停；恢复时仍由既有逻辑重新对齐时间。未修改存档schema。
真实ESP32-S3／PSRAM对照：360×360视口、26px图块、三层、每路径20帧，
全不透明mask旧265775µs→新51411µs（5.170倍），含透明mask旧222745µs→
新49030µs（4.543倍），像素完全一致。这是绘制核心基准，不是整机FPS；
临时诊断代码与LegacyCanvas仅保存在本地证据中，正式固件不包含测速代码。
Canvas裁切／负坐标／非整字节mask行／双字节序及完整圆屏ASan/UBSan旅程、
菜单和暂停停绘／恢复／子页返回／存档保护通过；家族源10项、架构13项、调度2项
及谷仓／StickS3 release／AMOLED V1 lite构建通过。148张原帧中137张一致；
11张变化仅探索地图，8张原地图范围完全一致，另3张来自新增暂停检查改变了
fixture动画时刻，未更改基线。新增六区域边缘及背包返回检查帧。
app 2,664,768 B，SHA256前缀66486c1bc5；证据见
.stick-local/artifacts/sdgoods-explore-performance-20261031，固件见
.stick-local/firmware/sdgoods/explore-performance-20261031。

2026-10-31 上述边缘／性能修复正式版已仅更新同一谷仓设备OTA0 0x20000，
app 2,664,768 B／66486c1bc5。写入校验通过；测速前完整低0x2c0000备份、
正式更新前最新低128KiB备份均保留，启动前回读与最新备份一致。原已孵化存档
恢复，音量0／PA=OFF，首帧553ms、精灵637ms；25秒启动采集无异常，串口已关闭。
正式固件不含测速代码，实机探索流畅度及上下地形观感待用户复测。


2026-10-31 谷仓战斗圆屏改版（仅代码／构建，未刷机）：ENCOUNTER 与 FRIENDSHIP
切换到原生360×360，RoundExplore::battleLayout 集中定义布局；共享绘制读取布局，
仍复用战斗／经验／日志时序。背景铺满，敌方信息卡152px、HP104px；我方信息卡
146×43，只显示HP，击败对手后原EXP_GAIN日志节点将HP区切换为EXP，保留等级
随经验动画更新和升级／进化／技能／结交流程。四指令56×44，沿下方浅弧排列，
去除整块底板；消息框通常228×46，最新长消息换行，极长消息最多三行临时上扩18px，
不改变队列或播放时序。异常位、投食／换人／攻击坐标、44px结交行与首次战斗提示
已适配，绘制和点击共用几何；背包／成长页仍使用原表面，返回战斗恢复360px。
完整圆屏ASan/UBSan旅程、四指令边角／取消／busy隔离／背包返回／换人／逃跑、
HP与EXP像素／实际奖励触发、大型精灵／长消息／结交／跨进程存档通过；155张既有帧
151张逐字节一致，仅4张战斗相关帧变化。家族10项、架构13项、调度2项及最终UI
两家族release/debug回归通过；谷仓／StickS3 release／AMOLED V1 lite构建通过。
旧探索回归在END阶段调度断言（host第398行）失败，已用本批修改前冻结源复现，
未修改旧基线或共享调度。app 2,666,272 B，SHA256 99d3c0a754406baa0c8246e0c6784417881086bd4d2f4e73b87330496b0e71f8。
证据见.stick-local/artifacts/sdgoods-battle-round-20261031，固件见
.stick-local/firmware/sdgoods/battle-round-20261031。用户要求先不刷入：未访问串口，
设备仍运行上一版探索性能修复固件；新战斗布局实机观感待后续授权刷入验收。

2026-10-31 用户授权刷入战斗改版后，最新源码重新构建包含 Canvas565 后续颜色
变换优化；完整圆屏 ASan/UBSan 旅程及 Canvas 检查通过，177 张画面与战斗验收版
逐字节一致。新固件 2,666,368 B，SHA256 前缀 de349ebf08，保存在
.stick-local/firmware/sdgoods/battle-round-flash-20261031/，原验收固件保留。
已只更新同一谷仓设备 OTA0 0x20000，低 0x2c0000 完整备份、写入哈希与
启动前低 128KiB 回读校验通过；原存档恢复，音量 0／PA=OFF，首帧 489ms、
精灵就绪 573ms，25 秒启动日志未见异常，串口已关闭。新战斗界面实机显示／
交互仍待用户确认；刷入和补验收证据保存在本批 sdgoods-battle-round artifacts。

2026-10-31 谷仓战斗字体清晰度调整：浮动战斗信息卡名称、消息及结交提示使用
原生16px更纱A4抗锯齿，四指令使用20px；英文／数字／等级及EXP标签保留原像素
字形。首次战斗提示同步16px抗锯齿，点击几何和战斗／经验／日志时序不改。
16px字库以共享文字源和现有zh16字符表补齐至1158字，20px补指令至62字；两种
字号的原56字逐字节保留，生成确定性及共享字库覆盖率检查通过。
完整圆屏ASan/UBSan旅程、新增抗锯齿／像素数字检查、坏字库回退、跨进程恢复、
家族10项／架构13项、StickS3 release/debug原像素回归及谷仓／StickS3 release／
AMOLED V1 lite构建通过。177张圆屏旧帧中152张一致，25张变化仅战斗文字，
已检查圆屏预览。app 2,813,488 B，SHA256前缀3ae22cc161；未访问串口、未刷机，
实机存档与音量未触动。证据见.stick-local/artifacts/sdgoods-battle-fonts-20261031，
固件见.stick-local/firmware/sdgoods/battle-fonts-20261031。


2026-10-31 微雪V2探索第二批：用户选降低绘制成本和绘制/传屏并行，切图停顿延期，
不改存档策略或帧率节奏。圆角标题混色表整块复用；V2新增329,728B PSRAM显示
帧缓冲、core1单在途帧worker，下一帧绘制与上一帧传屏重叠，交换只补齐未覆盖
旧脏区。场景/淡入淡出/锁屏前排空；失败回退，DMA超时禁止复用内存并停显示。
无新世界地图缓存，V1传屏保持同步。固定输入实机p95：青草白天50→33.35ms、
夜间60→49.45ms，古瀑白天60→38.98ms、夜间70→55.06ms；这是绘制+传屏探针，
不等于游戏FPS，古瀑未解锁。正式白天随机路线两滚动窗口约31.8/29.4FPS，样本短。
2244探索帧一致，Canvas/worker ASan/UBSan、脏区/失败/超时、架构/调度检查及
V1 lite/V2 release+debug/StickS3 release构建通过；native仍79/80唯一旧水桥资源断言。
V2 lite debug应用2,077,792B、SHA256前缀10c9b2ba67ec，仅更新OTA0并校验，按本会话
要求不备份。临时测速已移除，重启回房RAM/磁盘一致：144步、1358币、Lv51喷火龙
HP147/148、药水4；串口关闭。未做相机观感验收，不承诺夜间稳定30FPS。报告见
 doc/探索渲染优化与实机验收.md，证据.stick-local/artifacts/amoled-scroll-pipeline-20261031/。


2026-10-31 谷仓探索弧形 HP（仅代码／构建，未刷机）：移除地图名下方共用 HP 底板，
两只精灵分别显示在地图名左右的圆屏内缘，半径164.5px、弧长约60px、宽7px，
圆角端点与四采样抗锯齿；满血至低血沿用绿／黄／红，濒死灰色空槽，单只固定左侧。
异常状态使用原12px图标，分别位于(82,72,20,20)与(257,72,20,20)预留区；正常留空。
顶部20px地图名保持；弧槽28px触摸保护带和异常区域阻断地图操作，原中央HP区域
恢复点图行走。绘制无逐帧三角函数或动态分配，仅修改round_touch展示与对应host检查。
完整圆屏ASan/UBSan旅程、0/1/2成员、满血／阈值／零血／零上限／越界血量／濒死、
像素镜像／圆边／状态清除／触摸取消与暂停恢复、跨进程存档保护通过；家族源10项、
架构13项及谷仓构建通过。177张既有帧中160张一致，17张差异仅HP与异常状态区域，
新增5张HP检查图，已检查单／双／异常状态圆屏预览。固件2,814,608 B，
SHA256 0e4076dee10eec0f9997e5e7f7e4f98546c8c51a1d128ed5eba58c2bd8dc49e2。包含此前未刷入的战斗字体调整。
本次未访问串口、未刷入，设备存档及音量0未触动；实机观感待后续验收。
证据见.stick-local/artifacts/sdgoods-route-arc-hp-20261031/，固件见
.stick-local/firmware/sdgoods/route-arc-hp-20261031/。


2026-11-01 谷仓探索弧形HP对比度：原7px槽体的1px浅绿边改为纯白，外加2px
不透明深色外沿，内部5px血量与原圆弧位置、填充方向和异常图标保持；濒死保留
灰色槽及浅灰边。解决去掉共用深色底板后，HP在草地／沙地上与背景混合的问题。
仅改round_touch绘制及既有host像素边界预期；触摸热区、玩法、存档与资源不改。
完整圆屏ASan/UBSan旅程、弧槽／单双成员／HP边界／状态／暂停恢复／存档保护及
谷仓构建通过，已检查前后对比和六区域圆屏导出图。182张旧帧中160张完全一致，
22张变化仅弧槽描边区域（其中5张为既有HP检查图），未修改旧fixture。
app 2,814,688 B，SHA256 41580ff6e434c03a1254bfa89e05f447898e436b1cb9eeced2fa81517ded4fbc。
本次未访问设备、未刷机，音量0和实机存档未触动；实机观感待后续验收。
证据见.stick-local/artifacts/sdgoods-route-hp-contrast-20261101/；固件见
.stick-local/firmware/sdgoods/route-hp-contrast-20261101/。


2026-11-01 微雪夜间探索着色优化：复用原832×624世界缓存中的2倍重复行，偶数行
保留夜色、奇数行原色；不增加整图缓存或堆内存。视口静态区域直接取夜色，动画/
草丛/精灵阴影/头目/拾取覆盖图块取原色，原顺序合成后仅这些区域着色一次。
构建检查重复行，不满足时保持原色回退；黎明/黄昏不随色调逐分钟重建，固定
夜色进入/退出各一次重建。切图/存档长帧未处理，刷新与并行传屏配置保持。
同V2固定路径、48帧、两侧均并行：古瀑夜间绘制53.04→46.23ms，吞吐18.72→
21.46FPS，p95 55.17→49.33ms；青草夜间47.94→34.63ms、20.70→28.57FPS、
p95 49.59→36.84ms。属于绘制+传屏基准，不等于游戏FPS，不承诺夜间30FPS。
2244探索帧与上批一致，新增128组无缓存对照及生态ASan/UBSan通过；native仍
79/80唯一旧水桥资源断言，架构13/AMOLED源5、V1 lite/V2 release+debug/StickS3
release构建通过。正式V2 lite debug应用2,079,632B，SHA256前缀c7cad46213c2，
仅刷OTA0并校验，遵循本会话不备份要求，临时探针已移除。完整结果见
 doc/探索渲染优化与实机验收.md；证据.stick-local/artifacts/amoled-night-cache-20261031/。

2026-11-01 上述夜间缓存版正常探索、拾取、逃跑、菜单结束及重启验证完成。
最终重启回房，RAM/磁盘摘要一致：216步、1441金币、51级喷火龙HP148/148、饱腹77、药水4、队伍1/仓库0；串口已关闭。正常测试增加23步/28金币并消耗饱腹，未重置或解锁进度。


2026-11-01 谷仓弧形HP描边层次纠正：用户确认由内至外为5px血量／空槽、1px纯黑
细描边、2px纯白外沿；修正上一版白色细边＋深色外沿，弧形位置、总宽度与触摸
保护带保持，仅调整round_touch两处颜色常量及注释。濒死空槽保持灰色。
完整圆屏ASan/UBSan交互旅程与谷仓构建通过，前后导出图已检查；182张既有帧中
160张一致，22张差异仅HP边框区域。app 2,814,656 B，SHA256
89af46370f2086b1dab53666c49a34cfa6cb2575e01d2b5c648d01d4dd3fee9b。包含前批未刷入战斗字体。
本次未访问设备、未刷机，未改音量和实机存档；实机观感待后续验收。
证据见.stick-local/artifacts/sdgoods-route-hp-white-edge-20261101/，固件见
.stick-local/firmware/sdgoods/route-hp-white-edge-20261101/。


2026-11-01 谷仓探索异常状态接弧槽终点并刷入Debug：原12px状态图标接在远离
地图名的固定外侧终点，中心(47,80)/(312,80)，20px保护区(37,70)/(302,70)，
沿弧继续向下、与白色外沿约3-4px间隔；不随当前HP变化，正常留空，单双成员位置稳定。
状态绘制与点击共用几何，原状态占用区恢复地图操作。1px黑细边／2px白外沿保持。
谷仓CMake新增默认OFF的Debug/trace选项，build-debug独立构建；round_touch主菜单
使用编译期开关显示Debug入口（Debug共9项，普通版8项，均含返回），复用原DebugUi。
启动日志打印实际开关，未修改共享玩法、存档schema、资源或其他硬件代码。
普通版完整圆屏ASan/UBSan旅程、Debug菜单入口/返回与探索检查、家族源10项、架构
13项及谷仓普通/Debug构建通过；61个板级/UI编译单元确认debug=1/trace=1，普通版
仍为0。182张普通版既有帧中180张一致，仅两张异常状态图标位移，圆屏预览已检查。
最终Debug app 2,830,784 B，SHA256 029b73d3145f3d69f6c0d2b3d878c22b647ee565ed3c0a25147b9266ed6b6c59。
已仅更新原谷仓设备ec:da:3b:67:10:5c的OTA0 0x20000，旧app与上次刷入版本一致；
更新前低0x320000完整备份，写入哈希验证及启动前低128KiB回读一致，引导/分区/NVS
保留。启动确认debug=1/trace=1、原已孵化存档恢复，音量0/PA=OFF，首帧511ms、
精灵就绪596ms，25秒采集未见启动异常，串口已关闭；新布局实机观感待用户确认。
固件包含此前未刷入的战斗字体及弧形HP调整；未在设备重置、改档或试听。
证据见.stick-local/artifacts/sdgoods-route-status-debug-20261101/，备份见对应backups目录，
固件见.stick-local/firmware/sdgoods/route-status-debug-20261101/。

2026-11-01 长草园 P1 生成提速：共享 ExploreMapGenerator 的选树候选只评分一次，
两遍曼哈顿距离表保留距离上限 6，种草容量只扣除新树影响的四行；保持遍历顺序与
PRNG 消耗，无堆分配，不改地图资源和算法版本。4096 张地图三轮前后完整图层／
路线／指纹一致，9360 组候选与 49920 个距离参考对照、ASan/UBSan 和地图回归通过。
桌面 -O2 三轮生成总耗时中位数 1570.34→435.88 ms（下降 72.24%），不代表 ESP32
实测或行走 FPS。谷仓 build-native／StickS3 release／AMOLED V1 lite 构建通过；
未刷机，谷仓实机收益待验收。P2/P3 尚未实施。详见 doc/长草园生成性能优化.md，
证据位于 .stick-local/artifacts/park-p1-20261101/。


2026-11-01 谷仓探索精灵恢复1倍：ExploreRouteLayout增加actorScale（默认0.8），
仅round_touch设为1.0；队伍首位/跟随与地图头目共用新比例，原像素图宽高增加25%，
原脚底锚点/浮空/动作偏移/阴影/植被遮挡边界同步使用同一比例。地图图块、镜头、
玩法与存档不改；其他家族保留默认0.8。1倍直接走原生像素素材绘制路径。
完整谷仓ASan/UBSan交互与跨进程存档检查、家族源10项、架构13项以及谷仓Debug/
StickS3 release/AMOLED V1 lite构建通过。182张旧帧中165张一致，17张变化仅探索
精灵/阴影区域，已检查前后导出图。未覆盖旧fixture。
Debug app 2,830,160 B，SHA256 9f38bc14ade2ad4e72c23c28ae4166c4332e0339c7bd6e828682478b03b5b4bf。
本批构建包含工作区既有长草园P1提速，未修改其源码。未访问串口、未刷机，设备仍
运行上批029b73d314 Debug固件，实机存档与音量0未触动，1倍观感待刷入后验收。
证据见.stick-local/artifacts/sdgoods-route-native-sprites-20261101/，固件见
.stick-local/firmware/sdgoods/route-native-sprites-debug-20261101/。


2026-11-01 谷仓探索菜单移至顶部：原底部44px按钮及底板移除，地图名胶囊
(116,19,128,32)内组合20px场景名与24px三横线；菜单点击区(198,13,44,44)，
绘制共用点击区中心，地图名在保留菜单空间的74px宽区居中；菜单不可用时标题仍
居中。底部(158,296,44,44)恢复地图操作，右侧菜单与原暂停/恢复/返回逻辑保持。
仅修改round_touch布局/绘制和对应host坐标；不修改共享玩法、存档、资源或其他设备。
普通版完整圆屏ASan/UBSan旅程、Debug菜单/探索检查、44px四角命中/拖动取消/
底部释放/暂停恢复/嵌套页面/异常状态/存档保护通过，家族10项/架构13项及谷仓Debug
构建通过。182张旧帧中165张一致，17张变化严格限于标题胶囊与旧底部按钮区域，
已检查前后、菜单展开及双异常状态圆屏导出图。无额外面板或透明叠加逻辑。
Debug app 2,830,176 B，SHA256 b615752030ec9fc8c9f43892acaeda8b0cff3e364fc83909e2fa361a10278879。
包含前批未刷入的探索精灵1倍与工作区既有长草园P1优化。本批未访问串口、未刷机，
设备仍为上批029b73d314 Debug，音量0与实机存档未触动；新布局手感待后续实测。
证据见.stick-local/artifacts/sdgoods-route-top-menu-20261101/，固件见
.stick-local/firmware/sdgoods/route-top-menu-debug-20261101/。


2026-11-01 谷仓探索顶部菜单独立按钮：顶部场景名缩为(116,19,84,32)，
菜单独立32px圆角按钮(208,19,32,32)，两者间隔8px；按钮深色底/细描边/18px
三横线，命中区(202,13,44,44)，绘制与命中中心均为(224,35)。不再共用整条胶囊。
菜单不可用时地图名仍回到原128px居中面板。底部释放、1倍精灵、HP/状态、暂停/恢复
逻辑保持，仅改round_touch两文件。
完整谷仓ASan/UBSan交互旅程、44px边角/取消/菜单开合/暂停与存档保护及Debug构建
通过，前后圆屏导出图已检查。182张原帧中165张一致，17张差异仅顶部原胶囊区域，
未修改旧fixture。Debug app 2,830,208 B，SHA256 2e50e1612b2c7c14b0b3ea570037d30d8e459dcac6a0007ed98d989c052f9075。
未访问串口、未刷机，实机音量0/存档未触动；包含此前未刷入的1倍精灵与长草园P1。
证据见.stick-local/artifacts/sdgoods-route-menu-button-20261101/，固件见
.stick-local/firmware/sdgoods/route-menu-button-debug-20261101/。实机观感待后续验收。


2026-11-01 长草园 P1 已刷入谷仓并完成实机对照：同一 Debug/trace 测速固件交替
运行原版和优化版，256 种子×4 入口×3 轮，共 3072 对／1024 张不同地图，完整图层、
路线点和指纹全部一致。生成耗时中位数 141.53→24.37ms（下降82.78%），P95
248.17→49.10ms，平均152.06→27.82ms（下降81.71%）；仅 generate()，不是切图总耗时
或FPS。测试前后内部可用堆263475B，剩余栈低水位14252B；未启动游戏会话，测速后
低128KiB与备份完全一致。正式Debug应用2830192B、SHA256前缀d23792d6835d已只写
原谷仓ec:da:3b:67:10:5c的OTA0 0x20000，写入哈希及启动前低128KiB回读通过。
原已孵化存档恢复并保存成功，debug=1/trace=1，音量0/PA=OFF，首帧540ms、精灵625ms；
25秒启动日志未见异常，串口已关闭，正式版无测速代码。使用此前冻结的P1固件，
未混入其他任务后续精灵1倍／顶部菜单改动。P2/P3未实施。
报告见doc/长草园生成性能优化.md；证据.stick-local/artifacts/park-p1-device-20261101/，
备份为对应backups目录，正式固件.stick-local/firmware/sdgoods/park-p1-20261101/。


2026-11-01 用户授权后，谷仓探索顶部独立菜单按钮 Debug 版已刷入：使用此前验收
固件 2,830,208 B，SHA256 2e50e1612b2c7c14b0b3ea570037d30d8e459dcac6a0007ed98d989c052f9075，
包含探索精灵1倍和长草园P1优化。实读确认原设备ec:da:3b:67:10:5c当前为P1正式
Debug版d23792d6835d，备份低0x320000后只写OTA0 0x20000；写入哈希通过，启动前
低128KiB回读与备份一致。原已孵化存档恢复并保存成功，debug=1/trace=1，音量0／
PA=OFF，首帧601ms、精灵就绪1701ms；25秒采集未见启动异常，串口已关闭。
新按钮和1倍精灵的实机显示／触摸观感待用户确认。刷入证据与备份分别位于
.stick-local/artifacts/sdgoods-route-menu-button-flash-20261101/及对应backups目录，
固件仍位于.stick-local/firmware/sdgoods/route-menu-button-debug-20261101/。


2026-11-01 房间第二只精灵夜间觅食／探索出门修复（代码及构建，未刷机）：
MainScene 区分“步进未到达”和“没有移动”，重新寻路保留受阻起点；解决饥饿睡眠
精灵正常走一步却取消觅食、60秒后重复的问题。出门改为队友原地等候、按实际站位
顺序走出；不可达时尝试换序，停滞重新寻路，第二只在第一只离开后再规划。
移除可见阶段强设等待点／门口坐标，空路线不算到达；极端失败保持坐标、8秒后
用原淡出流程进入探索，跨门也有超时兜底。睡眠／进食／探索消耗与存档规则不变。
新增真实房间 AI ASan/UBSan 回归并加入 CI：饥饿进食／睡眠保持、受阻计时、64组
连续出门、换序、动态阻挡重规划与失败淡出通过。原房间12项／圆屏完整交互、
家族源10项／架构13项及谷仓Debug／StickS3 release构建通过。旧对话框静态检查
因查找过时 drawContact 签名失败，修改前同样复现；未改动该对话框或旧测试。
谷仓 app 2828960 B，SHA256 dd8a94932dc1801ab0a9a4f4e74b02de3f4629c377f6b8057cf36a41e915b90d。
验证见 .stick-local/artifacts/room-ai-fix-20261101/；待刷固件见
.stick-local/firmware/sdgoods/room-ai-fix-debug-20261101/。未访问串口，未刷机。


2026-11-01 谷仓背包／商店三列与顶部Tab已刷入：共用round_touch的68×68卡片、
8px间距、三列220px居中网格；背包仍用中央240px表面，商店物品视口(60,98,240,212)。
商店顶部购买／出售Tab分别(66,56,60,36)/(134,56,60,36)，白色20px选中＋短白线、
未选灰色；取消原侧栏弧形动画和上下滑动切换，列表上下滑动只滚物品，模式各记滚动。
余额固定(206,56,88,36)，金币图标＋右对齐16px金额，万以上最多两位小数向下截断，
点击显示精确值；详情居中，原购买／出售及背包确认规则保持。
追加35个48px Kind到枚举末尾，其中32个道具直接取48px Essentials原图、不缩放，
三种肥皂从原素材适配48px画布。旧1459个Kind编号及48个旧UI帧完全保持，新图标
只用于谷仓背包网格／商店；旧包缺少48px帧时可回退。共享ui包83帧、22997B，
AMOLED包同步生成，校验器接入追加Kind；其他设备原图标选择与尺寸不改。
完整谷仓ASan/UBSan旅程、三列边界／Tab取消与滚动隔离／模式恢复／实际买卖／
跨进程存档及坏档保护通过；资源23项、AMOLED资源4项、架构13项、家族10项及
谷仓Debug／StickS3 release／AMOLED V1 lite构建通过。已检查圆屏预览。173张共同
旧帧中150张一致，23张变化含背包／商店与战斗状态粒子时间相位（差异仅原粒子区，
共享战斗代码未改），未覆盖旧fixture。
Debug app 2,833,504B，SHA256 2b40bc21c2bf7ee2714f0d978df741c6f9e77278a3e06fac2254009cd4704d44；
已备份低0x320000并只更新原设备ec:da:3b:67:10:5c的OTA0 0x20000，写入哈希与
启动前低128KiB回读通过。原已孵化存档恢复／保存成功，debug=1/trace=1、音量0／
PA=OFF；首帧617ms、精灵701ms，25秒无启动异常，串口已关闭。实机新布局观感／
触摸待用户确认；未重置存档、改档或试听。证据见
.stick-local/artifacts/sdgoods-bag-shop-tabs-20261101/，备份为对应backups目录，
固件见.stick-local/firmware/sdgoods/bag-shop-tabs-debug-20261101/。


2026-11-01 谷仓 Debug／触摸诊断页原生圆屏布局（未刷机）：新增 round_touch/DebugUi.cpp，
谷仓构建与 host 源选择替换 compact Debug 绘制；菜单进入／返回 Debug 使用 360×360
原生表面，不再对触摸减去(60,60)。Debug 及触摸页共用主菜单 nativeHeader，顶部
返回键(138,12,40,40)／标题完全对齐。Debug 列表按圆屏弦宽伸展、可滚动；诊断九点
改为近圆边八点（半径约162px）＋中心点，避开顶部返回键；采样、标记与统计均用
原生坐标，重测／返回置于下方安全区域。保留诊断语义，不写硬件校准参数或存档。
新增真实引擎触摸回归：九点按下／移动／松开原生坐标、边缘取消、重测、返回、重入
及 Debug 弹窗通过。普通版完整圆屏 ASan/UBSan 旅程与跨进程存档、Debug／探索
回归、家族源10项／架构13项及谷仓 Debug 构建通过；圆屏导出图已检查。
证据及冻结源码见 .stick-local/artifacts/sdgoods-debug-native-20261101/；待刷固件见
.stick-local/firmware/sdgoods/debug-native-debug-20261101/。未访问串口、未刷机；
实机观感待验收，设备音量／存档未触动。


2026-11-01 用户授权后，谷仓 Debug／触摸诊断全屏版已刷入：冻结应用2,836,464 B，
SHA256 e0ae5f76f1c766c30bca6f58a42640884b537684ef1f0bc66ea7ebdfae20aa94。
实读原设备ec:da:3b:67:10:5c当前为房间AI距离版73241c936cfb，与此前OTA0回读
逐字节一致；备份低0x320000后只写OTA0 0x20000，写入哈希通过，启动前低128KiB
（含原分区／NVS／OTA信息）回读与本次备份完全一致。USB串口仅取得初次ROM启动
片段，后续软件复位／采集未取得应用日志；用户短按电源后明确确认“已亮起，可以
进入主界面”。本批不能宣称启动日志／运行时音量／保存日志已验收；未改设置、存档，
未试听。串口已关闭。证据.stick-local/artifacts/sdgoods-debug-native-flash-20261101/，
备份为对应backups目录，固件.stick-local/firmware/sdgoods/debug-native-debug-20261101/。
全屏Debug／九点边缘布局的实机观感仍待用户确认。


2026-11-01 按用户实机反馈，谷仓触摸诊断八个外圈点从半径约162px内收到约140px，
距圆边约40px，中心点保持(180,180)。上方两点避开 native 返回键；进度下移，
坐标／误差文字与重测／返回按钮上移，保证十字和14px瞄准范围不进入按钮热区。
原生坐标记录保持，不修改触摸驱动／硬件校准或存档。新增14px净空断言，九点
采样／取消／重测／返回和原Debug／探索ASan/UBSan回归、谷仓Debug构建通过，
前后圆屏对照图已检查。证据.stick-local/artifacts/sdgoods-touch-inset-20261101/；
待刷固件.stick-local/firmware/sdgoods/touch-inset-debug-20261101/。本批未访问串口、
未刷机，设备仍为前批e0ae5f76f1c7全屏诊断版；实机点击边缘偏差仍需后续观察。


2026-11-01 谷仓点位内收版实机触摸采样：串口启动ELF SHA前缀9e77ed554与
内收版25eed4a8532a的app descriptor匹配，确认设备已运行该版（本次未执行刷写）。
原已孵化档restored=1，音量0／PA=OFF，首帧538ms。用户确认测试完成，日志实际
收到两轮各8个外圈点，未录到中心点，不视为完整九点校准。平均误差24.16／21.95px，
最大39.62／34.67px，沿圆心向外平均偏17.61／20.55px；按下至松手漂移最大3px。
右侧目标(309,234)读到(338,261)/(338,253)，支持用户反馈“识别偏向边缘”；仍需
检查坐标比例／映射，不能仅靠移动测试点当作校准完成。未修改触摸参数或存档。
原始日志与逐点报告.stick-local/artifacts/sdgoods-touch-measure-20261101/，串口已关闭。


2026-11-01 谷仓触摸完整九点重测：用户看到9/9 COMPLETE后确认完成，日志完整收到
九点；中心(180,180)实测(181,181)，误差1.41px，全部平均23.70px，外围平均26.48px，
最大38.08px（右下与左下）。右下偏(+35,+15)，左下偏(-9,+37)，但左侧两点本轮
向右／朝内，与前两轮方向不完全一致，不能把所有误差视为统一向外放大或平均平移。
建议核对坐标映射、重复采样校准并以独立重测验证，尚未实施／保存校准参数；人工
瞄准误差仍是限制，不能据此断言硬件故障。DOWN到最后有效采样最大位移4px，UP
复用该坐标，不等于独立精度验证。启动ELF前缀9e77ed554、音量0／PA=OFF、原档
restored=1；本轮未刷机、未改触摸参数，串口采集已正常结束并释放。
证据与报告：.stick-local/artifacts/sdgoods-touch-retest-20261101/。


2026-11-02 谷仓触摸校准实现（未刷入）：核对 SDK 原映射 x=rawY、y=359-rawX 与
现有驱动一致，新增 Debug→触摸→校准，保留九点诊断。最后五次稳定采样取中值，
至少四次／100ms／每轴跨度≤10px；八点仿射拟合门限RMS8／单点12px，中心独立验证
≤12px后用户点保存。旧手测数据离线残差RMS16.01／最大22.73px被拒绝，未直接
生成补偿；仍需实机稳定采样。独立 sg_touchcal/affine_v1 32B版本／校验记录，
普通版也加载；支持还原单位映射，取消／锁屏／离页恢复原参数，失败写入保留运行
中映射，非法记录回退单位映射。切换等待松手，修正后越界取消，不压到屏幕边缘。
ASan/UBSan模型／存储／实际驱动与真实引擎校准→保存→独立九点重测／原Debug与
探索回归通过；普通版完整旅程及跨进程存档通过（同期用药改动前），StickS3 release
与架构13项通过。补齐12个新文案字形，原1158个字形字节不变；最终圆屏导出图已检查。
家族静态检查一项因同期 MenuSceneItemUse.cpp 内 PixelRenderer 失败，非触摸改动，
未改旧测试。独立build-touch-calibration-debug成功且668个构建源／资源期间无变化。
Debug app 2,845,856B，SHA256 950fd63694bf2a828ba99d0f8f8f90cbf0c40b4f061379f66aac05d04d043866，
固件 .stick-local/firmware/sdgoods/touch-calibration-debug-20261101/；
验收报告／日志／冻结源码 .stick-local/artifacts/sdgoods-touch-calibration-20261101/。
本批未刷机、未写实机触摸参数，音量0／原档未操作；实际校准与重启恢复待实机验收。


2026-11-02 谷仓背包使用流程对齐（未刷机）：三列完整库存复用ItemInventory目录，
回复/解异常/复活改为详情→使用→队伍选目标→500ms回血＋200ms停留→返回列表，
移除额外是/否确认；取消/失败不扣库存、不耗战斗回合，敌方在反馈结束后再行动。
糖果通过GameEngine::useTeamItem接入共享经验与成长通知，进化石延迟提交/可取消；
圆屏升级全屏清底，进化居中240px表面避开顶部，完成用触摸继续替代A提示。
食物战斗详情显示投喂并直接执行；鳞片点击条目直达首位招式回忆；食物/肥皂/贵重物品
在非战斗背包可查看，战斗不适用项保留并提示不可用。喷雾/蜜限探索非战斗上下文，
未复制微雪入口缺陷；商店卖出金额toast已去除。其他设备背包交互/存档schema不改。
完整圆屏ASan/UBSan旅程、新背包旅程及使用后存档回读、共享物品/队伍像素、库存/背包/
经验/进化/存档/care/调度/家族/架构检查通过；谷仓Debug/StickS3 release/AMOLED V1 lite
构建通过。旧探索UI回归在host:398暂停菜单调度断言失败，隔离的改动前快照同样复现，
未改断言或像素fixture。已检查目标/回血/糖果/进化/投喂导出图；音量0/实机存档未触动。
待刷Debug app 2846192B，SHA256 ff7e5fb417adb0a21e7cec40b6e2f301103a61d5fff4383be8692b95e1a43aba，
位于.stick-local/firmware/sdgoods/bag-flow-debug-20261102/。证据与before快照为
.stick-local/artifacts/sdgoods-bag-flow-20261101/，说明见doc/谷仓背包使用流程对齐.md。


2026-11-02 谷仓背包／商店底部裁剪修复（未刷机）：背包改用360×360原生表面，
列表视口(60,60,240,300)；商店物品视口延伸为(60,98,240,262)，均绘制到y=359。
三列68px卡片／48px图标、顶部导航／Tab／余额与背包详情原物理位置保持。
共享Grid增加默认0的scrollBottomInset，仅round_touch设44px，绘制视口不缩短，
末排可滚入圆屏安全区域，reveal同步避开圆底；其他家族滚动与像素不变。
完整圆屏ASan/UBSan交互／用药成长／探索战斗／跨进程存档与坏档保护通过；新增
y=359绘制、底部点击／拖动／取消、末排四角可见和余额固定检查。家族／架构23项、
共享背包商店语义与原像素回归以及谷仓Debug／StickS3 release／AMOLED V1 lite构建
通过，圆屏导出图已检查。与前批完整旅程191张共同帧178张一致，差异为背包／商店
列表布局、长购买目录fixture及此前已验收的进化居中修复（与最新verified帧一致）。
Debug app 2847872B，SHA256 a665418feb2c5ec348965743f7b32bbe55df3d738707d69583e8ad2ac034544b，
位于.stick-local/firmware/sdgoods/items-full-height-debug-20261102/；证据／before／冻结源码位于
.stick-local/artifacts/sdgoods-items-full-height-20261102/。未访问串口、未刷机，音量0与实机存档未触动。


2026-11-02 谷仓触摸日志采集：01:13:49–01:19:26（Asia/Shanghai）收到三轮完整
TouchTest九点诊断，平均误差23.40／16.01／20.76px，最大38.90／22.56／36.77px，
中心6.08／2.24／3.00px；未录到任何TouchCalibration进入／拟合／保存事件。
启动ELF仍为9e77ed554…，与最新底部全高版f6cbfe443…不同；用户表示有校准按钮，
但本次串口证据不能确认正式校准已运行或保存。原档restored=1，音量0／PA=OFF；
未刷机、未发送串口写入／复位命令，连接时录到USB_UART_CHIP_RESET启动。
串口已关闭。原始日志／逐点JSON／报告位于
.stick-local/artifacts/sdgoods-touch-calibration-capture-20261102/。


2026-11-02 右侧触摸偏移复测：用户要求“采集开启后通知，操作完成后再分析”，
已按该顺序采集01:46:26–01:47:09的一轮完整九点并释放串口。右上第3点目标
(309,126)，DOWN(347,124)／UP(347,121)，偏(+38,-5)；右下第4点目标
(309,234)，DOWN／UP(339,253)，偏(+30,+19)。中心误差1.41px。合计四轮，
第3点x跨度23px、第4点17px，重复人工点击的瞄准／指腹差异未排除。端点变化
3px／0px，但旧日志没有中间轨迹／采样范围／接触时长，不能确认或排除按住时
瞬态漂移，后续需加轨迹或min/max诊断再测，不能凭端点接近声称没有漂移。
设备ELF仍9e77ed554…，原档恢复、音量0／PA=OFF；未刷机或写入校准。
原始日志／逐点与跨轮分析／报告：.stick-local/artifacts/sdgoods-touch-right-drift-20261102/。


2026-11-02 谷仓右侧渐进触摸补偿（已构建，未刷机）：新增平台层
RightTouchCompensation，按四轮中位数右上(344.5,113)→(309,126)、
右下(335.5,248)→(309,234)平滑修正；x180→320渐入，y56→113渐入／
y248→309渐出，左半／顶部导航／下缘保持。DOWN/MOVE共用映射，UP复用末次
有效坐标，原取消／越界／等待松手语义保持。CMake开关
STICKMON_SDGOODS_RIGHT_TOUCH_COMPENSATION默认ON；有任何有效保存仿射记录
（包括明确保存的单位映射）时禁用实验补偿，校准采样bypass绕过两种补偿，
成功保存仅用仿射，失败保持旧模式；不改NVS记录格式，不写实机参数。
ASan/UBSan校准／存储／实际驱动和新补偿回归、Debug真实引擎校准与探索旅程、
驱动音频／电池／通讯及家族／架构23项通过。全屏检查无越界／折返，最小局部
Jacobian0.539；现有八个右侧样本浮点回放均值31.84→7.66px、最大20.45px，
属于开发样本回放，不是独立实机收益，第二轮右上仍过度补偿。未要求用户再采点。
谷仓Debug构建通过并核对编译开关均为1，app2848528B，SHA256
05e7c308a2c7d113ecdd85f3dbc758a7ad49b7690130feb926034457dfb087cd，
ELF前缀92c54453c；固件.stick-local/firmware/sdgoods/right-touch-compensation-debug-20261102/。
尚未访问串口或刷机，设备仍实测旧ELF9e77ed554…；音量0／原档未操作。
证据／before／冻结源码.stick-local/artifacts/sdgoods-touch-right-compensation-20261102/，
说明见doc/谷仓右侧触摸渐进补偿.md。用户下一步只需刷入后日常点击右侧验证，
不强制九点重测；若再采日志必须先通知“已开启”，等用户说完成再读取分析。


2026-11-02 用户实机反馈右侧渐进补偿“更准了，可以先保持这样”。当前参数
按此验收意见暂时保留，后续不主动调参或要求重新采点。对应交付固件为
right-touch-compensation-debug-20261102，app SHA256前缀05e7c308a2c7。
本次仅记录用户反馈，未访问设备、未重新构建或刷机；无新增串口版本核验。

2026-11-02 用户随后反馈“还是不行，3，4，点位还是很偏”，覆盖上一条暂时接受
结论：右侧渐进补偿尚未通过实机验收。当前未获得补偿后方向／坐标和运行时开关
证据，不能假定需要增大补偿或已确定根因；本次尚未改补偿参数／构建／刷机。

2026-11-02 微雪探索地图改为1倍（代码／构建，未刷机）：独立
`EXPLORE_ROUTE_SCALE=1`，地图格26×26，玩家／跟随／头目1倍，野外生态0.8倍；
全局房间资源仍为2倍，选择／战斗／HUD／菜单不改。416×312地图在368×344视口
纵向居中（cameraY=-16、上下16px区域底色），横向镜头0～48；拾取球、阴影、
浮空／动作偏移、草丛反馈／遮挡与刷新矩形同步。夜色缓存不再依赖2倍重复行，
改同一分配中的原色／夜色双平面：白天259584B、夜间519168B，旧版1038336B；
负镜头／边缘留白均安全裁剪，留边内离场最后一帧仍清理，完全离场后不重绘。
ASan/UBSan原生界面81项通过76项，5项失败与改动前完全一致（出门、桥面资源、
进食离碗、两项配对旧基线）；旧路线fixture同样预先失配，但本次前后实际路线／
状态／RNG逐字节一致，9项会话检查通过。夜色128组无缓存对照、全部生态／群组／
草丛／霜窟／遮挡／拾取／镜头专项及架构13／源5／家族10／页面18、调度、画布、
传屏流水线检查通过。51张共同帧48张一致，仅3张route图变化；六区域昼夜已看。
V1/V2 lite构建通过，未访问串口或刷机，实机观感／性能待验收。
见doc/微雪探索地图1倍适配.md与.stick-local/artifacts/amoled-explore-1x-20261102/validation.json。


2026-11-02 市集浮动摇杆与聊天图标（未刷机）：MarketWalkUi 将地图 DOWN 原点作为
摇杆中心，按住期间固定，松手／取消隐藏；保留8px死区／4px滞回／26px位移上限。
菜单／聊天优先获得整次手势，跨按钮不误触，暂停／离场禁止新行走。透明底像素
气泡由内置image_gen生成，原图／提示词位于origin_asset/icon/market/，28px准备工具
为tools/prepare_market_chat_icon.py；末尾追加MARKET_CHAT，不改旧Kind编号。共享／
AMOLED UI包84帧，原各83帧逐像素保持；更新AMOLED包校验器。真实Stick／谷仓／
AMOLED场景ASan/UBSan（含边缘裁剪／取消／按下归属／暂停／返程／存档不变）、
资源24／原生资源4／架构13／家族10／调度2项通过；谷仓release／StickS3 release／
AMOLED V1 lite构建通过，圆屏导出图已检查。应用3037488B，SHA256
9378323a564bd8029c885a542c416946ceefd02a5c56a7b746f99c26f2c255ab。
证据.stick-local/artifacts/market-floating-controls-20261102/，固件
.stick-local/firmware/sdgoods/market-floating-controls-20261102/；未访问设备／串口，
实机显示和触摸待验收。聊天仍为点击占位，未增加对话业务；AMOLED原操作保持。


2026-11-02 市集紧凑对话按钮／走出返回（未刷机）：聊天背景由76×36改为
28px图标＋四周4px的36×36，独立44×44点击区。移除MarketWalkUi右上菜单、
暂停／返回弹层及热区，旧菜单位置可作为摇杆起点。手动漫步朝正式entry迈出时
调用共享leave流程，保持玩家与同伴走出动画并返回地区列表；同伴占门可先出，
进图不会自动返回，仅正式出口允许触达边界。非手动AMOLED模式和实体键不改。
ASan/UBSan四向出口×单人／同伴、占门／重进／其他边界，10种子非手动30分钟，
真实Stick／谷仓／AMOLED场景（无菜单像素／点击归属／摇杆离场／音乐／存档不变）、
架构13／家族10／调度2项通过。谷仓release／StickS3 release／AMOLED V1与V2 lite
构建通过，已检查圆屏导出图。谷仓app 3036624B，SHA256
56da74a88652ed171e239ca05a9755c0b02c4ae9821af3157939842268fe096b。
证据.stick-local/artifacts/market-walkout-20261102/，固件
.stick-local/firmware/sdgoods/market-walkout-20261102/。未访问设备／串口或刷机，
实机显示与操作待验收；图标原图／资源编号／存档schema未改。


2026-11-03 谷仓探索选择页全宽轮播（未刷机）：RoundExplore::Selection 的预览
窗口由 (60,106,240,140) 扩为 (0,106,360,140)，保持精灵原尺寸、y=172 与原间距。
轮播改为整排共同平移，向左右循环补绘到窗口外，不再仅三只或从右侧远端追加入场；
单精灵仍独立居中。导航／出发／手势与游戏池规则不改。真实引擎完整 ASan/UBSan
旅程、跨进程恢复／坏档保护，新增双侧覆盖／移动六时刻／垂直裁切／单精灵稳定／
不同尺寸剪影循环边界／缺帧检查，以及架构13／家族源10项通过，圆屏动画导出图已看。
谷仓Debug／trace构建通过，app3059088B，SHA256
59e6546c925bfc68466f47c9874b5ffdc4a0c591be12959b6b4c12977826adb8。
固件 .stick-local/firmware/sdgoods/explore-full-width-debug-20261103/；证据／before／
冻结修改 .stick-local/artifacts/sdgoods-explore-full-width-20261103/。未访问串口或设备，
用户自行刷入，实机显示／性能待验收；音量和实机存档未触动。


2026-11-03 谷仓探索选择页轮播性能优化（未刷机）：当前池就绪后原子提交预览帧，
市场纳入逐只预载；移动及前置保护窗口不启动同步解码，长加载延长停留，加载完成
额外唤醒以恢复轮播。单成员不周期刷新，省电停留、唤醒续播。Selection 缓存
360×140 静态带（100800B），离场释放、分配失败完整绘制；普通移动仅重画/传输此带，
比整屏少61.11%数据。FrameDamage 默认整屏，提示/按钮/地图/画布变化、全局失效、
切页和淡入淡出完整刷新；IDisplayDevice::presentRegion 默认整屏回退，谷仓贯通DMA。
移动阶段33ms视觉节拍通过独立场景更新入口运行，核心照护/音频/存档保持原节拍。
新增 tools/test_explore_carousel.py：ASan/UBSan逐像素全图/局部对照、传屏区域、内存失败、
加载结束/滑动禁止预载、静止/单成员/省电/淡出；主机500ms为15帧（14局部）、8核心tick。
完整圆屏旅程/跨进程恢复、调度2/架构13/家族10/DMA测试及谷仓Debug、StickS3、
AMOLED V1/V2 lite构建通过。界面大小/顺序/剪影、手势/出发/解锁/存档保持。
固件 .stick-local/firmware/sdgoods/explore-carousel-opt-debug-20261103/，app3061296B，
SHA256 58889e511553d431332518191e38ed6fdcf7c8c12f77104315cb805dc5b4772b。
证据 .stick-local/artifacts/explore-carousel-opt-20261103/verification.md；未连接设备/刷机，
真机FPS、PSRAM峰值和冷切区加载耗时待测；现有RenderStats传输KB仍是整屏等效估算，
局部传输应按实际矩形计数。未做第三批全池正面图展开缓存，设备音量/存档未触动。


2026-11-03 微雪探索地图改为1.5倍（未刷机）：接替上一版1倍方案，图块39px、
世界624×468，地图显示区仍368×344，正常镜头不再出现上下补边。玩家／跟随／
头目1.5倍、生态1.2倍；原生解码图块与显式启用的RLE最近邻区间采样保持原色和
透明轮廓，其他页面保持既有绘制规则。精灵生成模板同步接口；镜头／草丛／遮挡／
阴影／拾取／昼夜／脏区共用取整后的像素原点，世界地面偏移仍为9px。底部104px
操作栏、房间／选择／战斗和玩法／存档不变。日缓存584064B、夜原色＋着色平面
1168128B，缓存失败回退同倍率直接绘制。ASan/UBSan原生82项通过77项，剩余
5项与修改前一致；新增分数倍率透明／裁剪／镜像／字节序以及Canvas越界检查通过，
六区昼夜／生态／群组／草丛／飞行／遮挡／拾取及局部全帧对照通过。9项会话检查
通过，实际路线／状态／RNG与1倍可执行文件逐字节一致（旧路线fixture预先失配）。
51张共同页面48张一致，仅3张route变化；架构13／家族10／源5／页面18／调度2、
画布和探索传屏流水线通过。V1/V2 lite Debug与StickS3 release构建通过，Debug
固件和匹配组件在.stick-local/firmware/amoled-v{1,2}/explore-1p5x-debug-20261103/。
预览已看；未访问串口／刷机，实机观感和性能待验收。说明见doc/微雪探索地图1.5倍适配.md，
证据见.stick-local/artifacts/amoled-explore-1p5x-20261103/validation.json。

2026-11-03 野外铁甲蛹固定位置（未刷机）：共享 ExploreEcology 为 species 11
增加独立原地 REST/WATCH 行为，禁止散步、睡醒靠近及身体浮动／行走帧；
保留原地转向观察和隐藏草丛反馈，不改队伍移动或陆生草丛接触规则。
地图继续保持已确认的1.5倍。新增72组90秒定点回归，覆盖草地／遮挡、
玩家四向靠近、隐藏揭示、暂停和离屏，绿毛虫仍可移动；原实现会在新回归失败。
完整共享生态ASan/UBSan、微雪真实生态局部重绘／暂停专项、架构13项／家族源10项
以及AMOLED V1/V2 lite Debug、StickS3 release构建通过。未访问串口／刷机。
证据见.stick-local/artifacts/metapod-stationary-20261103/，匹配Debug组件保存于
.stick-local/firmware/amoled-v{1,2}/metapod-stationary-debug-20261103/。


2026-11-03 首次刷入仓库旧市集（后由用户指出与 Chrome 草稿不符，已被下节替换）：正式工程旧路灯占地在当前编辑器规则下迁移，
先备份，再用现有recognizeRoads/storeRoadRoute重存路网并由正式导出器安装；地图
图层/坐标/活动范围不改，生成头仅来源指纹更新（4320bf78…）。市集9项、三家族真实
场景ASan/UBSan、完整谷仓菜单/探索/跨进程存档、架构13/家族源10项通过，圆屏预览
已看；Debug/trace app3089152B，SHA256 56016379aa4e948df01abfb43c4a4014a7b95b897f586a8d4b438621ee827d33。
同一MAC ec:da:3b:67:10:5c、32MiB Flash，核对原分区/OTA0 sequence3 VALID；
低0x320000及上一版app59e6546c…已备份，仅写0x20000，哈希校验通过，重启前
低128KiB逐字节不变。启动ELF809d022de…，原档restored=1 hatched=1、保存成功，
species1 stock0 bowl1 bites1 satiety57；首帧545ms/精灵630ms，音量0/PA OFF，原触摸
校准加载。30秒启动无panic/传屏错/循环重启，串口释放。监视器虽指定V2 ELF，实读
内容仍为谷仓原生版本，未因此改分区。本轮未人工操作真机市集；显示/触摸/FPS待验收，
右侧触摸历史反馈未解决。证据.stick-local/artifacts/sdgoods-market-flash-20261103/；
固件.stick-local/firmware/sdgoods/market-debug-20261103/，备份同名任务目录（非整片Flash）。


2026-11-03 市集来源纠正：必须先核对用户当前 Chrome 工程，不能只编译仓库旧源并称为最新地图。已下载浏览器市集并安装18×17源，常驻352/094/115/113；常驻无172，但页面四组访客含025+172，保持原配置。尺寸/原图层/物件/入口/NPC/活动区与浏览器逐项相同；新注册素材的落地占地补默认值，画面像素及道路节点/边不变。手工湖岸不再误按自动湖泊校验。
MarketMapSize.h与MarketMapData.h须一同生成；Market独立图层/uint16出生及巡逻索引/镜头/遮挡，普通探索仍16×12，AuthoredNpc不再定义污染后续include的默认尺寸宏。设备导出限1295导航点，控制16KiB栈；大图活动预览保留。新增Outside1340/1381/1410/1411/1419/1427使用14000起的独立组，Kind末尾追加6项，原1357帧内容/编号/调色板不变。地图包1363帧、载荷372242B。
市集11、编辑器34、大图10、资源24、三家族真实场景/完整谷仓主机流程ASan/UBSan、架构13/家族源10均通过，StickS3 release/AMOLED V1/V2 lite/谷仓Debug构建通过；已看正式渲染器新版全图及圆屏。
已备份同MAC低0x320000与上版app56016379…，只写OTA0 0x20000，app3089216B SHA256 064df933a9d2cb6181295a3deb17207f6c19edca7f01d0cf005c4d702e95b257，写入校验及启动前低128KiB不变通过。30秒启动ELF2cfc69f16…，restored=1 hatched=1、save ok、音量0/PA OFF、原校准加载；首帧579ms/精灵663ms，无panic/传屏错/循环重启，串口释放。真机市集显示/手指操作/FPS尚待人工验收。
证据.stick-local/artifacts/sdgoods-market-browser-sync-20261103/；固件.stick-local/firmware/sdgoods/market-browser-sync-debug-20261103/；备份同名backups目录（非整片Flash）。

2026-11-03 微雪市集自由行走（未刷机）：V1/V2 共用 MarketWalkUi，启用
Market::Session::enableManual，2× 地图与精灵铺满368×448；移除旧底栏及点击寻路。
按下地图定位摇杆，松手／越界／取消隐藏并清除方向；锁屏当下取消，解锁不会续走。
靠近NPC时底部(166,399)出现36×36聊天图标，44×44命中区，远离隐藏；
聊天只消费点击，保持谷仓现有占位行为。正式入口走出后返回地区列表。
真实微雪App专项ASan/UBSan覆盖满屏像素、实际移动／停止、四向控制、取消、锁屏、
按钮归属、实际NPC邻近检测与走出返回；共享市集11项、Stick／微雪／谷仓真实场景、
架构13／家族源10／微雪源码5／停绘2项通过，V1/V2 lite Debug构建通过。
四张微雪导出预览已检查，未访问设备／串口，实机触摸及帧率待验收。
证据.stick-local/artifacts/amoled-market-joystick-20261103/validation.json；匹配
Debug组件.stick-local/firmware/amoled-v{1,2}/market-joystick-debug-20261103/。

2026-11-03 微雪市集出口／倍率修正（未刷机）：用户反馈入口两侧不能离场，
实测旧manualStep只认entry中心半格，新24组四向×单只／同伴×三入口半格回归
在旧实现失败。共享Session现以entry同边连续道路定义入口宽度，向外可触发
共享离场；已站入口也可离开，进场等待不自动退出，同边断开的道路不算出口。
微雪地图及精灵统一1.5×最近邻，39px图块、18×17地图702×663，镜头范围
X0～334／Y0～215。Viewport控制倍率独立设2，聊天背景72×72、图标56×56、
点击88×88、位置(148,363)，底部间距13px；谷仓仍1×／36px按钮。
真实微雪App ASan/UBSan已用摇杆分别从正式入口左／中／右离场完成并返回，
含放大按钮归属、锁屏取消、远近隐藏和满屏无底栏像素对照；共享市集13项、
Stick／微雪／谷仓真实场景、架构13／家族源10／微雪源码5／停绘2项通过。
V1/V2 lite Debug、StickS3 release、谷仓Debug构建通过，预览已检查；
未访问串口／刷机。证据.stick-local/artifacts/amoled-market-exit-scale-20261103/；
匹配微雪Debug组件.stick-local/firmware/amoled-v{1,2}/market-exit-1p5x-debug-20261103/。


2026-11-03 市集观望／混合并发／交流演出（代码完成，未刷入）：18×17 图层、物件、
入口、道路及 NPC 出生／活动范围保持；四摊主 reception 方向独立于移动，空闲观望，
优先回应本摊顾客及无遮挡的附近玩家。源工程追加 market 设置、NPC reception／
lookDirections／customerPoints；原四组结伴加单只225／184／212，3组／6游客上限，
60%单只，首组2～4秒、补充4～8秒、行程45～75秒。摊前点预约覆盖前往与停留，
一摊一组，结伴使用两个相邻点；临时观望避开窄道／别组预约，阻塞按原边让行。
现有 MARKET_CHAT 复用到头顶，轮流说话、偶尔300ms／4px轻跳；影子、导航与深度
排序留地面，全场最多两摊交流，暂停／锁屏／离场取消同步。运行时和wire接受v1/v2，
新导出v2；旧工程缺省单组。编辑器字段、活动预览与正式生产渲染器复用Session。
市集13项（含5种子各15分钟多组、六只峰值、空中暂停与无演出路径一致），三家族
真实场景、编辑器34／NPC16／尺寸10、架构13／家族10通过；四目标构建通过。
谷仓Debug app3098720B，SHA256 c9f78535cb23e29f4523d6b1bd3a0148adf96debef5ffa13ae1670d99a168d51。
90秒全图／圆屏动画、性能与限制见.stick-local/artifacts/market-liveliness-20261103/verification.md；
匹配固件在.stick-local/firmware/sdgoods/market-liveliness-debug-20261103/。未访问串口或
设备、未刷入；真机观感、任务栈水位、帧耗时与传屏待实测，不以主机耗时冒充真机FPS。

2026-11-03 用户后续授权后，市集观望／混合游客／交流演出版已刷入谷仓：匹配上述
3098720B Debug app，SHA256 c9f78535cb23e29f4523d6b1bd3a0148adf96debef5ffa13ae1670d99a168d51。
同MAC ec:da:3b:67:10:5c／32MiB，实读发现当前分区表已更新为OTA0 0x20000／3MiB、
OTA1 0x320000／3MiB、NVS 0x9000／24KiB，OTA0 sequence1 VALID；旧app为3059088B／
59e6546c…、ELF b4ae4ea09…，不能继续套用旧OTA1 0x790000与sequence3记录。
首次备份串口中断，未写Flash；检查无占用后重新备份低0x320000成功，旧app镜像SHA验证
通过。仅写0x20000，写入哈希通过；启动前低128KiB逐字节不变，当前bootloader／分区表／
NVS／OTA记录／OTA1／resources／coredump／brainfs保留。30秒启动ELF4d363b54e…匹配，
restored=1 hatched=1 species=1 stock=6 bowl=1 bites=2 satiety=74、save ok、原校准加载、
音量0／PA OFF；首帧524ms／精灵607ms，无panic／传屏错／循环重启，串口已释放。
本轮未人工进入市集操作，现场画面／触摸／帧耗时与任务栈水位仍待验收。证据
.stick-local/artifacts/sdgoods-market-liveliness-flash-20261103/，备份同名backups目录（非整片Flash）；
固件仍为.stick-local/firmware/sdgoods/market-liveliness-debug-20261103/，manifest已记录刷入。

2026-11-04 谷仓电压趋势充电估算（代码／构建完成，用户明确要求先不刷入）：
用户接受用GPIO8电压持续上涨近似判断；已知SDK只有ADC1 CH7电池电压、三倍分压，
未找到CHRG／STAT／VBUS信号。新增平台层ChargingTrend，不使用USB串口连接状态。
电源键处理后独立每2秒采八次ADC、至少六次有效并去高低均值；电量仍10秒80/20平滑。
每五轮形成一组均值，连续四组逐组升至少4mV且首尾升至少24mV才显示黄色闪电，
初始需约40秒。连续两组下降至少4mV、距峰下降24mV或120秒无合格上涨时退出；
ADC失败、间隔超过6秒、亮度变化及锁屏／恢复清空趋势。房间和孵化页接入共享闪电，
不持久化状态、不控制充电芯片；微雪AXP2101硬件判断保持原样。
纯趋势／真实Battery驱动ASan/UBSan、未知／0／10／73／100电量的真实房间／孵化
闪电像素范围、完整谷仓交互／跨进程存档恢复、架构13项／家族10项与Debug／trace
构建通过，圆屏导出图已检查。app3107664B，SHA256
6ff9ad1c98759c52ee492ea6247e248a17badcf4b841079c0625f17702a95375。
证据.stick-local/artifacts/sdgoods-charge-trend-20261104/，冻结固件在
.stick-local/firmware/sdgoods/charging-trend-debug-20261104/。本轮未访问设备／串口，
未刷入，音量和存档不变。阈值尚未实机验证；满电平台／缓慢充电可能漏判，拔线后
可能延迟消失，其他负载降低也可能误判。不能将闪电视为硬件确认的充电状态。

2026-11-04 市集红花摆动（代码／资源／构建完成，未刷入）：Outside.png:1340／14000
以原像素增加8帧轻摆循环，上部左右1～2px、原图22～31行根部固定，第0帧与原图一致，
不丢不透明像素、不越出图格。编辑器和设备复用同一动画；原取样物件与地图摆放不改。
保留原1533个Kind编号和六帧EXTRA，末尾追加EXTRA_6～13；旧1363帧数据／调色板／
尺寸均不变，maps.smonfx现1371帧／373126B原载荷，增加884B。正式源只更新来源指纹，
地图图层／物件／道路／入口／NPC活动逐项保持；生成头通过正式导出器更新。资源25、
编辑器35、市集13项与diff检查通过；正式生产渲染的左下三处红花均有三姿态且根部固定，
预览已检查。StickS3 release／AMOLED V1/V2 lite Debug／谷仓Debug构建通过。
谷仓app3109520B（3MiB余36208B），SHA256
57333a53f0f74870ff2450db032399f206762dd0bd10613286d47f8395e79449；嵌入地图包匹配。
证据.stick-local/artifacts/market-red-flower-20261104/，匹配固件
.stick-local/firmware/sdgoods/red-flower-debug-20261104/。未访问串口或刷入，真机待验收；
谷仓资源随应用安装，Stick／AMOLED需同步地图资源包。

2026-11-04 市集八向移动／回房渐变（代码与构建完成，未刷机）：共享 MarketDirection
保留原四向编号并追加四斜向；浮动摇杆八扇区、8px死区与5度滞回，玩家／同伴按
200ms直行、283ms斜行的半格毫秒事件移动，NPC／游客仍按100ms原四邻接模拟。
斜行双侧通路检查并预占整个半格方块，禁止切角／交叉穿身，遇阻沿开放轴滑动；
同伴八向跟随／反向让路，正式四边宽出口支持斜向含朝外分量离场。
生成器／运行时68种精灵方向映射扩为八向，复用既有帧／镜像，不改Kind编号或包载荷；
房间／探索既有方向转换补齐新增枚举。离场保留市集与资源至全黑，复用正常探索
全屏渐隐→房间渐显及主从入门流程，不显示探索列表，不结算市集的探索费用／奖励。
市集13项及资源／边界／家族源／调度48项通过；新增ASan/UBSan八向等速、双侧切角、
扫掠占用、反向跟随、四边双斜向出口和7种子各10分钟随机行走通过。Stick／谷仓圆屏／
微雪真实场景验证68种八向帧、摇杆滞回／归属／锁屏、完整黑帧与回房入门；已检查
方向图和三界面回房画面，StickS3 release、AMOLED V1/V2 lite Debug、谷仓Debug构建通过。
证据.stick-local/artifacts/market-eight-directions-20261104/；匹配固件存于
.stick-local/firmware/<目标>/market-eight-directions[-debug]-20261104/，manifest记录buildOnly／
flashed=false。未访问串口或设备；实际摇杆手感、帧率与显示仍待实机验证。
