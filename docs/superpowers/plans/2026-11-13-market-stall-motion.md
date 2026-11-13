# Market Stall Motion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the three AMOLED stall transitions gentler and keep shifted cameras on the static-map cache.
**Architecture:** Share the existing smoothstep curve across panel/camera/HUD transitions. Reuse the map cache and redraw only uncached edge-ground cells before actors and daylight tint.
**Tech Stack:** C++17, Canvas565, native production AmoledApp journeys and pixel comparisons.
**Spec:** specs/market-stalls/panel-motion.md. User approved the preceding proposal except refresh scheduling on 2026-11-13: keep the present frame limit, loop waits and panel transport.

## Global Constraints
- Keep greeting360ms, opening420ms, closing520ms and all destinations.
- Do not change MARKET_FRAME_MS, either firmware main loop, full-frame transport, rules, saves or assets.
- Keep the existing cache allocation; outside the map repeat ground only, including empty-edge fallback tile385.
- Preserve animation frames, actor shadows/occlusion, byte order, daylight, pause/resume and touch cancellation.
- Work on the existing feature branch; no automatic commit, push, device access or firmware flash.
- Retained evidence and execution ledger live under .stick-local/artifacts/stall-motion-fix-20261113/ per AGENTS.md.

## Review Focus
- Upward and downward camera shifts, including partial edge cells, must equal direct rendering.
- Animated or empty edge ground must use the current frame and correct fallback.
- Night tint must apply exactly once to edge cells and actors.
- AMOLED, round and small viewport cache users must retain pixel parity, including swapped pixels.
- Cache allocation failure must retain direct rendering; successful cache allocation must not grow while panning.

## Task 1: Gentle shared motion
**Files:** src/ui/common/MarketStallMotion.h; MarketShopUi.h; MarketTutorUi.h; MarketCareUi.h; tools/market_stall_motion_host.cpp.
**Interfaces:** Keep tween(int,int,uint32_t,uint32_t); panel, camera and HUD use this common smoothstep progression.
- [x] Add an actual interpolation regression: at40ms intervals a160px/420ms opening advances at most24px, stays monotone, and lands at its destination. Run before modification and observe failure.
- [x] Replace cubic easing with the existing smoothstep algorithm and consolidate transition callers. Preserve durations and pages.
- [x] Run the motion check and all three real stall journeys, including pause, close and touch checks.

## Task 2: Cache-backed shifted camera
**Files:** src/ui/common/MarketView.h; tools/market_stall_cache_host.cpp; tools/test_market_stall_motion.py.
**Interfaces:** Keep MarketView::render and cache geometry unchanged; reuse cellTile for missing visible rows.
- [x] Add a production-App/session cache probe. Compare cached/direct pixels for offsets[-240,-120,-1,0,1,94,120,240], eight animation phases, day/night, normal/swapped byte order and three viewport families. Require a cached render and unchanged allocation after warmup. Observe the old camera-window fallback fail.
- [x] Keep cached-world copying; add uncached visible rows to ground redraw and tint regions. Draw only ground beyond map bounds, followed by actors and tint. Retain allocation-failure fallbacks.
- [x] Exercise empty/animated edge ground and restore authored data afterward. Run sanitized cache/motion probes and related architecture/family checks.
- [x] Replay real20ms application updates with unchanged scheduling; compare opening displacement and cache-path counters against baseline. Export and inspect before/after frames.
- [x] Obtain one independent review, resolve material findings, and update stall specifications and project memory with verified results and hardware limits.
