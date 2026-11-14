# Market Transaction and Coin HUD Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline, user already approved and said 执行. Preserve previous box changes. No commit/push/flash requested.

**Goal:** Route every market payment through one durable transaction boundary and one rolling coin HUD.
**Architecture:** Game::MarketTransaction::commit applies a typed service, saves or rolls back, emits CoinChange only after success. App private commitMarketTransaction wires the coin motion once; all handlers call it. MarketCoinMotion has bounded per-digit rolls and two short float tags; shared MarketCoinHud renders current frame for all stalls.
**Tech Stack:** C++17 header services, native ASan/UBSan and production AmoledApp captures.
**Spec:** docs/superpowers/specs/2026-11-14-market-coins-design.md.

## Global Constraints
450ms ease-out per-digit downward spend/upward gain; actual state commits immediately. Tag32px/1px horizontal weight,0..120ms fade in,12px lift,650..900ms fade out. Two tags max in separate lanes. Preserve per-digit fractional position on consecutive events. Reset on entry/exit, no replay from load; no schema/assets/frame-rate or ordinary-shop changes. Keep box save-before-show/one-shot sounds.

## Review Focus
- All five paid service categories hit same boundary; rejected, failed saves, free services never animate.
- Partial snapshot commits survive reload and must animate exactly once; int64 deltas handle uint32 bounds.
- Retarget during roll, mixed signs, 999↔1000 and long balance cannot jump backward/overflow/clip.
- Display pause does not catch up; close/lock clears presentation, never alters committed balances.
- Frame rendering does not mutate model/state, duplicate sounds, cache unbounded data or increase market FPS.

### Task 1: durable transaction and motion
Files NEW src/game/MarketTransaction.h, src/ui/common/MarketCoinMotion.h, tools/market_coins_host.cpp, tools/test_market_coins.py.
- [x] RED missing APIs; native service outcomes verify rollback/event boundaries, positive/negative/zero changes; motion verifies450/900, mixed retarget continuity, zero-delta pause, reset, signed32-bit extremes/tags.
- [x] Implement commit(state,apply,save,notify)->Outcome<Result>; CoinChange{before,after,delta()}; enum Status REJECTED/SAVE_FAILED/COMMITTED.
- [x] Implement reset(value), committed(CoinChange), update(delta), frame() consumed by renderer. Max10 digit wheels with fractional continuous positions and two bounded stamps, no heap.
- [x] ASan/UBSan native GREEN.

### Task 2: shared HUD and App boundary
Files NEW src/ui/common/MarketCoinHud.h; AmoledApp.h/cpp; MarketShopUi/CareUi/TutorUi.h; new real App coins host + wrapper.
- [x] Write actual App regression proving payments update true balance immediately, keep old display first frame, then rolls and negative tag, failures and partial durable commit; teach/train/massage/tea all emit same event; gains and width extremes controlled fixtures.
- [x] Replace duplicated saves with private commitMarketTransaction; advice/recall keep typed outcomes. Reset coin presentation per service entry/close; update only active time with independent clock.
- [x] All UI renders consume optional shared motion, fallback directly synchronized balance for standalone renderers. Same right anchor; preserve titles/camera.
- [x] Native real App captures and actualPCM/box/season/care/tutor/lock regressions; inspect paid/gain/roll/carry/width/tag frames.

### Task 3: review and finish
- [x] Independent review current coin increment against before/ snapshot; address meaningful defects, docs/AGENTS/evidence; final concise outcome and native GIF, no firmware claims.

Completion evidence: .stick-local/artifacts/market-coins-20261114/verification.md. All authorized code retained uncommitted; no push or flash.

Typography follow-up: user approved32px native Unscii, one-pixel horizontal weight, existing outline,48px tag lanes. Existing real App render/clip/byte-order/payment tests plus fresh visual capture verify layout; no new design-constant tests.
