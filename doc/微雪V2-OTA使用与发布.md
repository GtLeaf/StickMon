# 微雪 V2 OTA 使用与发布

2026-10-09。已实现微雪 AMOLED 1.8 **V2 Lite / Claw** 的应用 OTA。V1 和 Stick 没有 OTA 入口。

## 玩家操作

1. 首次通过 USB 安装本次完整 V2 引导包，包含回滚 bootloader、分区表、应用和更新后的素材。不要整片擦除，也不要写入 NVS 镜像；现有存档和 brainfs 保留。
2. Debug 版本中打开游戏「设置 → 系统更新」（正式版暂不展示此入口）。游戏先完成存档，再重启到独立更新页；拜访中或保存失败时拒绝进入。
3. 手机连接设备显示的 `StickMon-OTA-xxxx` 热点，输入屏幕上的随机密码。打开 `http://192.168.4.1`。
4. 设置家里的 2.4 GHz Wi-Fi 和刷机站正式 HTTPS 地址。设置存放在独立 `ota` 命名空间，不修改 Claw 配置或 Wi-Fi 驱动的持久化配置。
5. 点击「检查更新」，接通稳定电源，确认后安装。下载成功会自动重启。取消或失败保留原固件；点设备上的「返回游戏」退出。

连接路由器时热点信道可能切换，手机短暂掉线后需重新连接热点。V2 当前没有可靠的充电状态检测接口，因此电源由用户明确确认，不能声称固件能检测 USB 供电。

首期只更新应用，不下载字体、精灵、音频或地图。不匹配的素材集合需要完整 USB 安装。更新维护期间游戏和 Claw 不启动，退出通过重启恢复；不使用后台自动安装。

## 构建与签名

版本位于 `firmware/amoled_1_8_v2/ota/version.json`。每次发布递增 `build`（uint32），`version` 用于显示；Lite 与 Claw 独立匹配。

```sh
# 可选：把正式站点作为固件的默认地址；未设置时玩家在手机配置页填写。
export STICKMON_OTA_ENDPOINT=https://your-update-site.example
bash tools/build_amoled_variant.sh v2 lite
bash tools/build_amoled_variant.sh v2 claw

python3 tools/sign_ota_release.py firmware/amoled_1_8_v2/build-lite --key .ota/release-key.pem
python3 tools/sign_ota_release.py firmware/amoled_1_8_v2/build-claw --key .ota/release-key.pem
```

`build-lite/out/`、`build-claw/out/` 包含供刷机站上传的五个 bin；签名后另有 `ota-release.json`。设备只下载对应的 `firmware.bin`，不使用绝对刷写偏移进行 OTA。

本机已生成发布私钥 `.ota/release-key.pem`（仅本机，权限 0600，Git 忽略）。**请单独备份此私钥**。公钥在 `firmware/amoled_1_8_v2/ota/public-key.pem`，与刷机站 `config/ota-public-key.pem` 完全一致。私钥不能上传到站点或提交 Git；更换公钥需要 USB 安装新的信任基线。

签名为 ECDSA P-256 / SHA-256，签原始 JSON 字节，清单用 base64 包装。签名工具拒绝 debug 包和未启用回滚的构建。构建配置缓存也会由 V2 构建脚本启用回滚；直接运行 idf.py 的旧目录需先启用此选项，否则构建会拒绝继续。

资源指纹来自最终 SPIFFS staging 文件，包含 UI 从 `ui_amoled.smonfx` 到目标 `ui.smonfx` 的映射。每条记录为 `相对路径\t文件SHA256\n`，排序后整体 SHA256；设备逐文件复核实际资源。只增加中文提示也会改变字体基线，所以已有设备需要本次 USB 首刷。

## 刷机站接入

使用独立工程 `/Users/gtleaf/project/web/stickmon-flasher`。

1. 在管理后台注册游戏 `stickmon` 对应的微雪 V2 设备：默认 slug `waveshare-amoled-1-8-v2`，ESP32-S3，16 MB。已有 slug 不同时设置服务端 `OTA_DEVICE_SLUG`，不能用 V1 设备代替。
2. 设备烧录模板：bootloader=`0x0`，partitions=`0x8000`，boot_app0=`0xf000`，firmware=`0x20000`，littlefs=`0x620000`。这里的 `littlefs.bin` 实际承载 SPIFFS，是现有上传字段命名。
3. 在固件页按该组合上传同一次构建的五个 bin。普通上传不会自动成为 OTA 更新。
4. 在新「OTA 更新」页选择该固件及对应 `ota-release.json`，验证并保存；再发布到 Lite/Claw 的 stable 通道。beta 通道也可管理，当前设备默认只使用 stable。
5. 撤回通道后停止向新检查推送。已发布记录和固件文件继续保留，避免正在下载的设备因删除失败；修改网站展示版本不会改变签名中的固件版本。

生产站和固件下载都必须是 HTTPS。对象存储模式为每次检查生成新的签名 URL，设备安装前再次刷新，服务器不代理 bin；本地文件模式必须配置 `OTA_PUBLIC_BASE_URL` 为正式 HTTPS 源站地址。设备不会接受 HTTP 下载或跳转，不依赖浏览器 CORS。

接口：

- `GET /api/ota/v1/latest?target=waveshare-amoled-1.8-v2&flavor=lite&channel=stable&build=2026100901`
- `GET /api/ota/v1/releases/:id/firmware.bin`：本机文件兜底或对象存储跳转。
- `GET /api/admin/ota`
- `POST /api/admin/ota/releases`：`{firmwareId,envelope}`，导入签名清单。
- `PUT /api/admin/ota/channels/:flavor/:channel`：`{releaseId}`，null 撤回。

设备配置门户只接受连接设备热点的请求，并用会话 token 防止跨站提交。管理接口沿用会话和 CSRF 守卫。发布/通道写入在对象存储备份完成后才成功返回，失败恢复旧通道。备份内容恢复同时恢复游戏、设备、组合、签名清单、通道及递增高水位，保留本地管理员配置。站点只持有公钥，不具备签任意固件的能力。

## 安全与回滚

设备拒绝错误型号、类型、布局、素材、存档 schema/version、debug、相同或更低 build、过大应用和错误签名。TLS 使用 IDF 信任根及网络校时。下载写入非运行应用槽，大小、SHA256 和 ESP 镜像验证全部成功后才设置启动分区；不写 resources、NVS 存档或 brainfs。

新应用 pending verify 阶段禁止平台 blob 写入、删除和 NVS 自动擦除。核对资源和可解码快照，初始化平台、触摸、应用与两次显示传输后确认；120 秒启动超时会触发重启。未确认即重启或崩溃由回滚 bootloader 回到旧应用。签名防止服务端文件被替换，但没有启用硬件 Secure Boot 或 eFuse 防降级。

首期只允许存档格式不变的更新。双槽存档是正常保存冗余，不能当作跨格式迁移的长期升级备份。资源 OTA 和跨 schema 更新另行设计。

## 验证状态

本地已进行 V2 Lite/Claw 构建、Stick/V1 回归构建；存档 codec/迁移、架构边界、设置及发布包测试。刷机站自动测试验证签名、错误目标与文件匹配、通道隔离、下载字节、撤回、高水位、备份失败恢复和内容灾备恢复。

尚未进行真实 V2 的 Wi-Fi/热点联调、升级中断电、错误新程序回滚、USB 首刷及存档保留验证；也未发布线上网站。上线前用 baseline 与更高 build 的同素材候选包完成一轮真机升级，检查 Lite、Claw、退出恢复、错密码/断网/取消及未确认重启回滚。
