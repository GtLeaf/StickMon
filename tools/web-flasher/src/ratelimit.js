'use strict';
/**
 * 频控：按 IP 滑动窗口计数 + 固件下载的每日流量配额。
 * 内存实现（单进程部署足够）；重启后计数清零，属可接受语义。
 */

const WINDOW_CLEANUP = 60 * 1000;

function clientIp(req) {
  return req.ip || req.socket.remoteAddress || 'unknown';
}

/** 通用滑动窗口限流器 */
function slidingWindow({ windowMs, max, message }) {
  const hits = new Map(); // ip -> number[]
  setInterval(() => {
    const cutoff = Date.now() - windowMs;
    for (const [ip, arr] of hits) {
      const kept = arr.filter((t) => t > cutoff);
      if (kept.length) hits.set(ip, kept); else hits.delete(ip);
    }
  }, WINDOW_CLEANUP).unref();

  return (req, res, next) => {
    const ip = clientIp(req);
    const now = Date.now();
    const cutoff = now - windowMs;
    const arr = (hits.get(ip) || []).filter((t) => t > cutoff);
    if (arr.length >= max) {
      res.setHeader('Retry-After', Math.ceil(windowMs / 1000));
      return res.status(429).json({ error: message || '请求过于频繁，请稍后再试' });
    }
    arr.push(now);
    hits.set(ip, arr);
    next();
  };
}

/**
 * 固件下载配额：次数 + 每日字节数双闸。
 * 一次完整刷机需要下载 5 个 bin（合计通常 < 8MB）。
 */
const dailyBytes = new Map(); // `${ip}:${yyyy-mm-dd}` -> bytes
const DAILY_BYTE_CAP = Number(process.env.DAILY_DOWNLOAD_MB || 200) * 1024 * 1024;

function consumeBytes(req, size) {
  const day = new Date().toISOString().slice(0, 10);
  const key = `${clientIp(req)}:${day}`;
  const used = (dailyBytes.get(key) || 0) + size;
  dailyBytes.set(key, used);
  // 防止 Map 无限增长：超过 5000 条时清空（最坏情况是配额重置一次，可接受）
  if (dailyBytes.size > 5000) dailyBytes.clear();
  return used <= DAILY_BYTE_CAP;
}

const apiLimiter = slidingWindow({
  windowMs: 60 * 1000, max: 120, message: '请求过于频繁，请稍后再试',
});

const loginLimiter = slidingWindow({
  windowMs: 10 * 60 * 1000, max: 10, message: '登录尝试过多，请 10 分钟后再试',
});

const downloadLimiter = slidingWindow({
  windowMs: 10 * 60 * 1000, max: 30, message: '下载次数超限（每 10 分钟最多 30 个文件），请稍后再刷',
});

module.exports = { apiLimiter, loginLimiter, downloadLimiter, consumeBytes, DAILY_BYTE_CAP };
