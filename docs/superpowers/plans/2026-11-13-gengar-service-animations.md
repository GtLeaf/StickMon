# Gengar Service Animation Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Integrate selected prototype2 for learning and prototype1 for training, without per-move animation types yet.
**Architecture:** Keep all save/transaction ownership in App. MarketTutorUi selects a saved learning or training performance and controls pause/skip/completion; a focused shared renderer draws the two selected prototype timelines using existing directional sprites.
**Tech Stack:** C++17 native Canvas565 / AMOLED V1/V2 shared UI.
**Spec:** User selected "学习技能用示范模仿，特训用练靶", explicitly deferred per-type styles. Existing specs/market-stalls/gengar-ui.md records prior preserved behavior.

## Constraints and Review Focus
- Lesson save success -> 5600ms demonstration/imitation -> learned result. Save failure stays confirmation with no performance or charge.
- Training save success -> 4800ms target practice -> proficiency result. Failed save remains selection and rolls back.
- Existing400ms anti-skip, completed tap skip, lock/standby pause and direct result exit remain.
- Recall continues directly to result; no new recall animation.
- Left Gengar RIGHT and selected partner LEFT, blank lower panel, existing resources only. Same generic visual effects for all moves this round.
- No new schema/price/reward rules; no replayed payment during animation.

## Task
- [x] Extend real App journey to require lesson performance, saved-state visibility during performance, lock/standby pause, full/skip completions, and full4800ms training; observe old behavior fail.
- [x] Add src/ui/common/MarketTutorPerformance.h as renderer only, port prototype1/2 with variable partner species; integrate Ui state and label in MarketTutorUi.h and UiStrings.h.
- [x] Adapt native frame coverage to new positions/timelines; verify four species over full training duration, directional frames and blank footer.
- [x] Run complete market tutor rules/App/public UI suite, architecture and family checks; export actual App teaching/training GIFs.
- [x] Independent incremental review, inspect output, update spec/memory and ledger. Keep feature branch and existing uncommitted work; no push/device access.
