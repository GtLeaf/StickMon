# Gengar Facing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Make training actors face one another and remove the bottom move label.
**Architecture:** Use PokemonSprites walkingAnimation directional metadata and flipX, just as the market does; draw frames with a shared ground anchor. Keep the current performance/save/input lifecycle.
**Tech Stack:** C++17 production UI and native application renderer.
**Spec:** User approved the preceding facing/removal proposal by saying 执行; existing specs/market-stalls/gengar-ui.md describes retained behavior.

## Constraints and Review Focus
- Left Gengar faces RIGHT; partner faces LEFT throughout frame changes and hops.
- Align feet using frameGroundOffsetY; fit frames within 80px cells and panel208..444.
- No bottom move-name text. Existing 2400ms/400ms timing, save and pause behavior stays intact.
- Use existing resource frames; no asset regeneration or gameplay changes.
- Extend native pixel replay for Pikachu, Eevee, Snorlax and Gyarados, multiple timestamps, both actor directions and clear footer.

## Task
- [x] Add directional sprite/ground-anchor and blank-footer pixel assertions to tools/market_tutor_amoled_host.cpp; run and observe old icon render failure.
- [x] Replace only training actor draw calls in src/ui/common/MarketTutorUi.h with directional frames, remove move label.
- [x] Run ASAN_OPTIONS=detect_leaks=0 python3 tools/test_market_tutor.py with output .stick-local/artifacts/gengar-facing-20261113/frames; run architecture/family checks.
- [x] Export and inspect real-render GIFs/contact sheet for the four species, review diff, update spec/memory.

Execution stays in existing feature branch with prior uncommitted user-authorized changes preserved. Artifacts and ledger follow AGENTS.md .stick-local convention. No commit/push/device write.
