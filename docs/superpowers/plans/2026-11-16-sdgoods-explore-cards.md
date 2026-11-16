# SDGOODS scene cards implementation plan

> Use superpowers:executing-plans inline. User approved implementation and
> specified centered card with arrows beside it. Keep uncommitted prior work.

**Goal:** Unified native360 scene card and isolated species browsing.
**Architecture:** Selection owns mode/page/gestures; pure view+page layout use
shared cover bytes, species frames and existing unlock/market flags. Scene
supplies IDs/readiness, keeps preload, stops obsolete auto-strip scheduling.
**Tech Stack:** C++17, Canvas565, existing art/fonts/native engine tests.
**Spec:** specs/sdgoods-explore-cards.md.

## Constraints
Native1x species; no rule/schema/resources changes. Preserve Debug gates and
market return-before-lock. No V1/OSTB builds/hosts, flashing or auto commits.

## Review focus
- Scene identity vs visible-order indices, changing pool/loading and page clamps.
- Ball/arrow/swipe/CANCEL ownership and mode reset without accidental departure.
- Native species dimensions/circular viewport and unknown rare silhouettes.
- No obsolete automatic preview/33ms scheduling or needless static redraws.
- Release gating, scene return/lock and earlier transactions remain intact.

## Task1: Full card view, mode controller and proof
Files: round ExploreSelectUi.h/.cpp; new round ExploreSelectionView.h;
shared ExploreRouteUiModel.h; ExploreScene.cpp; native host/new card tests.
- [x] Side-arrow/native card center assertion RED on batch4.
- [x] New unified view and measured pages; mode/gesture controller and model IDs.
- [x] Scene readiness/preload and static scheduling; market resident preview.
- [x] Actual all7covers, preview/locked/hidden/loading/circle/clip/swap and mode
      input tests; prior services/lock/gate GREEN; retire explicitly superseded
      old nonmarket selector pixel comparisons, preserving frozen before/.
- [x] Staged ASan/UBSan, shared regressions and two SDK builds, inspect captures,
      one fresh final review; source retained uncommitted.
