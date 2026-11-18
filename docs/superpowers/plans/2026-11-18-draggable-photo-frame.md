# Draggable Photo Frame Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan in the current session. User authorized execution of the preceding design.

**Goal:** Make the256px camera square draggable and render real centered/off-center examples.

**Architecture:** PhotoSession owns crop coordinates and gesture state; PhotoScreens and capture read the same geometry. Reuse66ms update to coalesce MOVE renders.

**Tech Stack:** Existing C++17 AMOLED application, Canvas565, native Python/C++ test harness.

**Spec:** docs/superpowers/specs/2026-11-18-draggable-photo-frame-design.md

## Global Constraints

- Preserve current uncommitted photo work and5-photo capacity; leave changes uncommitted.
- Save192×192 RGB565; no world camera pan, storage/schema/resource change or extra copy buffer.
- No V1/OSTB build, device access or flash.

## Review Focus

- Gesture begins on crop then ends on shutter/back: never activate controls.
- CANCEL/out-of-bounds then stale MOVE/UP: never resume dragging or capture.
- Clamped crop ends at exact pixel367/343: no read or corner drawing beyond viewport.
- Multiple MOVE events before66ms: only latest crop rendered; UP renders final position.
- Retake preserves position; reopening camera centers; saved readback matches moved clean world.

### Task1: Crop geometry, drag and real render

**Files:** Modify PhotoSession.h/.cpp, ui/PhotoScreens.cpp, PhotoApplication.cpp, AmoledApp.cpp; extend tools/photo_amoled_host.cpp.

**Interfaces:** PhotoSession exposes FRAME_SIZE, MAP_WIDTH, MAP_HEIGHT and frameX()/frameY(); existing touch/update/capture signatures remain.

- [x] Add actual-App behavior tests and show RED on the expected256px frame geometry.
- [x] Implement bounded gesture state and shared crop; preserve header priority and consume the complete crop gesture.
- [x] Implement frame mask/corners and66ms map dirty-row delivery.
- [x] Run native complete photo journey; test centered/moved pixels, save/reload, retake/reset, boundaries/cancel/controls and render bands.
- [x] Run relevant exploration/lock/render/architecture regressions and AMOLED V2 SDK build.
- [x] Review delta separately, inspect real rendered images, document evidence in AGENTS and inline preview.

No commit step: the user requested implementation/rendering only.
