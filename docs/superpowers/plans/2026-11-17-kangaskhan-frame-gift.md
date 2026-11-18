# Kangaskhan Frame Gift Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans inline. Keep changes uncommitted until requested.

**Goal:** Implement the approved three-dialogue, once-per-save photo-frame gift without opening the furniture workshop.

**Architecture:** Shared gift rules reserve one stable furniture ownership bit. A shared native-pixel controller renders the approved dialogue and frame for360/368px surfaces. Each application's existing market transaction boundary saves the grant before exposing the reward.

**Tech Stack:** C++17, TextBlock/Canvas565, existing SaveCodec and market transactions, native engine/App hosts.

**Spec:** Approved conversation and `.stick-local/artifacts/kangaskhan-frame-gift-preview-20261117/`.

## Global Constraints

- Exact dialogue: `袋龙：你好呀！我正为工坊开张做准备呢。` / `袋龙：这是我试做的相框，送你吧！` / `袋龙：工坊还在布置中，开张了记得来捧场！`.
-112×112px wood frame centered on screen, black alpha156 backdrop, centered32px `获得相框`, no button.
- A fresh completed tap dismisses the reward; the grant tap's UP cannot dismiss it.
- First gift only; later visits show the final preparation dialogue. No craft menu, purchase, auto placement or consumable inventory changes.
- Use existing `room.ownedFurniture`, reserve bit0 for the photo frame, preserve all other bits and `placedFurniture`; no save schema change.
- Failed saves roll back ownership and permit retry; sound/reward only after a durable grant.
- Preserve market pause, input ownership, power cancellation and render cadence.
- No V1/OSTB builds, device access, flash, commit or push.

## Review Focus

- Repeated UP, drag/cancel and lock must neither skip the gift nor award twice.
- Full inventory, zero coins, period changes and reload must not change gift eligibility except ownership.
- Save failure/slow save must not show a false success or charge anything.
- Long dialogue, round edges, clip bands and swapped buffers must preserve full text and identical output.
- Reuse existing glyphs unchanged; append only missing16/32px glyphs using the canonical rasterizer.

### Task1: Shared rule/controller and regression gate

**Files:** Add `src/game/MarketWorkshopService.h`, `src/ui/common/MarketWorkshopUi.h`, `tools/sdgoods_market_workshop_tests.h`; modify `tools/sdgoods_market_host.cpp`, `src/core/UiStrings.h`.

- [x] Add failing native gift/controller/save checks and record RED.
- [x] Implement `hasFrame(const GameState&)`, `grantFrame(GameState&) -> Result{OK,ALREADY}`.
- [x] Implement shared `MarketWorkshopUi`: open/update/touch/committed/close, layout-aware rendering and ITEM_GET feedback.

### Task2: Production entry and durability

**Files:** Modify `src/scenes/ExploreScene.{h,cpp}` and shared AMOLED `AmoledApp.{h,cpp}`; add actual native/App gift journeys.

- [x] Register115 as a service and route entry, touches, render, update and lock/exit cancellation.
- [x] Commit grant via MarketTransaction using existing app save methods; preserve coin and item state.
- [x] Check first visit, failed/slow save, repeat visit/reload, cancel and zero-coin behavior in actual applications.

### Task3: Resources and verification

**Files:** Append missing glyphs in `data/packs/dev/fonts/zh{16,32}.smonfont`; update `AGENTS.md` with verified outcomes.

- [x] Preserve every existing glyph byte; add only the new dialogue's missing characters.
- [x] Run native sanitizer/App journeys, save/font/architecture regressions and current SDK builds; no V1/OSTB.
- [x] Inspect actual dialogue/reward frames; conduct one scoped review and correct important findings.
- [x] Leave work uncommitted, report practical validation limits.

## Authorization

The user approved the rendered design and explicitly asked to implement it. No further approval gate is needed.
