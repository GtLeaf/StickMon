# Market Mystery Box Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the approved300-coin daily mystery box as the fourth scrollable Kecleon shelf entry.
**Architecture:** Keep the three regular shelf slots and expose a fourth service row, not a new inventory ItemId. Reuse the shop state's two reserved bytes for purchased flag and a persisted1..100 draw; append those bytes in codec schema7 without changing the raw GameState layout/version8. Save inventory, payment and quota together before showing the result.
**Tech Stack:** C++17, Canvas565, existing SGFX/SMFN resource generators, native App and sanitizer tests.
**Spec:** User approved the preceding reward table and requested execution plus a scrollable shelf. Detailed rules go in specs/market-stalls/kecleon-ui.md.

## Global Constraints
- Keep all existing uncommitted stall motion/layout changes and the current feature checkout. No automatic commit, push or device access.
- Price300; one box per game day. Rewards: honey3/25%, super potion3/25%, max repel2/15%, revive1/15%, heart scale1/12%, unlocked stone1/6%, candy1/2%.
- Unavailable reward tiers and exhausted daily candy quota fall back to honey3; no reward reroll for full stacks. Check space for the entire reward before charging.
- Freeze the private draw at initial shelf creation; failed saves/re-entry/reload do not reroll. Mix day and the initial leader identity before persisting it, consistent with the existing private shelf draw.
- Keep normal three-slot stock layout, GameState binary size/offsets and SAVE_VERSION8; codec schema7 appends box state after care data, schemas1..6 default it to unused.
- Persisted valid shelves with old zero draw initialize only box state, retaining sold stock. Invalid box flags fail closed until next day.
- Keep y208/14px rounded panels, badge, motion timing and current frame scheduling. Clip the four72px shelf rows to y216..440, capture drag through release and preserve scroll on back.
- Add the generated icon through the existing48px resource pipeline; preserve every old Kind ID/frame/palette. Add missing glyphs with existing Sarasa rasterization only.
- Retain all evidence under .stick-local/artifacts/market-mystery-box-20261113/.

## Review Focus
- Migration of schema6 and raw version8 records preserves purchases and Chansey records.
- Full quantities, exhausted candy quota, locked rewards and every probability boundary.
- Failed saves and stale day confirmations cannot lose coins, reroll or double-grant.
- Slow drags, CANCEL, border crossing and lock/standby cannot select the box or start walking.
- New resource bytes preserve old decoded assets and glyphs and remain reproducible without the optional external Essentials checkout.

## Task 1: Shared rewards and save compatibility
**Files:** src/game/MarketShopService.h; src/game/GameState.h; src/core/SaveCodec.{h,cpp}; src/core/SaveManager.cpp; tools/market_mystery_box_host.cpp; tools/test_market_shop.py.
**Interfaces:** ROW_COUNT4, BOX3; Reward{ItemId item,uint8_t amount}; boxRewardForRoll(state,roll0..99), check and buy(slot3) with optional Reward* output; priceForSlot(state,slot).
- [x] Add tests for all100 draw buckets, unlock fallback, daily limit, stack98+3 refusal, stale period, re-entry and save round trips. Observe missing feature fail.
- [x] Implement pure rules and schema7 append; migrate zero-draw old shelves without regenerating regular stock; validate invalid box state conservatively.
- [x] Run ASan/UBSan reward/rule tests and schema6/raw8 migration checks.

## Task 2: Icon and glyph resources
**Files:** origin_asset/icon/market/mystery-box*.png; tools/prepare_mystery_box_icon.py; tools/generate_game_assets.py; src/assets/GameAssets.h; data/packs/dev/game/ui.smonfx; font packs and manifests.
**Interfaces:** append MARKET_MYSTERY_BOX Kind at the end;48px frame loaded by the existing GameAssets::drawCentered API.
- [x] Add native48px/Kind preservation and existing-frame/glyph parity checks.
- [x] Prepare the user's generated icon, append the new UI frame using the production Writer, and retain full-generator support. Add only missing Chinese glyphs with the production rasterizer.
- [x] Run resource mapping/generator tests and inspect enlarged/native icon previews.

## Task 3: Scrollable shelf and atomic App purchase
**Files:** src/ui/common/MarketShopUi.h; src/core/UiStrings.h; firmware/amoled_1_8_v1/main/AmoledApp.cpp; tools/market_shop_amoled_host.cpp; specs/market-stalls/kecleon-ui.md.
**Interfaces:** scrollOffset(), cancelTouch clears gesture state; committed(saved,ItemId reward,uint8_t amount). App buy obtains Reward and passes it only after the save result.
- [x] Add real App drag-to-fourth-row, tap, confirmation, save-failure/retry, result, persisted quota and scroll-back checks; observe old three-row behavior fail.
- [x] Wire the fourth row, clipped rendering and drag capture; adapt confirmation/result for box rewards without revealing a pending prize. Keep regular purchase behavior.
- [x] Run complete shop App/rules suite, save migration, related Gengar/Chansey checks and architecture/family checks. Inspect exported shelf/confirm/result frames and reward icon.
- [x] Obtain independent review, resolve material findings, update specs/memory/ledger and report hardware/toolchain limits.
