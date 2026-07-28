'use strict';
/** API 路由：公开浏览/下载 + 管理员 CRUD */

const path = require('path');
const fs = require('fs');
const crypto = require('crypto');
const multer = require('multer');
const store = require('./db');
const { consumeBytes, DAILY_BYTE_CAP } = require('./ratelimit');

// ESP32-S3 固定偏移（bootloader / partitions / boot_app0 由芯片与 SDK 约定），
// firmware 与 littlefs 的偏移从固件工程的 partitions.csv 解析
const DEFAULT_OFFSETS = {
  bootloader: 0x0,
  partitions: 0x8000,
  boot_app0: 0xe000,
  firmware: 0x10000,
  littlefs: 0x450000,
};
const BIN_KEYS = Object.keys(DEFAULT_OFFSETS);
const MAX_BIN_SIZE = 20 * 1024 * 1024;
const MAX_IMG_SIZE = 5 * 1024 * 1024;

/**
 * 解析 partitions.csv：第一个 app 分区 → firmware 偏移，
 * subtype 为 spiffs/littlefs 的 data 分区 → littlefs 偏移。
 * 解析失败时回退到 DEFAULT_OFFSETS。每次调用重新读文件，改分区表无需重启。
 */
function loadOffsetsFromCsv(csvPath) {
  const offsets = { ...DEFAULT_OFFSETS };
  let appSeen = false, fsSeen = false;
  try {
    for (const line of fs.readFileSync(csvPath, 'utf8').split('\n')) {
      const cols = line.split(',').map((c) => c.trim());
      if (cols.length < 4 || !cols[0] || cols[0].startsWith('#')) continue;
      const [, type, subtype, offStr] = cols;
      const off = Number(offStr);
      if (!Number.isInteger(off) || off < 0) continue;
      if (type === 'app' && !appSeen) {
        offsets.firmware = off;
        appSeen = true;
      } else if (type === 'data' && (subtype === 'spiffs' || subtype === 'littlefs') && !fsSeen) {
        offsets.littlefs = off;
        fsSeen = true;
      }
    }
  } catch { /* csv 不存在或不可读，用默认偏移 */ }
  return offsets;
}

const IMAGE_TYPES = {
  'image/png': { ext: '.png', magic: [0x89, 0x50, 0x4e, 0x47] },
  'image/jpeg': { ext: '.jpg', magic: [0xff, 0xd8, 0xff] },
  'image/webp': { ext: '.webp', magic: [0x52, 0x49, 0x46, 0x46] }, // RIFF
};

function sha256File(p) {
  return crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
}

function hasMagic(p, magic) {
  const fd = fs.openSync(p, 'r');
  const buf = Buffer.alloc(magic.length);
  fs.readSync(fd, buf, 0, magic.length, 0);
  fs.closeSync(fd);
  return magic.every((b, i) => buf[i] === b);
}

function registerRoutes(app, ctx, { apiLimiter, downloadLimiter, requireAdmin }) {
  const { db, dataDir } = ctx;
  const fwRoot = path.join(dataDir, 'firmware');
  const imgRoot = path.join(dataDir, 'images');

  const upload = multer({
    storage: multer.diskStorage({
      destination: path.join(dataDir, 'tmp'),
      filename: (req, file, cb) => cb(null, `${crypto.randomUUID()}.part`),
    }),
    limits: { fileSize: MAX_BIN_SIZE, files: 8 },
  });

  // ---------- 公开接口 ----------

  /** 固件列表（主页画廊数据） */
  app.get('/api/firmwares', apiLimiter, (req, res) => {
    const list = store.listFirmwares(db).map((fw) => ({
      id: fw.id, name: fw.name, version: fw.version, notes: fw.notes,
      chip: fw.chip, downloads: fw.downloads, createdAt: fw.createdAt,
      updatedAt: fw.updatedAt, totalSize: fw.totalSize,
      image: fw.image ? `/api/firmwares/${fw.id}/image` : null,
    }));
    res.json({ firmwares: list });
  });

  /** 固件封面图 */
  app.get('/api/firmwares/:id/image', apiLimiter, (req, res) => {
    const fw = store.getFirmware(db, req.params.id);
    if (!fw || !fw.image) return res.status(404).json({ error: 'not found' });
    const p = path.join(imgRoot, fw.image);
    if (!p.startsWith(imgRoot) || !fs.existsSync(p)) return res.status(404).json({ error: 'not found' });
    res.setHeader('Cache-Control', 'public, max-age=3600');
    res.sendFile(p);
  });

  /** esp-web-tools manifest（浏览器刷机入口，一次刷机拉取一次 → 记一次下载量） */
  app.get('/api/firmwares/:id/manifest.json', downloadLimiter, (req, res) => {
    const fw = store.getFirmware(db, req.params.id);
    if (!fw) return res.status(404).json({ error: 'not found' });
    store.bumpDownloads(db, fw.id, 1);
    res.json({
      name: fw.name,
      version: fw.version,
      new_install_prompt_erase: false,
      builds: [{
        chipFamily: fw.chip,
        parts: fw.files.map((f) => ({
          path: `/files/${fw.id}/${encodeURIComponent(f.file)}`,
          offset: f.offset,
        })),
      }],
    });
  });

  /** bin 文件下载（频控 + 每日流量配额） */
  app.get('/files/:id/:filename', downloadLimiter, (req, res) => {
    const fw = store.getFirmware(db, req.params.id);
    if (!fw) return res.status(404).json({ error: 'not found' });
    const entry = fw.files.find((f) => f.file === req.params.filename);
    if (!entry) return res.status(404).json({ error: 'not found' });
    if (!consumeBytes(req, entry.size)) {
      return res.status(429).json({
        error: `当日下载流量超限（每 IP 每日 ${Math.round(DAILY_BYTE_CAP / 1024 / 1024)}MB），请明天再来`,
      });
    }
    const p = path.join(fwRoot, fw.id, entry.file);
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('X-Content-SHA256', entry.sha256);
    res.sendFile(p);
  });

  // ---------- 首页轮播图 ----------

  const slideRoot = path.join(dataDir, 'slides');
  const slideUpload = multer({
    storage: multer.diskStorage({
      destination: path.join(dataDir, 'tmp'),
      filename: (req, file, cb) => cb(null, `${crypto.randomUUID()}.part`),
    }),
    limits: { fileSize: MAX_IMG_SIZE, files: 6 },
  });

  /** 轮播列表（公开） */
  app.get('/api/slides', apiLimiter, (req, res) => {
    res.json({ slides: store.listSlides(db).map((s) => ({ id: s.id, url: `/api/slides/${s.id}/image` })) });
  });

  /** 轮播图片（公开） */
  app.get('/api/slides/:id/image', apiLimiter, (req, res) => {
    const slide = store.getSlide(db, req.params.id);
    if (!slide) return res.status(404).json({ error: 'not found' });
    const p = path.join(slideRoot, slide.file);
    if (!p.startsWith(slideRoot) || !fs.existsSync(p)) return res.status(404).json({ error: 'not found' });
    res.setHeader('Cache-Control', 'public, max-age=3600');
    res.sendFile(p);
  });

  /** 上传轮播图（管理员，一次最多 6 张） */
  app.post('/api/admin/slides', requireAdmin, slideUpload.array('images', 6), (req, res) => {
    const files = req.files || [];
    const cleanup = () => files.forEach((f) => fs.rmSync(f.path, { force: true }));
    try {
      if (!files.length) return res.status(400).json({ error: '请选择至少一张图片' });
      const added = [];
      for (const f of files) {
        const type = IMAGE_TYPES[f.mimetype];
        if (!type || !hasMagic(f.path, type.magic)) {
          return res.status(400).json({ error: '仅支持 PNG / JPEG / WebP，且每张不超过 5MB' });
        }
        const id = crypto.randomUUID();
        const file = `${id}${type.ext}`;
        fs.renameSync(f.path, path.join(slideRoot, file));
        store.insertSlide(db, { id, file });
        added.push(id);
      }
      res.json({ ok: true, added });
    } finally {
      cleanup();
    }
  });

  /** 删除轮播图（管理员） */
  app.delete('/api/admin/slides/:id', requireAdmin, (req, res) => {
    const slide = store.getSlide(db, req.params.id);
    if (!slide) return res.status(404).json({ error: 'not found' });
    store.deleteSlide(db, slide.id);
    fs.rmSync(path.join(slideRoot, slide.file), { force: true });
    res.json({ ok: true });
  });

  /** 调整轮播顺序（管理员）：body { dir: -1 | 1 } */
  app.put('/api/admin/slides/:id/move', requireAdmin, (req, res) => {
    const dir = Number(req.body && req.body.dir);
    if (dir !== -1 && dir !== 1) return res.status(400).json({ error: 'dir 必须为 -1 或 1' });
    if (!store.swapSlideOrder(db, req.params.id, dir)) {
      return res.status(400).json({ error: '无法移动（不存在或已在边缘）' });
    }
    res.json({ ok: true });
  });

  // ---------- 管理员接口 ----------

  /** 下一个建议版本号（上传表单自动填入） */
  app.get('/api/admin/next-version', requireAdmin, (req, res) => {
    res.json({ version: store.suggestNextVersion(db) });
  });

  // ---------- 构建产物自动拉取 ----------

  /** 按相对路径定位各 bin：boot_app0 在 PlatformIO 框架目录，其余在工程构建目录 */
  function resolveBuildFiles() {
    return BIN_KEYS.map((key) => {
      const p = key === 'boot_app0'
        ? ctx.bootApp0Bin
        : path.join(ctx.buildDir, `${key}.bin`);
      let exists = false, size = 0, mtime = null;
      try {
        const st = fs.statSync(p);
        exists = st.isFile();
        size = st.size;
        mtime = Math.round(st.mtimeMs);
      } catch { /* 文件不存在 */ }
      return { key, path: p, exists, size, mtime };
    });
  }

  /** 构建产物清单（管理员）：文件是否存在 + 大小 + 修改时间 + 总大小 */
  app.get('/api/admin/build-info', requireAdmin, (req, res) => {
    const files = resolveBuildFiles();
    res.json({
      buildDir: ctx.buildDir,
      files,
      totalSize: files.filter((f) => f.exists).reduce((s, f) => s + f.size, 0),
      ready: files.some((f) => f.key === 'firmware' && f.exists),
    });
  });

  /** 单个构建产物下载（管理员）：供管理页自动填入文件选择框 */
  app.get('/api/admin/build-files/:key', requireAdmin, (req, res) => {
    const f = resolveBuildFiles().find((x) => x.key === req.params.key);
    if (!f || !f.exists) return res.status(404).json({ error: 'not found' });
    res.setHeader('Cache-Control', 'no-store');
    res.sendFile(f.path);
  });

  /** 上传新固件（multipart：5 个 bin + 可选封面图 + 元数据） */
  app.post('/api/admin/firmwares', requireAdmin, upload.fields([
    ...BIN_KEYS.map((k) => ({ name: k, maxCount: 1 })),
    { name: 'image', maxCount: 1 },
  ]), (req, res) => {
    const cleanup = () => {
      for (const f of Object.values(req.files || {}).flat()) {
        fs.rmSync(f.path, { force: true });
      }
    };
    try {
      const name = String(req.body.name || '').trim().slice(0, 64);
      const version = String(req.body.version || '').trim().slice(0, 32);
      const notes = String(req.body.notes || '').trim().slice(0, 2000);
      if (!name || !version) return res.status(400).json({ error: '名称与版本号必填' });
      if (!req.files || !req.files.firmware) {
        return res.status(400).json({ error: 'firmware.bin 必须上传' });
      }

      const id = crypto.randomUUID();
      const offsets = loadOffsetsFromCsv(ctx.partitionsCsv);
      const files = [];
      for (const key of BIN_KEYS) {
        const arr = req.files[key];
        if (!arr || !arr.length) continue;
        const f = arr[0];
        files.push({
          key, offset: offsets[key],
          file: `${key}.bin`,
          size: f.size,
          sha256: sha256File(f.path),
          tmpPath: f.path,
        });
      }

      // 封面图：MIME + 魔数双重校验
      let imageName = null;
      const imgArr = req.files.image;
      if (imgArr && imgArr.length) {
        const img = imgArr[0];
        const type = IMAGE_TYPES[img.mimetype];
        if (!type || img.size > MAX_IMG_SIZE || !hasMagic(img.path, type.magic)) {
          return res.status(400).json({ error: '封面图仅支持 PNG / JPEG / WebP，且不超过 5MB' });
        }
        imageName = `${id}${type.ext}`;
        fs.renameSync(img.path, path.join(imgRoot, imageName));
      }

      const dir = path.join(fwRoot, id);
      fs.mkdirSync(dir, { recursive: true });
      for (const f of files) {
        fs.renameSync(f.tmpPath, path.join(dir, f.file));
        delete f.tmpPath;
      }

      const now = Date.now();
      store.insertFirmware(db, {
        id, name, version, notes, image: imageName, files,
        chip: 'ESP32-S3', createdAt: now, updatedAt: now,
      });
      res.json({ ok: true, id });
    } finally {
      cleanup();
    }
  });

  /** 修改元数据（名称/版本/说明）。bin 文件不可替换，需换文件请删旧传新 */
  app.put('/api/admin/firmwares/:id', requireAdmin, (req, res) => {
    const fw = store.getFirmware(db, req.params.id);
    if (!fw) return res.status(404).json({ error: 'not found' });
    const name = String(req.body.name ?? fw.name).trim().slice(0, 64);
    const version = String(req.body.version ?? fw.version).trim().slice(0, 32);
    const notes = String(req.body.notes ?? fw.notes).trim().slice(0, 2000);
    if (!name || !version) return res.status(400).json({ error: '名称与版本号必填' });
    store.updateFirmwareMeta(db, fw.id, { name, version, notes });
    res.json({ ok: true });
  });

  /** 删除固件（连带磁盘文件与封面图） */
  app.delete('/api/admin/firmwares/:id', requireAdmin, (req, res) => {
    const fw = store.getFirmware(db, req.params.id);
    if (!fw) return res.status(404).json({ error: 'not found' });
    store.deleteFirmware(db, fw.id);
    fs.rmSync(path.join(fwRoot, fw.id), { recursive: true, force: true });
    if (fw.image) fs.rmSync(path.join(imgRoot, fw.image), { force: true });
    res.json({ ok: true });
  });
}

module.exports = { registerRoutes, DEFAULT_OFFSETS };
