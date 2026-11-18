# Concise touch communication Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to execute the approved preview inline.

**Goal:** Apply the approved concise preview to Waveshare V2 and SDGOODS.
**Architecture:** One portable VisitMenu renderer owns labels, geometry and hit testing. Each application retains ownership of its session and navigation. Expose peer MAC suffixes read-only through the existing view model.
**Tech Stack:** C++17 / Canvas565 / ESP-IDF / native C++ probes.
**Spec:** Approved preview `/workspace/generated_images/exec-0b5cfa55-ce79-4acc-aad4-d23b9b3205e4.png` and the user's request to remove explanatory copy.

## Global Constraints

- V2 368x448; SDGOODS full native round 360x360; existing bitmap fonts and no A/B touch hints.
- Keep only concise actions, room IDs and necessary Chinese error reasons. No homepage helper text.
- No V1 firmware, OSTB, device flashing, commit/push or environment publishing.
- Preserve protocol, save schema and existing active-visit lifecycle.

## Review Focus

- Four rooms must remain reachable without shrinking circular touch targets.
- Release on a changed room list must not join a different device.
- Cancel/back must preserve active-session ending behavior.
- All screen states and all labels must fit and exist in the shipped fonts.
- Adjust-team navigation must stop the radio session and retain normal back navigation.

### Task 1: Shared presentation and V2
- [x] Add failing native tests for real row gaps, cancellation and concise rendering.
- [x] Add `src/presentation/VisitMenu.{h,cpp}` with shared layout/hit testing, error copy, icons and pressed feedback.
- [x] Adapt HomeScreen wrappers and AmoledApp actions; add read-only requester identity to VisitSessionService.
- [x] Validate native touch/render, dirty-row rendering and session regressions.

### Task 2: Native round touch
- [x] Add failing real SocialUi tests: native surface, direct decisions, cancel, error action and room-list changes.
- [x] Adapt round SocialUi and SceneSurface; provide pagination for up to four rooms (two per round page).
- [x] Connect adjust-team action to existing menu and test navigation.
- [x] Inspect actual frames; validate native round and firmware builds for V2/SDGOODS where available.

## Execution ledger

- Baseline: 55cd25e, clean `work` checkout in the dedicated task cloud workspace. Work in place to preserve the user's selected checkout; no additional worktree or commits.
- User approved execution of the concrete second preview. No additional approval gate is needed.
- Protocol reliability findings remain out of scope.

- Completed: shared native renderer, all requested concise states, direct touch, room MAC suffixes, Chinese errors and normal team navigation.
- Review fixes: same-count identity redraw; displayed-model/gesture/request-sequence binding; round page-transition binding; activity-dot ticks; concise rejection/timeout feedback.
- Preserved the existing AMOLED active/ending pet detail screen; only its end button feedback/hit box is aligned.
- Verification: 10 native communication/session tests, 20 architecture/font/scheduling checks, real SDGOODS social-flow with ASan/UBSan, V2 and SDGOODS Debug firmware builds.
- Full SDGOODS room suite stops at the existing Release marketPreview assertion (line 2155 before this task). Focused social-flow passes; no unrelated market rules changed.
- No resource/glyph change, wire protocol/save schema change, flash, commit or push.

## Follow-up: continuous list and production icon assets

User explicitly requested scrolling instead of pagination and the missing preview icons.
- [x] Replace round pagination with a clipped continuous list, inertia and scrollbar; keep cancel fixed.
- [x] Preserve displayed-room identity binding while dragging/braking/list changes and cancel inertia on scene transitions.
- [x] Generate production door, pawprints, visitor+door and warning pixel assets, encode them reproducibly, and use the same assets in both renderers.
- [x] Verify drag/CANCEL/braking/last-row access/fixed controls, asset determinism, actual render images and V2/SDGOODS builds.

- Follow-up complete: continuous scrolling replaces the earlier pagination decision; four generated pixel icons are now the source of truth for both targets. Verification: social-scroll-icons-20261117/verification.md.
