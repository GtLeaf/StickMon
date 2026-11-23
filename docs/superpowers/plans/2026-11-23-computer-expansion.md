# Computer Expansion Implementation Plan

> Implement inline with executing-plans and TDD; one independent whole-change review after verification.

**Goal:** Add Kecleon's fixed periodic expansion card and unlock computer capacity from 20 to 200.
**Architecture:** Shared StorageCapacity rules and existing MarketShop/MarketTransaction. Keep eight seasonal slots stable; append one virtual expansion row with a separate durable period marker. Serialize new state via a new schema and migrate portable/raw predecessors explicitly.
**Tech Stack:** C++17, Python asset tools, native renderer, ESP-IDF V2/SDGoods.
**Spec:** docs/superpowers/specs/2026-11-23-computer-expansion-design.md

## Global Constraints
- Exact prices, capacity, UI copy and stock policy are in the approved spec.
- Preserve old companions, currencies, care records, supply and furniture/photos.
- Do not build Waveshare V1/OSTB, flash, commit or push.

## Review Focus
- Current-period legacy saves get one card without rerolling existing goods.
- Repeated presses, reboot, stale market and maximum cannot grant extra upgrades.
- Save failure or allocation failure leaves capacity, coins and quota unchanged.
- 200 companions, daily-care records and transaction snapshots fit heap/codec limits and do not exhaust the device stack.
- Both screens fit 13000 and 180 to 200, with clipped/swapped rendering and correct return navigation.

### Task 1: Shared capacity, fixed market offer and storage enforcement
Files: src/game/{GameState.h,StorageCapacity.h,MarketShopService.h,MarketTransaction.h,FriendshipService.cpp,ShopService.cpp}, src/core/GameEngine.cpp, firmware/amoled_1_8_v1/main/{AmoledApp.cpp,app/BattleSession.cpp}, tools/{computer_expansion_host.cpp,test_computer_expansion.py}.
Interfaces: StorageCapacity::{capacity,nextCapacity,price,canExpand}; MarketShop::{EXPANSION,rowCount,stock}, existing check/buy/ensurePeriod/priceForSlot.
- [x] Write/run failing tests for default capacity, nine prices, periodic availability, immediate purchase, insufficient coins, full bag, max, stale requests and rollback.
- [x] Add fixed maximum array, unlocked-capacity enforcement, independent period quota and safe transaction snapshots.
- [x] Run shared rules and existing market/friendship checks.

### Task 2: Versioned persistence and legacy migration
Files: src/core/{SaveCodec.h,SaveCodec.cpp,SaveManager.cpp}, tools/{save_codec_host.cpp,save_migration_host.cpp,computer_expansion_host.cpp}.
Interfaces: new schema reads old 20-slot/22-care schemas; persists upgrades/quota and up to 200 companions/care records, raw migrations use frozen legacy layout.
- [x] Add failing tests for schema-10/raw legacy preservation, full 200-member roundtrip, invalid capacity and upgrade fields.
- [x] Implement schema, bounded buffers, legacy-array counts and explicit raw-layout translation.
- [x] Run codec/migration and transaction restart tests.

### Task 3: Asset and dual-device UI
Files: origin_asset/icon/market/items/computer_expansion.png, src/assets/GameAssets*, tools/generate_game_assets.py, data assets, src/ui/{common/MarketShopUi.h,round_touch/MarketShopUi.h,common/MarketExpansionUi.h}, computer count UIs, native journey tests.
Interfaces: native 48px icon, virtual ninth row; capacity helper renders native arrow without unsupported glyph, simple confirm/result on both targets.
- [x] Add failing journey assertions for highest price, successful capacity result, sold-out restart, maximum row disappearance and save rollback.
- [x] Install approved icon, wire virtual row and shared minimal panel rendering, use unlocked counts.
- [x] Run native UI journeys/render checks, generate actual first/max purchase/result previews.

### Task 4: Final verification and review
- [x] Run focused market, save, capacity, assets and architecture regressions.
- [x] Build Waveshare V2 and SDGoods only; preserve evidence and report actual limitations.
- [x] Independent whole-change review; resolve important findings and verify fixes.


## Completed validation

Implemented on `feat/computer-expansion`, without commit/push or flashing. Evidence: `.stick-local/artifacts/computer-expansion-20261123/verification.json`.

- Shared capacity, all nine prices, period quota, bag independence, insufficient balance, full/last roster slot and rollback passed.
- Frozen schema-10 and raw-v11 fixtures preserve companions, coins, care and nonzero LRU records. Full200-companion/202-care roundtrip, marker failure, missing/corrupt sibling, allocation/read/mount failures, reset failure, malformed counts and larger valid future envelopes passed.
- Actual round purchase/retry/save/restart and AMOLED controller journeys passed with13000 and180→200; clipping/both byte orders, hidden maximum row and real round touch at contact199 passed.
- Existing market, furniture, standby, codec/migration, contacts, assets and architecture checks passed.
- Waveshare V2 and SDGoods SDK builds passed. Targeted SDK frame usage dropped from20–24KiB to48–528 bytes; no V1/OSTB build or device flash.
- Independent review found persistence, bounds and stack defects; all were resolved and covered in the fix pass. No deferred findings.
