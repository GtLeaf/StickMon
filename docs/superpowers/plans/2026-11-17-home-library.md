# Home Library Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline; no commits requested.

**Goal:** Ship the black-style `房间 → 家居库` flow for the owned photo frame, with fixed placement and durable removal.

**Architecture:** Shared pure catalogue/placement transaction plus shared native-coordinate controller. Render the same owned placement through the existing room cache in both home presenters and the preview. Keep the library inside the existing ROOM navigation context.

**Tech Stack:** C++17, Canvas565, TextBlock, existing MarketListScroll/TouchTap, SaveCodec/SaveManager, native real application hosts.

**Spec:** docs/superpowers/specs/2026-11-17-home-library-design.md

## Global Constraints

- Exact name `家居库`; retain current black colors, existing assets/fonts and frame limits.
- Only the existing photo-frame gift is production inventory. No demo skins, crafting, prices, or new ownership gates.
- Stable ID0 owns bit0; placement uses the separate existing bitfield. Preserve unknown bits and legacy saves.
- User already requested implementation; no further approval or integration step is needed.
- Reuse current feature checkout and preserve its uncommitted gift/skill changes; evidence in `.stick-local/artifacts/home-library-live-20261117/`.
- No V1/OSTB builds, flash, commit, push or merge.

## Review Focus

- Failed or slow saves, repeated UP, lock/cancel: no false success, placement leak or duplicate commit.
- Unknown placement/ownership bits and newer/failing saves: preserve existing state and retry safely.
- Preview cache versus actual home/day/night: cancelled preview must never become the saved room.
- Round centered-surface transition: both rendering and touch coordinates switch at the same boundary.
- Empty inventory, future long catalogue, clip bands, swapped buffers: usable controls and bounded memory.

### Task1: Shared furniture and presentation

- [x] Add failing native home-library probe and record RED.
- [x] Create `src/game/HomeFurnitureService.h`: stable catalogue, owned/placed query, same-slot replacement, save-rollback transaction.
- [x] Create `src/presentation/FurnitureArt.h`, delegate gift drawing without visual changes; extend `RoomRenderer` with explicit placement cache key and full-room thumbnail.
- [x] Create `src/ui/common/HomeLibraryUi.h`: owned list, future category filtering, inertia, preview/confirm/removal, feedback/error and cancellation. Add exact copy in UiStrings.
- [x] Verify real service/controller/renderer behavior, including failed save, preview isolation and clip/swap.

### Task2: Touch applications and saved home

- [x] Extend round room UI with third action and native library surface; host commits via shared transaction. Preserve food/shower and outer return navigation.
- [x] Extend shared AMOLED room routing/render/update/cancellation with the same controller; preserve black room menu pixels and entry geometry.
- [x] Pass owned placement through native HomeFrame and AMOLED HomeViewModel into RoomRenderer; no preview-state globals.
- [x] Add actual native/MenuScene and AMOLED application journeys covering empty/owned/try/back/place/remove/failure/reload/lock and existing menu actions.

### Task3: Resources and verification

- [x] Append missing16/32px glyphs only; verify existing glyph bytes unchanged.
- [x] Run relevant native ASan/UBSan, save/font/architecture/family and existing market regressions, SDGOODS and AMOLED V2 SDK builds.
- [x] Inspect actual screenshots and perform one fresh-context scoped final review; fix important findings with a reproducer.
- [x] Update project memory and ledger; leave all work uncommitted and report hardware limits.
