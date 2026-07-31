#!/usr/bin/env bash
# release_web.sh — 一键构建并发布 StickMon Web 模拟器到 flasher 站点
#
# 用法：
#   tools/release_web.sh                  # 构建 + 部署到 flasher
#   EMSDK=/path/to/emsdk tools/release_web.sh
#   FLASHER=/path/to/flasher tools/release_web.sh
#
# 前置：
#   1. 游戏代码改动已合入 web-simulator 分支（脚本不做 git 合并）
#   2. emsdk 可用（默认 /workspace/emsdk，沙箱 /tmp 会被清空，勿放 /tmp）
#
# 流程：Emscripten 构建 → 拷贝产物到 flasher public/sim/ → 内容哈希化
# 部署即时生效（静态文件，无需重启服务）：index.html 为 no-cache 立即可见，
# 哈希资产 immutable 长缓存，URL 随内容自动变化。

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK="${EMSDK:-/workspace/emsdk}"
FLASHER="${FLASHER:-/workspace/projects}"
SIM_DIR="$FLASHER/public/sim"

# --- 前置检查 ---
[ -f "$EMSDK/emsdk_env.sh" ] || { echo "[release_web] 错误: 未找到 emsdk ($EMSDK)，可用 EMSDK 环境变量指定"; exit 1; }
[ -d "$SIM_DIR" ] || { echo "[release_web] 错误: 未找到 flasher 模拟器目录 $SIM_DIR，可用 FLASHER 环境变量指定"; exit 1; }
[ -f "$FLASHER/scripts/hash-sim-assets.js" ] || { echo "[release_web] 错误: 未找到 hash-sim-assets.js（FLASHER 指向错误？）"; exit 1; }
command -v node >/dev/null || { echo "[release_web] 错误: 需要 node"; exit 1; }

# --- 1/3 构建 ---
echo "[release_web] 1/3 Emscripten 构建..."
cd "$REPO_ROOT"
# shellcheck disable=SC1091
source "$EMSDK/emsdk_env.sh" >/dev/null
tools/build_web.sh

# --- 2/3 拷贝产物 ---
echo "[release_web] 2/3 拷贝产物到 $SIM_DIR ..."
cp web/stickmon.js web/stickmon.wasm web/stickmon.data "$SIM_DIR/"
# web/index.html 是模拟器页面唯一编辑源，hash-sim-assets.js 会自动改写其中资源引用
cp web/index.html "$SIM_DIR/index.html"
# 面板图存在则一并拷贝（内容未变时哈希相同，hash 脚本会自动沿用）
[ -f web/device-body.webp ] && cp web/device-body.webp "$SIM_DIR/"

# --- 3/3 内容哈希化 ---
echo "[release_web] 3/3 内容哈希化..."
node "$FLASHER/scripts/hash-sim-assets.js"

echo "[release_web] 完成。静态文件即时生效，无需重启服务。"
echo "[release_web] 提醒: 如需归档，请分别在 StickMon 与 flasher 仓库 git 提交并推送。"
