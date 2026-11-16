# SDGOODS Gengar implementation plan

> Use superpowers:executing-plans inline. User approved second batch; no new
> permission gate or automatic commits. Existing first batch stays intact.

**Goal:** Gengar commerce and performances on native round360.
**Architecture:** RoundMarketTutorUi platform page/hit state; shared game
services for all mutation; ExploreScene exclusive active-service dispatch and
one coin clock. Optional transform in shared performance painter.
**Tech Stack:** C++17, Canvas565, shared resources, native engine hosts.
**Spec:** specs/market-stalls/sdgoods-tutor.md.

## Global Constraints
No schema/rule/resource changes;66ms market; no V1/OSTB builds/hosts or flash.
Keep uncommitted Kecleon work and Debug-only entry; Chansey dialogue only.

## Review Focus
- Gift/period saved once, failed persistence leaves no unsaved services.
- Paid move/member/slot identity cannot change during a quoted request.
- Long names, recall lists, button widths and sprites remain inside circle.
- Slow saves, pause, lock/exit and old UP events never charge/reveal twice.
- Six scripts/default AMOLED output and Kecleon behavior remain unchanged.

## Task1: Native tutor UI and service lifecycle
Files: new src/ui/round_touch/MarketTutorUi.h; src/scenes/ExploreScene.h/.cpp;
src/ui/common/MarketTutorPerformance.h; tools/sdgoods_market_host.cpp.
- [x] Actual Gengar handoff assertion RED on first batch.
- [x] Implement native page geometry/wrapping and optional performance transform.
- [x] Wire service identity, durable period/gift and atomic paid actions.
- [x] Full engine journeys and6styles, clip/swap, failed/slow save/lock GREEN.
- [x] Kecleon/AMOLED tutor regressions, save/architecture, two builds, actual
      screenshots and one fresh final read-only review. Leave uncommitted.

Verified2026-11-16: staged-resource ASan/UBSan actual round Gengar+Kecleon
journeys; full snapshot readback for tutor/gift/inventory/moves/proficiency;
full gift stack, init failure, failed/partial-history/slow paid saves,
scroll/brake, paused training, lock during paid performance, stale emitted
requests and clipped/swapped rendering. Six scripts60samples exactly match
pre-change AMOLED pixels and fit round stage. Shared AMOLED tutor rules,
navigation and full application6style journeys pass.27 save/architecture
checks pass (2pausedOSTBskips). SDGOODS Debug0x17c880 and V2 0x1fc7a0 builds
pass. Same-output embedded→external cache variant check passes after compiler
command hashing. First-batch source preserved. No commit/push/flash.

Final independent review: no Critical/Important findings. Deferred minor:
emitted requests are validated, but displayed quotes are not frozen when
selecting member/replacement; current flow has no concurrent party/move edits.
Physical touch/cadence/audio remain untested. Evidence and actual captures:
`.stick-local/artifacts/sdgoods-market-tutor-20261116/`.
