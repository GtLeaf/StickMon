# Three Learning Animations Integration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Integrate approved remote/melee/self learning previews without changing existing training animations.
**Architecture:** Reuse the move presentation classifier; translate training categories into separate learning styles. Pure renderer receives style/time/actual partner, Ui retains save/skip/pause/result lifecycle.
**Tech Stack:** C++17, Canvas565 and real native AmoledApp/UI replays.
**Spec:** User approved learning previews under .stick-local/artifacts/gengar-learning-types-20261113/ and said 写入.

## Constraints and Review Focus
- Learning5600ms: teacher demo, first attempt, correction, learner success and approval. Teacher purple, learner cyan.
- Same mapping as training; all STATUS including enemy control use self, ranged exceptions402/479 carry over.
- Training4800ms renders remain pixel-identical. No prices/schema/assets/gameplay changes.
- Save before performance; failed saves roll back,400ms anti-skip, canceled gestures, lock/standby pause and result exit retained.
- Actors alternate and face inward; no lower caption. Check larger partner head/halo bounds.

## Task
- [x] Add publicUi learning replay using actual eligible remote/melee/self/enemy-status lessons; assert teacher movement for melee and absent target for self, capture complete56frame timelines. Observe old generic rendering fail.
- [x] Port approved three learning scripts in renderer-only namespace, add category selector, wire chosen lesson in Ui.
- [x] Run full tutor rules/App/UI suite and architecture/family/related-stall checks; compare exports with approved previews and prior training frames.
- [x] Review increment, inspect images, update spec/memory/ledger. Preserve existing feature branch and earlier uncommitted work; no commit/push or device access.
