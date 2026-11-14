# Explore Gradient Scene Implementation Plan

> Inline execution on feat/explore-postcard. User approved corrected gradient preview, with Lv removed. Preserve rollback branch backup/explore-selector-before-postcard-20261114 and STICKMON_EXPLORE_POSTCARD=0.

**Goal:** Replace the earlier postcard trial with the approved dark, large-scene UI.
**Spec:** specs/explore-postcard-trial.md and approved .stick-local/artifacts/explore-gradient-preview-20261114/explore-gradient-scene-fixed.html, excluding level labels.
**Architecture:** Shared bounded geometry/pagination helper consumed by renderer and App input. Existing rule/state/save/preview loading remain authoritative; generated row-RLE scene thumbnails are 168x146, rendered at 2x.

## Constraints and review focus

Scene x16/y80/w336/h292, shared public header, label top-left, bottom-up alpha0..179 with one corner boundary; original-colour FRONT sprites at assetScale1, max132px height, ground350, available width312 with12px gaps. Depart x20/y388/w328/h48. No habitat/more text or Lv. Market retains actual opening status. Upper-card gestures select destination; lower218..372 gestures page sprites only. Preserve pending/drag/cancel guards, static render scheduling and original legacy mode. No commit/push/flash.

### Task 1: Geometry, paging, generation, renderer and input

- [x] Add geometry/pool cases proving new target areas and native-size width grouping; observe old geometry fail.
- [x] Implement shared row/pool geometry, regenerate art, replace renderer with fixed gradient and 1x sprites, remove Lv.
- [x] Integrate native paging gestures and dirty ranges; preserve hidden silhouettes at native size and async loading.
- [x] Run native pool cases, actual App and legacy flows, full/partial and byte-order pixels, maximum sprite/edge/no-Lv checks; inspect captures and obtain independent review.
- [x] Update specification/evidence, leave uncommitted and maintain rollback.
