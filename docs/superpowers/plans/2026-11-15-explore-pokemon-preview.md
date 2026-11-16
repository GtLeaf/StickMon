# Explore Pokemon preview implementation plan

> Execute inline using superpowers:executing-plans. User approved the two-mode
> proposal and requested implementation. No commit, push or new approval gate.

**Goal:** Pokeball toggle with larger touch target, scene-locked manual Pokemon
pagination, matching dots and hidden/inert scene arrows while previewing.
**Architecture:** Existing postcard layout/view/model and native App touch flow.
No new gameplay state or resource packs. Use a monochrome themed Pokeball mask.
**Tech Stack:** C++17, Canvas565, existing native host probes.
**Spec:** specs/explore-postcard-trial.md (updated to the approved proposal).

## Global constraints

368x448; card24/84/320x244; native FRONT1x, width272/gap12 grouping;
Pokeball32px, visual48x48 centered310/118, touch56x56 at282/90.
Scene lock lasts until toggle closed; footer departure/back unchanged.
Preview is manual only. No V1/OSTB build, no save/schema/resource changes.

## Review focus

- Starting on the expanded toggle owns the gesture even when dragged outside.
- All card/indicator swipes in preview keep the selected destination fixed;
  hidden arrow taps cannot select scenes; vertical/cancel gestures are inert.
- Dots track actual width-based pages, including single/empty/loading groups.
- Partial refresh must include changed dots and enlarged toggle top90.
- Scene mode, market residents, locked destinations, navigation/departure and
  legacy compile-time fallback retain behavior.

## Task1: Two-mode preview and verification

Files: ui/ExplorePostcardLayout.h, ui/ExplorePostcardUi.h, AmoledApp.h/.cpp;
tools/explore_postcard_layout_host.cpp, explore_postcard_amoled_host.cpp,
test_explore_postcard.py; remove obsolete preview clock and its standalone test.

- [x] Extend native touch/idle/render regressions; observe RED for enlarged hit
      target, upper-card scene lock, hidden-arrow taps and manual-only preview.
- [x] Implement toggle geometry/monochrome icon and page-dependent dots/arrows; update App
      gesture ownership, tap handling, redraw bands; remove auto-preview clock.
- [x] Run postcard native and legacy fallback, layout sanitizer, architecture,
      family and scheduler checks. Build V2 only; inspect actual captures.
- [x] One final scoped independent review, address blockers, save evidence and
      update this plan. Leave work uncommitted.
