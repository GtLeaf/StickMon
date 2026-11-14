# Kecleon Star Box Opening Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline. User already selected preview and requested implementation; no further approval. No commits/push/merge/flash unless requested.

**Goal:** Integrate the approved star dissolve box presentation after a durable purchase.
**Architecture:** MarketShopUi owns three added pages BOX_ARRIVE/BOX_WAIT/BOX_OPEN and one small clock. Pure MarketBoxPerformance draws existing assets; App saves/rewards exactly as today and consumes deferred sound.
**Tech Stack:** C++17/Canvas565/GameAssets/native rule and full AmoledApp hosts.
**Spec:** docs/superpowers/specs/2026-11-14-kecleon-box-opening-design.md.

## Global Constraints
600ms arrival (180ms text fade +420ms travel),48→72px,(44,240)→(184,316); explicit complete panel tap;1250ms opening,900ms ITEM_GET once; RESULT after controls fade. No schema/resources/frame-limit change. Existing lock-to-room and snapshots preserved. Keep two frozen rewards/ordinary purchases/advice/scroll.

## Review Focus
- Saving fails or only mirror succeeds: presentation follows durable transaction, no extra reward.
- Purchase lift/held drags/cancel/oldUP/rapid taps must never auto-open/skip/submit twice.
- Lock/display-off pauses elapsed time, cancels touch and sound; real lock-to-room retains bought items.
- Two boxes, frame delta and long idle cannot change or duplicate saved reward.
- Panel/camera/badges remain stationary; old pre-purchase stock/button fades, not sold-out text.

### Task 1: Controller and presentation
Files: src/ui/common/MarketShopUi.h; NEW MarketBoxPerformance.h; UiStrings.h; NEW tools/market_box_opening_host.cpp and test_market_box_opening.py.
- [x] Write native behavior test: saved box must not show immediate RESULT or ITEM_GET; failure stays CONFIRM; explicit later tap opens; drag/cancel/outside ignored; wait indefinite; one cue at reveal; RESULT guarded until1250; return preserves scroll; normal purchase remains immediate.
- [x] Observe behavioral RED on prior code, then implement three phase pages, clock, frozen presentation stock, pure renderer and click hint; renderer bounded to panel.
- [x] Run native ASan/UBSan tests GREEN, including zero-delta pause and close cancellation.

### Task 2: Production App integration and evidence
Files: tools/market_procurement_amoled_host.cpp; tools/market_sfx_amoled_host.cpp; new real App capture host or shared box journey; test wrappers.
- [x] Adapt real two-box and actual PCM tests for deferred reveal, no click/purchase cues, save retries and paused/cancelled presentation.
- [x] Export arrival/wait/open/reveal/result frames from production App; inspect actual animation and all existing box reward icons.
- [x] Run seasonal rule/UI/App, sound App, shared scroll, SaveCodec/migration, architecture/font/assets and lock regression. No unrelated baseline rewrites or firmware success claims.

### Task 3: Finish
Files: specs/market-stalls/kecleon-ui.md; AGENTS.md; task evidence.
- [x] Independent final code review; fix meaningful findings with regression evidence.
- [x] Update specs/ledger and concise final summary with native GIF and device limits; leave uncommitted feature branch for user.
