'use strict';
/** 管理员认证：scrypt 口令哈希 + 随机 token 会话（httpOnly Cookie） */

const crypto = require('crypto');

const COOKIE_NAME = 'stickmon_admin';
const SESSION_TTL = 12 * 60 * 60 * 1000; // 12 小时

function hashPassword(password) {
  const salt = crypto.randomBytes(16);
  const hash = crypto.scryptSync(password, salt, 64);
  return `scrypt:${salt.toString('hex')}:${hash.toString('hex')}`;
}

function verifyPassword(password, stored) {
  const [scheme, saltHex, hashHex] = String(stored || '').split(':');
  if (scheme !== 'scrypt' || !saltHex || !hashHex) return false;
  const hash = crypto.scryptSync(password, Buffer.from(saltHex, 'hex'), 64);
  const expected = Buffer.from(hashHex, 'hex');
  return hash.length === expected.length && crypto.timingSafeEqual(hash, expected);
}

/**
 * 初始化管理员口令：
 *  - 优先取环境变量 ADMIN_PASSWORD（部署时注入）
 *  - 未设置则生成随机口令，仅在首次启动时打印到控制台一次
 */
function ensureAdmin(db) {
  const existing = db.prepare("SELECT value FROM settings WHERE key = 'admin_hash'").get();
  if (existing) return;
  const envPw = process.env.ADMIN_PASSWORD;
  const pw = envPw || crypto.randomBytes(9).toString('base64url');
  db.prepare("INSERT INTO settings (key, value) VALUES ('admin_hash', ?)").run(hashPassword(pw));
  if (envPw) {
    console.log('[auth] 管理员口令已从 ADMIN_PASSWORD 环境变量初始化');
  } else {
    console.log('======================================================');
    console.log('[auth] 首次启动，已生成随机管理员口令（仅显示一次）:');
    console.log(`       ${pw}`);
    console.log('       请立即记录；之后可用 ADMIN_PASSWORD 环境变量重置');
    console.log('======================================================');
  }
}

function createSession(db) {
  const token = crypto.randomBytes(32).toString('base64url');
  const now = Date.now();
  db.prepare('INSERT INTO sessions (token, created_at, expires_at) VALUES (?, ?, ?)')
    .run(token, now, now + SESSION_TTL);
  return token;
}

function getSessionToken(req) {
  const cookie = req.headers.cookie || '';
  for (const part of cookie.split(';')) {
    const [k, ...v] = part.trim().split('=');
    if (k === COOKIE_NAME) return decodeURIComponent(v.join('='));
  }
  return null;
}

function isValidSession(db, token) {
  if (!token) return false;
  const row = db.prepare('SELECT expires_at FROM sessions WHERE token = ?').get(token);
  return !!row && row.expires_at > Date.now();
}

function cookieHeader(token, maxAgeSec) {
  const parts = [
    `${COOKIE_NAME}=${encodeURIComponent(token)}`,
    'Path=/', 'HttpOnly', 'SameSite=Strict',
    `Max-Age=${maxAgeSec}`,
  ];
  if (process.env.NODE_ENV === 'production') parts.push('Secure');
  return parts.join('; ');
}

const loginHandler = (ctx) => (req, res) => {
  const { password } = req.body || {};
  const row = ctx.db.prepare("SELECT value FROM settings WHERE key = 'admin_hash'").get();
  if (!password || !verifyPassword(password, row && row.value)) {
    return res.status(401).json({ error: '口令错误' });
  }
  const token = createSession(ctx.db);
  res.setHeader('Set-Cookie', cookieHeader(token, SESSION_TTL / 1000));
  res.json({ ok: true });
};

const logoutHandler = (ctx) => (req, res) => {
  const token = getSessionToken(req);
  if (token) ctx.db.prepare('DELETE FROM sessions WHERE token = ?').run(token);
  res.setHeader('Set-Cookie', cookieHeader('', 0));
  res.json({ ok: true });
};

const meHandler = (ctx) => (req, res) => {
  res.json({ admin: isValidSession(ctx.db, getSessionToken(req)) });
};

/** 管理接口守卫：会话有效 + 自定义头（配合 SameSite=Strict 阻断 CSRF） */
const requireAdmin = (ctx) => (req, res, next) => {
  if (!isValidSession(ctx.db, getSessionToken(req))) {
    return res.status(401).json({ error: '需要管理员登录' });
  }
  if (req.headers['x-requested-with'] !== 'fetch') {
    return res.status(403).json({ error: '非法请求来源' });
  }
  next();
};

module.exports = { ensureAdmin, loginHandler, logoutHandler, meHandler, requireAdmin };
