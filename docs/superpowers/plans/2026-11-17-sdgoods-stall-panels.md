# SDGOODS Stall Panels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Apply the approved Kecleon and Gengar previews to the native round-screen stalls.

**Architecture:** A round-only presentation helper owns the panel, dialogue, bitmap text, rounded actions and centered balance. The existing controllers keep their transaction, persistence, animation and scroll state; rendering and hit testing share the new native rectangles.

**Tech Stack:** C++17, Canvas565, TextBlock, native SDGOODS engine host.

**Spec:** Approved previews in `.stick-local/artifacts/sdgoods-kecleon-pages-preview-20261117/`, `sdgoods-gengar-pages-preview-20261117/` and `sdgoods-gengar-centered-preview-20261117/`; the conversation is the approval source.

## Global Constraints

- Panel x48/y132/w264/h236, continuing below the physical y360 edge.
- Dialogue x0/y220/w360/h180; original 32px bitmap text with line widths256/256/192.
- Balance centered at y16; remove stall-name badges. Preserve shared coin motion.
- Lists continue to y360; outside-panel taps return without triggering list items.
- All panel actions are rounded rectangles. Do not display the coin-shortfall sentence.
- Center learning confirmation vertically between the header and action; retain disabled actions, other errors and original micro copy.
- No save/rule/resource changes. No V1 or OSTB build, device access, commit or push.
- Preserve unrelated adaptive standby changes already in this worktree.

## Review Focus

- Long names and paragraphs must wrap completely without colliding with stock, errors or actions.
- Circular lower edges must not make the final list entry unreachable.
- New panel side margins are outside-return areas; inertial braking cannot select a row.
- Failed/slow saves and repeated UP events must retain once-only transactions.
- Glyph scaling, clipping and swapped RGB565 must agree with full rendering without full-screen temporary allocations.

### Task 1: Shared presentation and regression gate

**Files:** Create `src/ui/round_touch/MarketStallUi.h`; create `tools/sdgoods_market_layout_tests.h`; modify `tools/sdgoods_market_host.cpp`.

**Interfaces:** `Ui::RoundMarketStall` provides `Rect`, panel/dialogue geometry, bitmap drawing, fitted TextBlock drawing, rounded buttons, centered coin HUD and conversation framing. It contains no game mutation.

- [x] Add failing native checks for narrow panels, centered confirmation, rounded actions and no shortfall text; run and record RED.
- [x] Implement bounded per-glyph drawing and shared geometry; no framebuffer-sized scratch allocation.

### Task 2: Apply the two controllers

**Files:** Modify `src/ui/round_touch/MarketShopUi.h`, `MarketTutorUi.h`; update native journey geometry expectations in `tools/sdgoods_market_host.cpp`.

**Interfaces:** Keep public controller/request APIs; consume shared rectangles for both rendering and touch.

- [x] Apply Kecleon menu, shelf, advice, detail, result, wide dialogue and blind-box stage.
- [x] Apply Gengar member, move, recall, confirmation, performance and result pages.
- [x] Keep confirmations scrollable if a future paragraph exceeds the available region; center fitted content otherwise.
- [x] Update native journey scroll expectations for the approved row geometry; retain transaction/lock/pause assertions.
- [x] Run native regression and journeys, inspect actual screenshots and fix layout failures.

### Task 3: Integration verification and review

**Files:** Update `AGENTS.md` with verified outcomes only; retain evidence under `.stick-local/artifacts/sdgoods-stall-panels-20261117/`.

- [x] Run embedded Debug native journeys with sanitizers, Release access checks, shared market rules and architecture checks appropriate to the change.
- [x] Build SDGOODS firmware when the existing SDK is usable; never flash.
- [x] Review the scoped diff for clipping, touch ownership, camera framing, allocations and shared-target impact.
- [x] Report verified results and remaining device-only limitations; leave changes uncommitted.

## Execution Notes

- The user already authorized implementation of the reviewed previews; proceed directly without another approval gate.
- Existing unrelated work stays in place. Tests and review use the scoped diff, not a branch-wide claim about adaptive standby.

## Verification Ledger

- Task 1/2: initial native layout assertion failed at panelTop132 before implementation; final native journeys and pixel/gesture contracts pass.
- Ruling: retain this dirty worktree and leave all work uncommitted, because the user requested implementation and unrelated adaptive standby work is already present.
- Ruling: use bounded per-glyph32px bitmap sampling rather than the prototype's full-screen scratch canvas; same font family, no full-screen allocation.
- Ruling: frame real conversation actors without changing their world positions; constrain horizontal camera to authored map boundaries to avoid blank borders.
- Final review: independent read-only review of scoped files; unrelated adaptive standby explicitly excluded.
- Final: fixed training save error overwriting move rows; exact row-pixel check RED in review-red.log, GREEN in final-native.log/final-sanitized.log.
- Final: fixed clipped stale-period error; complete glyph-ink check RED in review-stale-red.log, GREEN in final-native.log/final-sanitized.log.
- Final: fixed clipped advice error at circular bottom; physical circle check RED in review-advice-red.log, GREEN in final-native.log/final-sanitized.log.
- Final: fixed unbounded horizontal framing; actual market camera check RED in review-camera-red.log, GREEN in final-native.log/final-sanitized.log.
- Final: disabled purchases cannot add an unwanted coin hint; disabled-red.log RED, final native/sanitized GREEN.
- Final: SDGOODS firmware build succeeds; no device access. Shared market rules,2880 cached/uncached map comparisons, text layout, architecture and family checks pass.
- No deferred review findings within scope. Physical touch feel and frame time remain device-only verification.
