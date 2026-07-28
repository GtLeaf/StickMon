'use strict';
/**
 * StickMon Web Flasher — 固件分发与浏览器直刷服务
 *
 * 技术栈：Express + node:sqlite（Node >= 22.5 内置，零原生依赖）
 * 前端刷机：esp-web-tools (esptool-js / Web Serial API)
 */

const path = require('path');
const fs = require('fs');
const os = require('os');
const express = require('express');

const { initDb } = require('./src/db');
const { ensureAdmin, loginHandler, logoutHandler, meHandler, requireAdmin } = require('./src/auth');
const { apiLimiter, loginLimiter, downloadLimiter } = require('./src/ratelimit');
const { registerRoutes } = require('./src/routes');

const ROOT = __dirname;
const DATA_DIR = process.env.DATA_DIR || path.join(ROOT, 'data');
const PORT = Number(process.env.PORT || 7100);
// 自动拉取构建产物：默认指向 StickMon 工程的 PlatformIO 构建输出目录，
// 可用 FIRMWARE_BUILD_DIR / BOOT_APP0_BIN 环境变量覆盖
const BUILD_DIR = process.env.FIRMWARE_BUILD_DIR
  || path.resolve(ROOT, '..', '..', '.pio', 'build', 'm5stick-s3');
const BOOT_APP0_BIN = process.env.BOOT_APP0_BIN
  || path.join(os.homedir(), '.platformio', 'packages', 'framework-arduinoespressif32', 'tools', 'partitions', 'boot_app0.bin');
// 烧录偏移从固件工程的分区表解析，可用 PARTITIONS_CSV 环境变量覆盖
const PARTITIONS_CSV = process.env.PARTITIONS_CSV
  || path.resolve(ROOT, '..', '..', 'partitions_stickmon_8mb.csv');

// ---- 运行时目录 ----
for (const d of ['firmware', 'tmp', 'images', 'slides']) {
  fs.mkdirSync(path.join(DATA_DIR, d), { recursive: true });
}

const db = initDb(path.join(DATA_DIR, 'flasher.db'));
ensureAdmin(db);

// ---- 初始轮播图播种：slides 表为空且存在 seed_slides/ 时导入 ----
(function seedSlides() {
  const seedDir = path.join(ROOT, 'seed_slides');
  if (!fs.existsSync(seedDir)) return;
  const count = db.prepare('SELECT COUNT(*) AS n FROM slides').get().n;
  if (count > 0) return;
  const files = fs.readdirSync(seedDir).filter((f) => /\.(png|jpe?g|webp)$/i.test(f)).sort();
  if (!files.length) return;
  const crypto = require('crypto');
  for (const f of files) {
    const id = crypto.randomUUID();
    const ext = path.extname(f).toLowerCase();
    const dest = `${id}${ext}`;
    fs.copyFileSync(path.join(seedDir, f), path.join(DATA_DIR, 'slides', dest));
    db.prepare('INSERT INTO slides (id, file, sort, created_at) VALUES (?, ?, ?, ?)')
      .run(id, dest, files.indexOf(f) + 1, Date.now());
  }
  console.log(`[slides] 已导入 ${files.length} 张初始轮播图（来自 seed_slides/）`);
})();

const app = express();
app.disable('x-powered-by');
// 部署在 Nginx/Caddy 反代之后时设置 TRUST_PROXY=1，否则频控拿到的全是反代 IP
if (process.env.TRUST_PROXY === '1') app.set('trust proxy', 1);

app.use(express.json({ limit: '256kb' }));

// 安全响应头
app.use((req, res, next) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('X-Frame-Options', 'DENY');
  res.setHeader('Referrer-Policy', 'same-origin');
  next();
});

// ---- 静态资源（页面本身，轻量，走通用频控）----
app.use('/assets', apiLimiter, express.static(path.join(ROOT, 'public'), { maxAge: '1h' }));

// ---- API ----
const ctx = { db, dataDir: DATA_DIR, buildDir: BUILD_DIR, bootApp0Bin: BOOT_APP0_BIN, partitionsCsv: PARTITIONS_CSV };
app.post('/api/login', loginLimiter, loginHandler(ctx));
app.post('/api/logout', logoutHandler(ctx));
app.get('/api/me', meHandler(ctx));

registerRoutes(app, ctx, { apiLimiter, downloadLimiter, requireAdmin: requireAdmin(ctx) });

// ---- 页面 ----
app.get('/', apiLimiter, (req, res) => res.sendFile(path.join(ROOT, 'public', 'index.html')));
app.get('/admin', apiLimiter, (req, res) => res.sendFile(path.join(ROOT, 'public', 'admin.html')));

// 404 / 错误兜底
app.use('/api', (req, res) => res.status(404).json({ error: 'not found' }));
app.use((err, req, res, next) => {
  if (err && err.code === 'LIMIT_FILE_SIZE') {
    return res.status(413).json({ error: '文件超出大小限制' });
  }
  console.error('[server error]', err);
  res.status(500).json({ error: 'internal error' });
});

app.listen(PORT, () => {
  console.log(`StickMon Web Flasher listening on http://localhost:${PORT}`);
  console.log(`数据目录: ${DATA_DIR}`);
});
