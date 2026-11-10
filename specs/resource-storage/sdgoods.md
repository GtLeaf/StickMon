# 谷仓独立资源存储（物理 32 MiB／布局 16 MiB）

更新：2026-11-09。目录修正版支持默认 16 MiB 布局／9 MiB 资源与可选 32 MiB 布局／24 MiB 资源；本轮按首版完整安装交付，运行中资源更新关闭。仅迁移谷仓，
微雪 V1/V2、StickS3 继续使用原分区与静态内容提供器。

## 分区与资源容量

实际构建源为 `firmware/sdgoods/partitions.csv`，不再复制 SDK launcher 表。
两个 OTA 应用槽共享单个 `resources` 分区，布局 ID 为 `sdgoods-16m-single-v1`。
完整布局使用 `firmware/sdgoods/partitions-32m.csv`，由 CMake `STICKMON_SDGOODS_FULL_STORAGE=ON` 选择。
方案目录的候选 CSV 仅保留为快照，不是构建入口。

| 区域／分区 | 类型 | 起始地址 | 大小 | 结束地址 |
| --- | --- | --- | --- | --- |
| 启动层及分区表 | 启动预留 | 0x000000 | 36 KiB | 0x009000 |
| `nvs` | NVS | 0x009000 | 24 KiB | 0x00F000 |
| `otadata` | OTA 元数据 | 0x00F000 | 8 KiB | 0x011000 |
| `phy_init` | PHY | 0x011000 | 4 KiB | 0x012000 |
| 应用对齐预留 | 预留 | 0x012000 | 56 KiB | 0x020000 |
| `ota_0` | 应用 A | 0x020000 | 3 MiB | 0x320000 |
| `ota_1` | 应用 B | 0x320000 | 3 MiB | 0x620000 |
| `resources` | LittleFS | 0x620000 | **9 MiB** | 0xF20000 |
| `userdata` | LittleFS | 0xF20000 | 768 KiB | 0xFE0000 |
| `resmeta` | NVS | 0xFE0000 | 64 KiB | 0xFF0000 |
| `coredump` | 崩溃转储 | 0xFF0000 | 64 KiB | 0x1000000 |

整张表结束于 0x1000000，共 16 MiB（16,777,216 B）；物理容量仍为 32 MiB，余下 16 MiB 暂不分区，默认与微雪的 16 MiB 规划对齐。共享资源容量为 9 MiB，两个 OTA 应用槽不再复制资源。
`userdata` 本批只预留，不挂载、不格式化；存档、设置、触摸校准继续留在原 NVS。
64 KiB coredump 是预留容量，尚未验证完整转储能力。

| 当前构建／主机测量项目 | 大小 |
| --- | ---: |
| Release 应用 | 1,316,800 B（3 MiB 槽约 41.9%） |
| 215 个索引文件及清单的逻辑总长度 | 3,743,888 B |
| 其中 49 张内容表 | 346,165 B |
| 主机挂载实际 LittleFS 镜像占用 | 4,239,360 / 9,437,184 B（约 44.9%） |
| 上传用合并镜像 | 16,777,216 B（16 MiB） |

应用大小以本批构建为快照，发布时以包内 `release.json` 为准。
9 MiB `resources.bin` 是整个文件系统镜像长度，不是已用文件大小；镜像经过重新生成，未截断旧镜像。
图片、音乐、叫声使用原压缩／流式解码，不额外保留 SMRA bulk archive。

## 内容与代码边界

资源包保持 `/packs/dev` 逻辑路径。`active.json` 在共享资源镜像内选择根目录；
运行应用槽只用于选择对应的应用绑定；资源始终从共享 `resources` 挂载，不按资源版本号跨槽挑选，不拼接另一槽文件。

```text
/bank-manifest.json          布局、目标、ABI、文件数和索引 hash
/files.idx                  有界 SMFI 文件清单：路径、长度、SHA-256
/active.json
/packs/dev/manifest.json
/packs/dev/sprites/          精灵图像与动画
/packs/dev/cries/            叫声
/packs/dev/audio/            BGM 与音效
/packs/dev/fonts/            共享与圆屏字库
/packs/dev/game/             UI、战斗特效、图块
/packs/dev/rooms/            房间与家具外观
/packs/dev/text/             市集对白
/packs/dev/catalog/          49 张 SMCD 内容表，含地图／NPC 数据
```

`tools/build_sdgoods_resources.py` 裁选谷仓资源并生成索引；
`tools/generate_sdgoods_catalog.py` 从维护中的 C++ 默认内容导出显式小端二进制，
避免手工维护第二套数值。生成器同步更新过的地图头支持静态与外置两种提供器。
本批地图表位于 `catalog/`，没有额外复制到 `maps/`。

外置范围：物种、招式效果与学习表、特性、行走／照料动画、房间显示参数、战斗尺寸、
移动参数、地区参数、生态／群组、探索图块／布局／路径、市集地图／道路／NPC、
遭遇／首领／掉落、物品与商品列表。房间和家具外观、对白已有资源格式，沿用现有读取器。

固件保留战斗、成长／进化、地图生成、寻路、生态、交易、UI、解码／缓存、硬件和更新代码。
既有规则里的少量分支参数仍随算法维护；本批没有新增未实现的家具打造或任意规则脚本。
新增规则、效果、二进制格式或超出上限的地图仍须更新应用。
物种／招式／Kind／道具等原有 ID 不重排，分区调整不改变保存 schema；当前构建 SaveCodec schema 为 6。

谷仓定义 `STICKMON_EXTERNAL_CONTENT=1`；共享 `ContentData` / `ContentStore`
不依赖 ESP-IDF，其他目标默认 0。表按需解码并常驻，启动时验证所有表与关联；
不在每帧重读。原始表与解码对象合计上限 2 MiB、最多 96 张表，单表不超过 1 MiB，
物种数量受现有接口限制为 255，普通探索仍为 16×12，市集仍受编译尺寸／导航上限约束。
已验证同规则追加物种与替换现有地图图块可只换资源，不重编应用。

## 启动、写保护与恢复

先初始化基本显示、触摸、USB，再加载资源并启动游戏：

1. 检查物理 32 MiB 和分区几何，以运行中 OTA subtype 选择应用绑定。
2. 从独立 `resmeta` 读取该应用槽双份 CRC 绑定，取该槽最高有效序号；
   核对运行应用 hash，共享资源只读挂载，校验 manifest、索引及所有文件 hash。
3. 绑定内容源，验证类型、长度、范围、引用及地图结构，再恢复原存档。
4. 检查关键字库、房间、当前伙伴图像可用；试运行期间禁止保存、删除或清空 blob。
5. 所有检查通过才将 OTA 标记 VALID，解除存档写保护并按原保存流程运行。

当前每次启动都会校验所有索引文件；不是仅首次启动全量扫描。
整个文件系统镜像 hash 在安装时验证，正常启动以文件索引验证实际内容。
音频流式、精灵与图块按场景缓存，不把 9 MiB 整包加载到 PSRAM。

试运行失败时通过 bootloader rollback 回到旧应用；资源区是共享的，不能在运行中擦除或切换资源镜像。
没有可用资源时进入最小恢复页，用独立 130 B ASCII 字形提示 USB 重装。
资源挂载失败从不自动格式化，坏资源不重置存档。
恢复页不能运行普通更新事务，修复使用 ROM USB 安装工具。

## 发布与 USB 安装

工具 `tools/sdgoods_release.py` 提供 `package / inspect / install / factory`；旧 `update` 子命令会明确拒绝，
命令示例见 `firmware/sdgoods/README.md`。构建脚本仅构建。
禁止使用 `idf.py flash`、SDK 刷机脚本或生成的 `flash_args` 安装：它们不会同步校验共享资源元数据。

发布包包含 app、9 MiB LittleFS、bootloader、分区表、初始 OTA 选择／双槽绑定及 envelope。
manifest 绑定目标／布局、content ABI 1、保存 schema（当前为 6）、应用／资源／索引 hash 和长度；
同时包含 bootloader／表摘要。`package` 解包实际生成的 LittleFS，与 stage 逐文件核对，
检查固件启用 rollback、32 MiB 和外置内容，并核对构建内固定的发布公钥。

Release 必须使用 ECDSA P-256 / SHA-256 签名；Debug 可显式使用未签名 development 包。
默认公钥复用 `firmware/amoled_1_8_v2/ota/public-key.pem`，可用 CMake 的
`STICKMON_SDGOODS_UPDATE_PUBLIC_KEY` 和工具 `--public-key` 一起替换。
私钥保持本地，不进入源码或发布包。发布签名不等同启用芯片 Secure Boot，
ROM 物理刷机权限仍由设备自身控制。

首版不开放运行中 USB 资源更新。共享资源区挂载期间拒绝旧的配对更新命令，
避免擦除当前应用正在读取的 LittleFS。资源或应用变更统一通过完整 USB ROM 安装器，
校验签名、分区表、镜像摘要和写后摘要后才复位。没有 Wi-Fi 自动更新、差分包或独立活动资源覆盖。
保持保存 schema 不变；将来有不可逆存档迁移时必须另做回退兼容设计。

## 首次迁移与保护范围

`install` 默认只显示定点计划，真正写入需 `--execute --port --mac`。
设备 MAC 和 32 MiB 容量必须匹配；空白 Flash 可直接首装，已有完整分区表只能是当前新表或工具列出的精确旧表；
其他历史 launcher／大 OTA 表会被拒绝，须另审迁移。初始写入两个应用槽和一个共享资源镜像。

写入 bootloader、分区表、otadata、两个 app、共享 resources 和 resmeta；
不写 `nvs` 0x9000～0xF000、PHY、新旧预留 `userdata` 范围，不整片擦除、不自动备份。
新 userdata 位于 0xF20000～0xFE0000，只有 768 KiB；旧高地址 userdata 不迁移。
安装工具接受精确旧 15 MiB、双 12 MiB 表和单 24 MiB 表，但发布包必须使用精确匹配的 16 MiB 或单资源 32 MiB 表；
写入和擦除扇区范围不得超过所选表的 16 MiB／32 MiB 边界。`factory` 按表导出对应长度的连续镜像，
用于新设备首装，地址间隙填充 0xFF，整包写入会覆盖 NVS；已有设备使用定点 `install`。
安装加速覆盖上述七区计划：每区先比较最多4KiB前缀，不同则写入；前缀相同
必须再比较该区域完整设备MD5，匹配才跳过，不信任本地安装缓存或配对记录代替实读。
写入区由esptool原生写后MD5校验，并确认每个地址／长度／摘要均实际检查；不再重复
整轮verify_flash。发布包仍按原签名／SHA256验证，启动绑定与保存结构不变。
全部跳过也需七区设备摘要通过；任何串口、摘要或漏校验错误停止，不自动跳过。
原 `sg_stickmon` 和 `sg_touchcal` 保持；本轮不改变存档后端或触摸校正。
首次改分区过程不是普通更新的原子事务，中断后须通过 USB 重新安装。

## 验证记录与边界

2026-11-08 当前 15 MiB 布局：Release 构建和签名发布包通过；实际 LittleFS 与 stage 逐文件一致。
完整安装模型 20 项、安装进程 3 项通过，覆盖精确旧表迁移、原 NVS／新 userdata 保护、
15 MiB 写入上限、扇区重叠、摘要错误和设备身份错误。真实签名包的合并导出逐段一致、
空隙 0xFF、固定 15 MiB 长度及篡改拒绝通过。
49 张表真实解码／再编码一致，28 组坏输入拒绝；绑定／旧事务模型 2,056 个中断点通过 ASan/UBSan。
旧配对资源事务模型用于格式回归，不表示当前共享资源支持运行中升级。
本批未访问设备／串口、未刷入，实机启动与显示仍待验证。
证据 `.stick-local/artifacts/sdgoods-15m-20261108/`，签名包
`.stick-local/firmware/sdgoods/single-15m-release-20261108/`，合并镜像
`.stick-local/firmware/sdgoods/sdgoods-factory-15m-20261108.bin`。

历史设备验收（2026-11-06，旧双 12 MiB 布局，不能作为新布局验收）：
MAC ec:da:3b:67:10:5c，首次迁移及签名 Release 完整安装共 8 个区域摘要通过；
重启 A 槽、debug=0／trace=0，原档 restored=1／hatched=1、伙伴 1／261、save ok，原触摸映射加载。
应用 1,479,440 B，LittleFS 4,218,880 / 12,582,912 B；15 秒未见 panic 或循环重启。
应用 SHA-256 `5ae520272841c2911d2e8f672ed99ed3d35886692bc54d3059164b655602f479`。
包 `.stick-local/firmware/sdgoods/resource-ab-release-production-20261106/`；
证据 `.stick-local/artifacts/sdgoods-resource-storage-20261105/` 下
`install-production-final.log`、`boot-production-final.log`、`verification.json`。
旧应用内 USB ACK 超时未修复；当前共享资源布局明确关闭该更新协议。

早期完整圆屏／跨进程保存和多目标构建记录仍属旧布局验证。
ancient_valley、mist_forest、valley_region 三个既有遮挡像素断言在冻结修改前同样失败，未覆盖基线。
串口首帧计时不包括前面的全量资源检查；实体面板、扬声器、手指触摸、真实断电与稳定帧率仍需独立验收。

### 目录修正（2026-11-08）

用户实机反馈两种整包均停在 Invalid content catalog；串口与真实 Release host 同时复现。
技能教学生成器直接使用 280 个招式的原始快照，正式 OFFICIAL_MOVES 已按玩法删除 PP／携带物依赖技能；
教学表未同步过滤，产生 168、180、343、373、374、450、512 七个悬空 ID（共 99 条引用）。
现统一过滤策略，教学表由 1,895 条变为 1,796 条，49 张表合计 346,165 B。
启动校验继续严格拒绝未知 ID，并输出出错技能 ID。打包工具从真实 LittleFS 解包后运行
同一份 ContentValidation.cpp；测试先验证正常目录，再做错误注入，避免无关错误让负例假通过。
旧整包 sdgoods-factory-{15m,32m}-20261108.bin 均停止发布；使用带 catalog-fix 的修正版。
本轮证据 `.stick-local/artifacts/sdgoods-invalid-catalog-20261108/`。

修正版实机结果：15 MiB 与最终 32 MiB 签名包各段均定点安装、写后摘要通过，
启动后持续观察 30 秒返回 layout=1／ready=1，无目录错误或崩溃；当前设备运行 32 MiB 修正版。
实际连续首装镜像未用于覆盖设备 NVS，已逐段核对镜像与实测签名包一致、间隙为 0xFF。
最终产物集中 `.stick-local/releases/sdgoods/catalog-fix-20261108/{15m,32m}/`，
摘要与完整证据路径见该目录 verification.json；实体面板／手指与声音仍不以串口结果替代。

## 2026-11-09 默认改为 16 MiB

默认布局 `sdgoods-16m-single-v1`：双 3 MiB 应用、9 MiB 资源，结束于 0x1000000；
与微雪规划中的 16 MiB 几何对齐，微雪现有固件分区本次不变。物理容量仍按 32 MiB 校验。
不再新建 15 MiB 发布包，安装器仍允许从精确旧 15 MiB 表迁移；保留 `--full-storage` 的 32 MiB／24 MiB 资源方案。
Release 构建、23 项安装、3 项进程、13 项架构、2312 个旧事务中断点、49 表／28 坏输入、
实际 LittleFS 启动目录校验、签名／篡改拒绝及 16 MiB 整包逐段／空隙校验通过。
本轮无 USB 串口设备，未刷机；不把历史 15 MiB／32 MiB 实机结果当作 16 MiB 验收。
产物 `.stick-local/releases/sdgoods/16m-20261109/`；证据 `.stick-local/artifacts/sdgoods-16m-20261109/`。
