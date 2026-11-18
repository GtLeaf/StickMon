# SDGOODS Photography and Album Implementation Plan

> **For agentic workers:** Use focused parallel agents for independent storage and shared-flow domains, then integrate scenes and verify the whole feature. Follow test-first for behavior. User approved execution.

**Goal:** Port photography and five-photo computer album to SDGOODS and show actual renders.
**Architecture:** PhotoLibrary codec stays common. Geometry-driven Ui::PhotoFlow shared by AMOLED and round UI; SDGOODS userdata provides IUserFiles; round scene adapters own camera and album lifecycle.
**Tech Stack:** C++17, existing Scene/UI, LittleFS/NVS, native hosts, ESP-IDF.
**Spec:** docs/superpowers/specs/2026-11-18-sdgoods-photo-album-design.md

## Global Constraints

- Preserve a183f91 functionality; no commits/device actions; no V1/OSTB builds.
-360 round,224 movable crop,192 saved,48 thumbnail,5 capacity; no partition/schema/resource removal.
- Keep wildlife live while freezing route; preview parked; accurate crop; retain archive on reset.

## Review Focus

- First userdata mount: blank proof before format; damaged/read-failed media never formatted.
- All224 crop corners and controls visible; drag/CANCEL cannot activate shutter/back.
- Shared flow extraction must retain all existing AMOLED touch/render expectations.
- Long failed save/preview cannot advance route/encounters; wildlife remains live in viewfinder.
- Scene exit/lock/reset must free draft, cancel touch and retain stored photos/IDs.

### Task1: SDGOODS userdata adapter (independent)

Files: new firmware/sdgoods/main/PhotoFiles.h/.cpp, platform bindings and lookupBlob, CMake. Interfaces: IUserFiles at services.userFiles, accurate IBlobStore::lookupBlob. Tests mocked ESP lifecycle and real photos. RED -> implement -> GREEN. No UI/common-flow edits.

### Task2: Shared geometry-driven flow (independent)

Files: src/presentation/PhotoFlow.h/.cpp, AMOLED PhotoSession compatibility alias and manifests. Interfaces: PhotoFlow API/rectangles/geometry as exact spec. Add round crop/input tests RED -> GREEN; keep existing AMOLED suite green. No SDGOODS platform or scene edits.

### Task3: Round UI and scene integration

Files: round PhotoUi, ExploreScene and ExploreUi clean-world mode/entry, MenuScene/ComputerHost/round ComputerUi. Tests actual round scene journey RED -> GREEN. Preserve ordinary menu and other device families. UI paints all controls from shared bounds.

### Task4: Verification and renders

- [x] Verify independent task tests, actual round journey, old AMOLED photo workflow, storage/library/lock/render/architecture regressions.
- [x] Build SDGOODS and AMOLED V2 only; no device operations.
- [x] Independent final review, fix important findings with failing tests first.
- [x] Inspect native round screenshots, provide inline preview, update AGENTS and evidence.
