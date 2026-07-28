# StickMon Web Flasher

浏览器直刷 StickS3 固件分发站。用户插 USB 打开网页即可一键刷机；管理员在 `/admin` 上传、编辑、删除固件版本，并可管理首页轮播图。

`seed_slides/` 内置 3 张初始轮播图，首次启动自动导入；之后在管理后台自由增删排序。

## 本地运行

```bash
npm install
ADMIN_PASSWORD=你的口令 npm run dev
# 打开 http://localhost:7100 （主页）  http://localhost:7100/admin （管理）
```

未设置 `ADMIN_PASSWORD` 时，首次启动会在控制台打印一个随机管理员口令（只显示一次）。

## 上传固件前准备

在固件工程（PlatformIO）里构建：

```bash
pio run                    # 产出 .pio/build/<env>/ 下的 bootloader.bin partitions.bin boot_app0.bin firmware.bin
pio run -t buildfs         # 产出 littlefs.bin
```

烧录偏移由服务端启动时读取 `partitions_stickmon_8mb.csv`（可用 `PARTITIONS_CSV` 环境变量覆盖）自动确定，管理页无需填写；分区表调整后直接重新上传即可，无需重启服务。

## 前端约定

- 玩家可见文案集中在 `public/flasher-i18n.js`（`window.FLASHER_I18N`）：`dialog` 是 esp-web-tools 弹窗的英中对照表（主页通过监听弹窗 shadowRoot 实时替换），`debug` 是串口调试面板文案。新增弹窗文案先补对照表。
- 弹窗内不提供 Logs & Console / 擦除入口；刷机永不全片擦除（保留 NVS 存档）。安装成功后点「ok」直接关闭弹窗，不再回到 Install 菜单页。串口日志用主页顶部「调试」弹窗（Web Serial，115200）。
- 上述两点依赖 esp-web-tools@10 的内部字段（`_startInstall` / `_state` / `_manifest`），升级大版本时需复查。

## 服务器部署

要求：Node.js ≥ 22.5（用到内置 `node:sqlite`），一个域名 + HTTPS（WebSerial 强制）。

```bash
npm install
ADMIN_PASSWORD=你的口令 PORT=7100 TRUST_PROXY=1 NODE_ENV=production npm start
```

Caddy 反代（自动 HTTPS）：

```
flash.example.com {
  reverse_proxy 127.0.0.1:7100
}
```

systemd 常驻示例：

```ini
[Unit]
Description=StickMon Web Flasher
After=network.target

[Service]
WorkingDirectory=/opt/web-flasher
Environment=ADMIN_PASSWORD=你的口令
Environment=PORT=7100
Environment=TRUST_PROXY=1
Environment=NODE_ENV=production
ExecStart=/usr/bin/node server.js
Restart=always

[Install]
WantedBy=multi-user.target
```

备份：整个 `data/` 目录（SQLite + bin + 封面图）即全部状态。

## 环境变量

| 变量 | 默认 | 说明 |
|------|------|------|
| `PORT` | 7100 | 监听端口 |
| `DATA_DIR` | ./data | 数据目录 |
| `ADMIN_PASSWORD` | — | 首次启动时的管理员口令 |
| `DAILY_DOWNLOAD_MB` | 200 | 每 IP 每日下载流量配额 |
| `TRUST_PROXY` | — | 置 1：反代后取真实 IP（频控需要） |
| `NODE_ENV` | — | production 时 Cookie 加 Secure |

设计细节见 [设计文档.md](设计文档.md)。
