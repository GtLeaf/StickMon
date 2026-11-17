# SDGOODS market entry and deferred lock plan

> Use superpowers:executing-plans inline. User approved implementation.
> No automatic commit/push/flash; keep all earlier uncommitted work.

**Goal:** Actual-map cream market card and market return before display lock.
**Architecture:** Shared asset bytes, native round market-only view consuming
open/countdown frame; Scene lock-preparation hook with GameEngine readiness;
SDGOODS queued lock and save-before-final-lock through PowerActions.
**Tech Stack:** C++17, Canvas565, existing native state-machine hosts/SDK.
**Spec:** specs/market-stalls/sdgoods-entry-lock.md.

## Constraints
Only market selection theme changes; public navigation geometry unchanged;
66ms market; no new rules/schema/resource bytes, V1/OSTB builds or device access.

## Review focus
- Parked calendar refresh and disabled closed departure, release gate unchanged.
- Actual map art/round clips/byte order and existing nonmarket/micro pixels.
- Requests during paid animation/exit/fade/departure cannot lose or repeat state.
- Save failures/retry/cancel/long press and wake never deadlock or darken early.
- Platform/header interfaces preserve idle/shutdown/other scene behavior.

## Task1: Market selector
Files: shared assets/ExplorePostcardArt.h, micro compatibility include and generator;
ExploreRouteUiModel, ExploreScene, round ExploreSelectUi/RoundPage, native tests.
- [x] Market card actual warm-pixel assertion RED.
- [x] Implement shared art move, market model/status, native card/theme/cache path.
- [x] Clock/day/touch/clip/swap/pixel/gate tests GREEN with real captures.

## Task2: Return-before-lock
Files: core Scene/GameEngine, ExploreScene; SDGOODS Platform/PowerActions;
platform hardware host and actual native scene/power tests.
- [x] Queued power dispatcher assertion RED.
- [x] Prepare hook, deferred physical/API lock, retry/cancel, save-before-dark.
- [x] Actual paid service→return→home→save→lock, departure/exit, errors/wake GREEN.
- [x] Earlier3stall/Debug-release/shared-power/save/architecture regressions,
      SDGOODS+V2 builds, actual screenshots and final fresh read-only review.

Verified2026-11-16: native full3stall journeys, warm card open/closed/day and
departure gate, parked day invalidation, full/banded and swapped rendering.
Hidden resident preview RED→GREEN: no displayed strip/animation behind card,
separate resident preload retained. Actual normal departure/fade/door/home/save
and deferred lock tested for3paid service types, failed final save1s retry,
wake, busy departure and canceled request. Native power request wrap/order,
release-gate/ordinary-dialogue checks,8pre-batch nonmarket/header pixel
comparisons and60unchanged AMOLED performance samples pass.27 save/architecture
and7shared lock checks pass; SDGOODS and V2 firmware builds pass.

Shared screenshot art is byte-identical; no new resource bytes, schema/rules,
V1/OSTB builds or device access. The existing automatic dim-only idle policy is
unchanged; physical/API lock requests are the tested scope. Independent final
read-only review approved with no outstanding findings. Source uncommitted.
Evidence: `.stick-local/artifacts/sdgoods-market-entry-lock-20261116/`.
