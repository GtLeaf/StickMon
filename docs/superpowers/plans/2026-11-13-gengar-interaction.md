# Gengar Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Implement the user-approved centered lesson/results, direct exits, training selection/performance and reliable recall dragging.
**Architecture:** Keep gameplay/save transactions in AmoledApp and MarketTutorService; MarketTutorUi owns selection, presentation and consumable touch state. Both AMOLED drivers must read held contacts instead of inferring release from missing interrupts.
**Tech Stack:** C++17, native AmoledApp host, Python regression runners, ESP-IDF 5.5 / PlatformIO where installed.
**Spec:** User-approved proposal in this conversation; specs/market-stalls/gengar-ui.md updated with the final behavior.

## Global Constraints
- Native 368x448; panel top 208 and existing camera geometry stay fixed.
- Training costs retain 40/80 boundaries; outcome ranks use existing 60/90 boundaries.
- Save before performance, rollback failures; no save schema change.
- All retained local evidence belongs in .stick-local/artifacts/gengar-update-20261113/.

## Review Focus
- Missing interrupts while a contact remains held must never activate a row.
- Payment release must not skip performance; skip after 400ms cannot repeat payment.
- Locked/standby time must not advance performance.
- All three results exit the stall; no input may leak into market walking.
- Long text stays centered and clipped to content; unavailable selections cannot charge.

### Task 1: Driver release correctness
Files: firmware/amoled_1_8_v{1,2}/main/TouchInput.cpp, tools/amoled_v{1,2}_touch_host.cpp.
- [x] Extend real-driver replays: no IRQ + held point at 120ms stays down; changed held point emits MOVE; failed reads retain gesture; actual empty sample emits one UP.
- [x] Run both Python touch tests and observe the old premature-release failure.
- [x] Poll held contacts through the existing bounded cadence; remove elapsed-time release.
- [x] Run both driver regressions.

### Task 2: Stall UI and journey
Files: src/ui/common/MarketTutorUi.h, src/core/UiStrings.h, tools/market_tutor_amoled_host.cpp.
- [x] Update journey to demand direct result exits, selected training rows, dynamic costs, failure rollback, a 2400ms saved performance, lock pause and skip gating; observe old-flow failures.
- [x] Center lesson confirmation/result text and switch result action to animated stall exit.
- [x] Select training without navigation, default first valid slot, pay via footer and show saved performance with two existing monster sprites.
- [x] Format rank-crossing and same-rank results without numerical footer.
- [x] Verify fresh driver sequence plus slow/fast/edge/cancel recall drags and last-item save through production code.
- [x] Run ASAN_OPTIONS=detect_leaks=0 python3 tools/test_market_tutor.py --output .stick-local/artifacts/gengar-update-20261113/frames.

### Task 3: Verification and visual evidence
- [x] Run architecture/UI-family, market care/shop and affected save regressions.
- [x] Attempt StickS3 and AMOLED V1/V2 builds; record exact missing prerequisites if unavailable. Attempts blocked: no ESP-IDF or PlatformIO installed; no firmware-build success claimed.
- [x] Export PNG flow sheets and animated training preview using real renderer frames, inspect images.
- [x] Update spec and AGENTS memory, review full diff, check no unrelated changes.

Execution notes: work in the task's fresh cloud checkout on a feature branch. User already approved the proposal and instructed execution; no further design gate. No automatic commits/push/deployment. Ledger and temporary artifacts follow repository .stick-local convention.
