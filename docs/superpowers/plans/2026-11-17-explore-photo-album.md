# Explore Photo Album Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline; user authorized execution. No intermediate commits or implementation permission prompts. One fresh scoped final reviewer.

**Goal:** Complete exploration photography, independent durable photo files and computer album on AMOLED.

**Architecture:** Optional IUserFiles platform service, scoped POSIX publish-new files on V2 FAT, portable PhotoLibrary codec/catalogue, AMOLED PhotoSession and PhotoScreens. Existing route renderer gains a clean-world mode shared by viewfinder/capture, keeping its default HUD unchanged.

**Tech Stack:** C++17, existing RGB565 canvas/bitmap fonts, ESP-IDF FAT/WL, native host Python runners.

**Spec:** docs/superpowers/specs/2026-11-17-explore-photo-album-design.md

## Global Constraints

- UI first on AMOLED, build V2 only; no V1/OSTB builds or device access. Shared services permit future SDGOODS UI.
- Photo192x192, thumbnail48x48, capacity5; source square x24/y12/320x320 inside the actual map. No schema/partition changes; no image NVS blobs or resource writes.
- Preserve existing104px bottom bar/menu/HP/battle bag. Black theme/minimal text, keep preview awake, retain66ms animation limit and dirty clipping.
- No automatic overwrite/format of nonblank storage; old photos survive write failures and firmware resource updates. Game reset retains independent photo archive in this batch.
- Artifact/ledger directory .stick-local/artifacts/explore-photo-album-live-20261117/. New feature branch in current checkout keeps existing build caches usable; no new worktree or commits.

## Review Focus

- Interrupted or failed counter/file/rename/delete must not publish half an image, reuse IDs or destroy existing pictures.
- Claw already-mounted or corrupted/nonblank FAT must never be reformatted or double-owned.
- Camera input/lock/exit must not leak taps, resume a stale route, start encounters or capture an overlay.
- Slow file writes must preserve draft/retry and allow bounded audio refill; capacity uses actual free space.
- Shared renderer must honor byte order, row/column damage and unchanged noncamera HUD; static photo pages park.

### Task1 — Portable files, format and photo library

Files: src/platform/api/PlatformServices.h, src/platform/common/PosixUserFiles.h, src/core/PhotoLibrary.{h,cpp}, tools/photo_library_host.cpp, tools/test_photo_library.py; portable manifest registration.

Interfaces: IUserFiles begin/exists/size/read/writeNew/remove/availableBytes, optional trailing Services.userFiles; BlobLookup FOUND/MISSING/FAILED with authoritative NVS/Desktop implementations. PhotoLibrary refresh/list/save/loadImage/loadThumbnail/erase, capacity5 and stable IDs; PixelBuffer RAII through platform memory.

- [x] Write/run native missing-feature assertion RED.
- [x] Implement validated fixed slot headers, separate CRC-protected NVS next-ID, image/thumbnail payloads, publish-new and scoped safe filenames.
- [x] Test real POSIX round trip, reboot/service reconstruction, five/full/delete/reuse without ID reuse, corrupted data/header/counter, failed persistence and allocation, unknown files preserved; ASan/UBSan.

### Task2 — Device file binding

Files: firmware/common/PlayerPhotoFiles.h, firmware/amoled_1_8_v1/main/AmoledPlatform.{h,cpp}, both maintained source CMake registrations where shared; src/platform/esp32/PlayerVolume.h and src/platform/common/BlankStorageGuard.h.

Consumes: Task1 IUserFiles. Produces: optional userFiles service mounted lazily at /brain/photos, coherent with existing brainfs; no partition changes.

- [x] Pin blank-scan and publish-only behavior in native checks before implementation.
- [x] Mount FAT without format; accept already-mounted path; only retry formatting for a fully erased partition. Bind free-space query and periodic music-refill callback while writing chunks.
- [x] Preserve nonblank/corrupt storage; no dependency on enabled Claw. Test source/device wiring and build V2.

### Task3 — Capture, camera and album UI

Files: firmware/amoled_1_8_v1/main/PhotoSession.{h,cpp}, ui/PhotoScreens.{h,cpp}, ui/ExploreScreens.cpp, ui/ComputerScreens.cpp, ui/models/ScreenModels.h, AmoledApp.{h,cpp}, app and UI manifests, src/core/UiStrings.h, tools/photo_amoled_host.cpp, tools/test_photo_album.py.

Consumes: Task1 catalogue/pixels and Task2 optional backend. Produces: current-view camera overlay, saved photo previews and computer album, preserving other app flows.

- [x] Actual app RED on left shortcut still opening BAG.
- [x] Camera touch state, stale-input cancellation, clean-world render and capture sampling; viewfinder ecology only, route/encounter paused; preserve return and power behavior.
- [x] Save/retry/retake/back/full/error; computer row with optional Claw indices preserved; album thumbnails/view/swipe/delete/empty/corrupt image states.
- [x] Test actual app/menu/camera/persistence/album/delete/pause/retry/lock/idle and full-vs-band/byte-order capture. Add only missing16/32px glyphs; old glyph bytes unchanged.

### Task4 — Final verification and review

- [x] Run portable sanitizer and real AMOLED journeys; existing save/fonts/architecture/lock/render and affected exploration/computer regressions. Build V2 and affected SDGOODS only.
- [x] Inspect real app screenshots and measure native write/capture time; do not claim hardware timings.
- [x] Fresh scoped final review, fix important findings RED→GREEN, record evidence/limits and update AGENTS. Leave code uncommitted/unflashed.
