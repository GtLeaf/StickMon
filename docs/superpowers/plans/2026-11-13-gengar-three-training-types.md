# Three Training Animations Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Integrate approved remote, melee and self training prototypes; keep learning imitation unchanged.
**Architecture:** Presentation classification maps existing MoveInfo to three renderer styles. App transactions and Ui performance lifecycle remain unchanged. Prototype motion is ported into the existing MarketTutorPerformance renderer with selected partner species.
**Tech Stack:** C++17, native AmoledApp, Canvas565.
**Spec:** User approved three GIF prototypes in .stick-local/artifacts/gengar-three-types-20261113/ by saying 这三个写入; specs/market-stalls/gengar-ui.md will record final behavior.

## Constraints and Review Focus
- Every STATUS move uses SELF, including effects applied to enemies.
- Damaging contact moves use MELEE; noncontact use REMOTE. Seed Bomb402 and Smack Down479 are explicit remote presentation overrides because current contact flags misdescribe their visual range.
- Learning uses the existing5600ms imitation. All training uses4800ms with400ms anti-skip, save-before-show, pause/cancel and result exit unchanged.
- Neutral cyan training effects and blank footer match approved prototypes. No resource/schema/battle-rule changes.
- Test that real Ui selects three different animations by move, not just direct renderer calls; include enemy-targeted status.

## Task
- [x] Add public Ui replay for electric ball, quick attack, self buff and enemy status; assert distinct positional/target behavior and capture full timelines. Run against old renderer and observe melee staying at origin.
- [x] Add REMOTE/MELEE/SELF styles and small presentation classifier; port all three approved timelines, preserving imitation.
- [x] Run full market tutor rules/App/UI regression, architecture/family and related stall check; inspect real Ui exports and multiple body sizes.
- [x] Incremental independent review, update spec/memory and evidence ledger; no commit/push or device writes.
