# Explore Photography and Album — First Batch

Approved by the conversation and corrected native preview on 2026-11-17. Execute now; leave changes uncommitted and do not flash.

## Scope

- First UI integration is AMOLED (V2 build, shared V1 source); SDGOODS circular UI is deferred. Keep the existing native exploration map and104px bottom bar. Replace only the backpack shortcut icon/action; menu, HP/status panel and battle bag remain unchanged.
- Current-view square photography, no pan/zoom/filter. Pause route movement and encounters while taking pictures. Ecology/idle sprite animation may continue in the viewfinder; preview pauses the photo scene. Resume the same expedition/map/camera without regenerating anything.
- Viewfinder now uses a draggable256px square wholly inside the actual344px map viewport (initial x56/y44), superseding the original320px centered frame; see `2026-11-18-draggable-photo-frame-design.md`. Its controls are excluded from capture. Bottom104px becomes the camera control bar. This corrects the earlier concept's invalid full-screen map assumption. Capture192x192 RGB565 with nearest sampling; shutter -> preview -> save/retry or retake. Back discards unsaved image. Save success returns to the route with a short toast.
- Photo pages keep awake automatically; manual power retains its existing behavior and discards unsaved drafts safely. Static previews/albums do not continuously redraw. Retain66ms route animation cadence and dirty-row/byte-order behavior.
- Computer adds Album (count/5) without breaking optional Claw entries. Two-column thumbnails, large view with swipe, one-line location/game-day metadata, delete confirmation, empty/full/error states. Existing black theme/fonts/rounded buttons, minimal text.
- Persist actual immutable image files, metadata and thumbnails outside GameState and resource packs. Default capacity5, fixed codec/version/size checks; monotonic stable photo IDs. Photo counter is a tiny separate NVS record; image pixels never enter NVS. Failed writes preserve existing pictures and retain the draft, deleted images free capacity, no automatic overwrite.
- Platform files use a scoped photo directory on existing V2 brainfs. Mount without destructive recovery; format only a positively verified fully erased partition. Cooperate with existing Claw mount ownership. No partition-table changes or automatic format on corruption. Updates must preserve photos. Independent photo archive is retained by the existing game-reset operation in this batch; photo deletion is explicit in Album.
- Room photography, binding photographs to furniture, frame viewing, sharing and SDGOODS UI are excluded.

## Approach

Store pixel snapshots, rather than scene recipes: map/assets updates must not change old photographs. Use fixed slot files with self-contained validated headers and a separately durable ID counter; publish only into an empty slot after a complete temporary write. This avoids overwriting existing files on FAT and avoids an index that could orphan every photograph. Shared photo service and platform file interface can later support SDGOODS and frame bindings.

## Validation

Real file round trips, bounds/corruption/failed writes/IDs/delete/full; actual AMOLED entry/shutter/no-HUD capture/retry/album/delete/route return and partial redraw; fonts, persistence/architecture and lock regressions; V2 and affected SDGOODS builds. No V1/OSTB builds, device access, commit/push/merge.
