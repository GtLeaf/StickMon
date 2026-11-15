# Shared TextBlock Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline. Preserve the
> user's uncommitted work; do not commit, push or flash.

**Goal:** Shared bounded wrapping text and adoption in Gengar confirmation pages.
**Architecture:** Header-only owning layout component and shared bitmap metrics;
explicit PixelRenderer font adapter; cached confirmation body in MarketTutorUi.
**Tech Stack:** C++17, existing bitmap fonts, Canvas565, native host tests.
**Spec:** specs/text-block.md

## Global Constraints

- Existing palette/fonts and 32px main confirmation text remain authoritative.
- No automatic font reduction, periodic redraw, heap allocation or new resources.
- Existing training list/coin changes and unrelated workspace changes survive.
- No commit, push, merge, hardware build or flash in this task.

## Review Focus

- Invalid/truncated UTF-8 and a container narrower than one glyph must terminate.
- Closing punctuation cannot occupy a line alone when a legal earlier break fits.
- Explicit/trailing newlines, empty text and reused components keep correct height.
- Fixed-height overflow must not conceal confirmation consequences or block actions.
- Full, dirty-row and byte-swapped drawing must agree and restore canvas state.

## Task 1: Shared layout and bitmap adapter

Files: src/presentation/BitmapTextMetrics.h, TextBlock.h, PixelRenderer.h/.cpp;
tools/text_block_host.cpp, text_block_render_host.cpp, test_text_block.py.

- [x] Write public behavior tests for the Review Focus, ASCII/CJK metrics, word
      wrapping, alignment, clipping/ellipsis and unchanged-input cache reuse.
- [x] Run tests and record the missing-component failure before implementation.
- [x] Implement TextBlock owning text, layout records, measure(), draw(); options
      include font, width, height, maxLines, lineGap, align, wrap, overflow.
- [x] Implement PixelRenderer::bitmapText with explicit 16/32px fonts, logical
      coordinates, shared advance metrics, existing resources, preserved state.
- [x] Run native sanitizer/layout and real-resource rendering tests.

## Task 2: Gengar adoption and documentation

Files: src/ui/common/MarketTutorUi.h, tools/market_tutor_amoled_host.cpp,
specs/market-stalls/gengar-ui.md, doc/字体与字号现状.md.

- [x] Add confirmation journey coverage for long names, stable 32px copy and
      dirty-row reconstruction; retain current transactional/navigation tests.
- [x] Replace confirmation-only wrapping/shrink logic with cached TextBlock and
      natural joined sentences; retain legacy helpers for untouched result pages.
- [x] Use existing body scroll helper only if measured text exceeds available room.
- [x] Run TextBlock, tutor, architecture, native font/render checks appropriate to
      these files; capture and inspect actual confirmation screens.
- [x] Update component documentation and verification ledger, review final diff.

Execution already authorized by the user's “执行”; no additional approval gate.

## Task 3: Approved market text adoption

User approved: Kecleon item descriptions and purchase results (including box
reward text), Gengar results, and both static stall greetings. NPC typewriter
dialogues and Chansey are excluded. No font/assets, palette, timing or rules changes.

- [x] Extend real-resource public-UI tests for greeting punctuation, item bodies,
      ordinary/box rewards, Gengar outcomes, clipped bands and pixel byte order.
- [x] Observe the old greeting's punctuation layout fail the new render check.
- [x] Reuse one cached primary TextBlock per stall, preserving supporting labels,
      fixed body areas, deliberate training-result newline, and fade colors.
- [x] Audit canonical item/reward/greeting and species/move result strings against
      their reserved heights; retain confirmation-only scroll behavior.
- [x] Run component, tutor, shop, box-motion and architecture checks; inspect
      saved renders and obtain a scoped read-only review before completion.

## Task 4: NPC typewriter composition

Approved: keep MarketDialogue as the playback controller, reuse cached full-text
layout, extend generic drawing with a line window and visible-character budget.
40ms/character, two lines/page, click-to-complete/advance, opening/arrow behavior,
pause/lock handling, existing fonts/colors and unrelated work remain unchanged.

- [x] Write tests for fixed full-line alignment at partial reveal, UTF-8 budgets,
      line windows, dirty rows and generic backwards compatibility; observe RED.
- [x] Extract pure TextLayout from TextBlock without changing wrapping/metrics.
      Add DrawRange and static drawLayout over an existing layout; preserve the
      old draw API and make reveal independent of paint clipping.
- [x] Have MarketDialogue own one layout, advance fixed two-line windows and
      track revealed glyphs while keeping visibleText compatibility. Continue
      long layout chunks from source boundaries if the bounded cache fills.
- [x] Have MarketDialogueUi draw the fixed page window and reveal budget;
      retain frame, camera, cursor and hit geometry.
- [x] Test 16/32px playback, timing, page/utterance traversal, pause, UTF-8,
      resource failures, full/dirty/swapped rendering and actual App locking.
- [x] Run prior component/market regressions and architecture checks; perform a
      fresh read-only scoped review and leave verification under .stick-local.

## Task 5: Bag and ordinary-shop detail descriptions

Approved: descriptions only; preserve existing 32px font, palette, header/icon/
price/owned positions and action/back behavior. Cover home/battle bag and buy/sell.

- [x] Write native long-copy, canonical-fit, dirty-band, swapped-buffer and toast
      tests; capture baseline screens and observe the old one-line render fail.
- [x] Reuse one keyed TextBlock cache in ItemShopScreens.cpp, x24/y248/320x100
      above unchanged y354 buttons. Keep bag toast feedback above wrapped copy.
- [x] Compare outside-body pixels against baseline; run controller/shop rules
      and rendering checks, inspect saved screens and obtain scoped review.
- [x] Update docs and evidence; preserve unrelated work, no commits or flashing.

Evidence: .stick-local/artifacts/item-detail-text-20261115/verification.json and
comparison.png. All 48 items fit in four modes; six baseline frames match outside
the description band. Independent scoped review passed without findings.

## Task 6: Skill detail and skill-management descriptions

Files: firmware/amoled_1_8_v1/main/ui/TeamScreens.cpp;
tools/move_detail_text_host.cpp, tools/test_move_detail_text.py;
tools/test_move_management.py, tools/test_amoled_team_layout.py;
specs/text-block.md and doc/字体与字号现状.md.
Uses existing TextBlock and the shared drawMoveDetailContent entry point;
ProgressionScreens reuses this entry without a separate implementation.

- [x] Add native punctuation/ASCII/empty descriptions, canonical fit, expansion/
      fade/scroll, cache switching, dirty-row and swapped-buffer tests; save
      baseline screens and observe old hand-written wrapping fail.
- [x] Replace private byte wrapping with one cached TextBlock at x24/base+64,
      width320/height192/font32/gap8. Preserve power/accuracy and animation;
      intersect expanded panel drawing with original dirty rows if necessary.
- [x] Run native render checks, team layout/navigation and progression replacement
      journeys, plus shared move rule tests. Inspect before/after actual renders
      and compare pixels outside description areas.
- [x] Update docs/evidence and obtain scoped read-only review; retain earlier
      uncommitted work, no commit/push/merge/firmware build/flash.

Evidence: .stick-local/artifacts/move-detail-text-20261115/verification.json.
All271 descriptions fit (maximum4 lines); native full/dirty/swapped tests and
20 team/progression regressions pass. Corrected playable-move before/after frames
retain identical styling. Independent scoped review approved without blockers.

## Task 7: Growth message adoption

Files: firmware/amoled_1_8_v1/main/ui/ProgressionScreens.cpp;
tools/growth_text_host.cpp, tools/test_growth_text.py;
specs/text-block.md and doc/字体与字号现状.md.
Uses existing TextBlock only; ProgressionFlow/timeline/save rules are unchanged.

- [x] Add native canonical species/level/move-name fit, explicit32px under ambient
      nativeText=false, semantic line breaks, all evolution phases, cancel hold,
      dirty rows/swapped buffers and cache switching checks. Capture baseline
      frames; observe the old renderer fail explicit-font message rendering.
- [x] Replace level-up manual drawing, centered evolution message/instruction
      drawing and new-move name drawing with bounded cached TextBlock instances;
      preserve short text positions and authored level-up semantic breaks.
- [x] Run native render checks, progression skip/replace and evolution hold/
      cancel/commit App journeys, shared timeline and architecture checks.
      Inspect real renders and compare all unaffected pixels to baseline.
- [x] Update documentation/evidence and obtain scoped read-only review. Retain
      all prior uncommitted work; no commit/push/firmware build/flash.

Evidence: .stick-local/artifacts/growth-text-20261115/verification.json.
64species x5 levels/x3 evolution messages and271move names fit. Literal authored
level-up rows, nine evolution phases, explicit fonts, dirty/swap drawing and
7 growth/timeline/App checks pass; four normal native frames are pixel-identical.
Independent scoped review approved without actionable findings.
