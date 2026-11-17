# Home Preview Comparison Implementation Plan

> Use superpowers:executing-plans inline. The user explicitly authorized execution; leave changes uncommitted.

**Goal:** Compare fixed-slot furniture with a single `摆放`/`撤下` toggle, save on preview Back, and prevent automatic sleep during preview.

**Spec:** Latest approved conversation. Retain black theme and original right-wall placement/camera. Preview contains the full room, existing header/back and one bottom toggle. Remove the icon/name/normal status from preview only; library rows retain them. Failed saves may show an error.

**Architecture:** Shared controller keeps entry placement and a local draft boolean. Toggle never mutates GameState; Back returns a save request only if the current draft differs. Native and AMOLED use their existing durable transaction. Scene-level keep-awake query suppresses native idle; AMOLED idle eligibility consults the same controller query. Manual power actions and other pages retain existing behavior.

## Constraints and Review Focus

- Unplaced/new item starts with original room and `摆放`; toggling shows candidate/`撤下`; withdrawing restores original slot content. Already placed item starts visible/`撤下`, can preview removal or put it back.
- Back with unchanged draft returns without saving. Changed draft saves then returns. Failed save retains draft and page for retry. Other automatic saves, cancel, power transitions and abnormal exits never publish a draft.
- Preview is kept awake without changing the saved timeout setting or spinning/redrawing a parked scene. Returning to library restores normal timeout behavior.
- Preserve unknown save bits, ownership, existing library scroll/filter and touch cancellation; stale UP and repeats cannot double-toggle or double-save.
- No theme, anchor, camera, schema or frame-rate changes. No V1/OSTB builds, device access, flash, commit/push/merge.

### Task1 — Shared comparison state and Back transaction

- [x] Update actual native journey to fail on immediate save after toggle; record RED.
- [x] Update HomeLibraryUi entry snapshot, local toggle, Back request/no-op return, failure/retry and lean preview rendering.
- [x] Route physical and touch Back through the same native save boundary; update native+AMOLED journeys for compare/revert/save/no-op/failed save and stale input.

### Task2 — Preview keep-awake policy

- [x] Add native Scene query delegated by MenuScene/RoomUi and checked by GameEngine idle policy; no effect on other pages.
- [x] Gate AMOLED automatic lock while preview is active; keep manual power behavior.
- [x] Test timeout exceeding30s on preview, normal timeout after Back, and original state seen by intermediate saves.

### Task3 — Verification

- [x] Add only missing glyphs and preserve existing font bytes.
- [x] Run both real application journeys, renderer band/swap and source/save/idle regressions, native ASan/UBSan, SDGOODS/V2 SDK builds.
- [x] Inspect actual black preview screenshots; one fresh-context scoped review and fix important findings with RED→GREEN.
- [x] Record verified behavior and limits; keep all prior work intact and uncommitted.
