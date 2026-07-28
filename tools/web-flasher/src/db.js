'use strict';
/** SQLite 存储层（node:sqlite 内置模块，Node >= 22.5） */

const { DatabaseSync } = require('node:sqlite');

function initDb(dbPath) {
  const db = new DatabaseSync(dbPath);
  db.exec(`
    PRAGMA journal_mode = WAL;
    CREATE TABLE IF NOT EXISTS settings (
      key   TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS sessions (
      token      TEXT PRIMARY KEY,
      created_at INTEGER NOT NULL,
      expires_at INTEGER NOT NULL
    );
    CREATE TABLE IF NOT EXISTS firmwares (
      id         TEXT PRIMARY KEY,
      name       TEXT NOT NULL,
      version    TEXT NOT NULL,
      notes      TEXT NOT NULL DEFAULT '',
      image      TEXT,
      files      TEXT NOT NULL,          -- JSON: [{key, file, offset, size, sha256}]
      chip       TEXT NOT NULL DEFAULT 'ESP32-S3',
      downloads  INTEGER NOT NULL DEFAULT 0,
      created_at INTEGER NOT NULL,
      updated_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_firmwares_created ON firmwares(created_at DESC);
    CREATE TABLE IF NOT EXISTS slides (
      id         TEXT PRIMARY KEY,
      file       TEXT NOT NULL,
      sort       INTEGER NOT NULL,
      created_at INTEGER NOT NULL
    );
  `);
  // 定期清理过期 session
  setInterval(() => {
    try {
      db.prepare('DELETE FROM sessions WHERE expires_at < ?').run(Date.now());
    } catch { /* 忽略清理失败 */ }
  }, 10 * 60 * 1000).unref();
  return db;
}

function rowToFirmware(row) {
  if (!row) return null;
  const files = JSON.parse(row.files);
  return {
    id: row.id,
    name: row.name,
    version: row.version,
    notes: row.notes,
    image: row.image,
    chip: row.chip,
    downloads: row.downloads,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
    totalSize: files.reduce((s, f) => s + f.size, 0),
    files,
  };
}

function listFirmwares(db) {
  const rows = db.prepare('SELECT * FROM firmwares ORDER BY created_at DESC').all();
  return rows.map(rowToFirmware);
}

function getFirmware(db, id) {
  return rowToFirmware(db.prepare('SELECT * FROM firmwares WHERE id = ?').get(id));
}

function insertFirmware(db, fw) {
  db.prepare(`
    INSERT INTO firmwares (id, name, version, notes, image, files, chip, created_at, updated_at)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
  `).run(fw.id, fw.name, fw.version, fw.notes, fw.image, JSON.stringify(fw.files),
    fw.chip || 'ESP32-S3', fw.createdAt, fw.updatedAt);
}

function updateFirmwareMeta(db, id, { name, version, notes }) {
  db.prepare('UPDATE firmwares SET name = ?, version = ?, notes = ?, updated_at = ? WHERE id = ?')
    .run(name, version, notes, Date.now(), id);
}

function deleteFirmware(db, id) {
  db.prepare('DELETE FROM firmwares WHERE id = ?').run(id);
}

function bumpDownloads(db, id, n) {
  db.prepare('UPDATE firmwares SET downloads = downloads + ? WHERE id = ?').run(n, id);
}

/** 依据最新固件版本号推算下一个建议版本（semver patch +1） */
function suggestNextVersion(db) {
  const row = db.prepare('SELECT version FROM firmwares ORDER BY created_at DESC LIMIT 1').get();
  if (!row) return '0.1.0';
  const m = /^(\d+)\.(\d+)\.(\d+)(.*)$/.exec(row.version.trim());
  if (!m) return row.version;
  return `${m[1]}.${m[2]}.${Number(m[3]) + 1}${m[4] || ''}`;
}

module.exports = {
  initDb, listFirmwares, getFirmware, insertFirmware,
  updateFirmwareMeta, deleteFirmware, bumpDownloads, suggestNextVersion,
  listSlides, insertSlide, getSlide, deleteSlide, swapSlideOrder,
};

// ---------- 首页轮播图 ----------

function listSlides(db) {
  return db.prepare('SELECT id, file, sort, created_at AS createdAt FROM slides ORDER BY sort ASC, created_at ASC').all();
}

function insertSlide(db, { id, file }) {
  const max = db.prepare('SELECT COALESCE(MAX(sort), 0) AS m FROM slides').get().m;
  db.prepare('INSERT INTO slides (id, file, sort, created_at) VALUES (?, ?, ?, ?)')
    .run(id, file, max + 1, Date.now());
}

function getSlide(db, id) {
  return db.prepare('SELECT id, file, sort FROM slides WHERE id = ?').get(id);
}

function deleteSlide(db, id) {
  db.prepare('DELETE FROM slides WHERE id = ?').run(id);
}

/** 与相邻项交换顺序；dir = -1 上移 / +1 下移。返回是否发生交换 */
function swapSlideOrder(db, id, dir) {
  const cur = getSlide(db, id);
  if (!cur) return false;
  const neighbor = db.prepare(
    `SELECT id, sort FROM slides WHERE sort ${dir < 0 ? '<' : '>'} ? ORDER BY sort ${dir < 0 ? 'DESC' : 'ASC'} LIMIT 1`
  ).get(cur.sort);
  if (!neighbor) return false;
  db.prepare('UPDATE slides SET sort = ? WHERE id = ?').run(neighbor.sort, cur.id);
  db.prepare('UPDATE slides SET sort = ? WHERE id = ?').run(cur.sort, neighbor.id);
  return true;
}
