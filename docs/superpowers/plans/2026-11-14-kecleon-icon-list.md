# Kecleon Icon List Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement inline, preserving the existing uncommitted market work.

**Goal:** Apply the user-approved native icon-list preview to the two Kecleon menus.

**Architecture:** Keep rendering and hit testing in MarketShopUi. Use the same row rectangles for both, procedural pixel bag/clipboard icons and existing item resources. Transactions and advice persistence remain unchanged.

**Tech Stack:** C++17, Canvas565, existing native UI/App hosts.

**Spec:** User-approved preview `.stick-local/artifacts/kecleon-icon-list-preview-20261114/`, and `specs/market-stalls/kecleon-ui.md`.

## Global Constraints

- Preserve panel x4/y208/w360/h236/r14, stall badge, 32px labels, frame limiter, existing pending/gesture guards.
- Home rows: y246/326, 80px. Advice: y230/294/358, 64px. Icons centered x48, labels x88.
- Remove advice selection-page Back drawing and hit region. Outside returns home. Advice acknowledgement retains its existing Home button.
- Selected advice uses preview pale-green fill and check; save error moves below rows to y426.
- No commit, push, firmware build or flashing requested.

## Review Focus

Former Back zone overlaps the last new row; lower blank must not return or submit. Pending gestures must not submit twice. Save errors must fit below the third row. Row rendering must follow panel motion. Prior box/coin changes must survive.

### Task 1: Apply icon menus and verify navigation

**Files:** `src/ui/common/MarketShopUi.h`; `tools/market_procurement_ui_host.cpp`; `tools/market_procurement_amoled_host.cpp`; `specs/market-stalls/kecleon-ui.md`; `AGENTS.md`.

- [x] Extend existing controller coverage for new row coverage, former Back overlap, lower blank and outside return; observed old home row fail at the new visible lower portion.
- [x] Implement approved pixel rendering, shared hit rectangles, removed Back target, and unobstructed failure text. Update old fixture coordinates.
- [x] Seasonal rules/controller/production App, sound/coin regressions, scrolling and architecture pass. Actual App home/advice/selected panel pixels match approved preview; inspected failure text.
- [x] Independent review found no issues in this task's delta. Evidence: `.stick-local/artifacts/kecleon-icon-list-live-20261114/verification.md`. Changes remain uncommitted.
