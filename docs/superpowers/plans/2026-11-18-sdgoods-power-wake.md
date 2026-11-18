# SDGOODS Power and Wake Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Reliable short-press sleep/wake, standby double-tap wake and long-press shutdown despite save failures.

**Architecture:** Keep native board ownership of the panel, key and CST816. Add a small raw standby tap recognizer; extend the existing key state machine with held confirmation. Save attempts cannot veto lock or shutdown.

**Tech Stack:** C++17, ESP-IDF 5.5.4, existing ASan/UBSan host runners.

**Spec:** `docs/superpowers/specs/2026-11-18-sdgoods-power-wake-design.md` (approved in chat).

## Global Constraints

- Preserve current uncommitted social UI work; work in the existing managed `work` checkout.
- No V1/OSTB builds, flash, commit, push or publish.
- Key hold 2000ms; tap hold <=250ms; inter-tap gap <=350ms; movement radius <=24px; retry failed saves after 2000ms.

## Review Focus

- A finger already down when locking must not supply the first wake tap.
- I2C errors and invalid/multiple contacts must not turn into a release or a wake.
- Holding the power key during shutdown must not immediately wake the board or generate a short press.
- A save failure on unchanged room food state must still be retried during standby.
- Waking must consume touch events and must not activate the underlying UI.

### Task 1: Power actions and held shutdown

**Files:** `firmware/common/PowerButton.h`, `firmware/sdgoods/main/PowerActions.h`, `tools/sdgoods_hardware_host.cpp`.
**Interfaces:** key `Action::SHUTDOWN_HELD` confirms once at 2s; existing `SHUTDOWN` remains release-only. Board handles darkening on held confirmation; action coordinator locks before best-effort save and always powers off on shutdown.

- [x] Change host assertions for held confirmation, save-failure lock/shutdown and shutdown priority; run `python3 tools/test_sdgoods_hardware.py --power-only` and observe failure.
- [x] Implement key/action policy; rerun to PASS (bounce, boot-held, wraparound, recovery included).

### Task 2: Standby touch and board integration

**Files:** create `firmware/sdgoods/main/StandbyTouch.h` and `PowerInputState.h`; modify `SdgoodsPlatform.{h,cpp}`, `main.cpp`; add focused tests in `tools/sdgoods_power_wake_host.cpp` and runner `tools/test_sdgoods_power_wake.py`.
**Interfaces:** `StandbyTouch::reset()` and `sample(const uint8_t*, uint32_t)` returning true only on a completed valid double tap. `PowerInputState` owns key/lock/shutdown state and raw touch routing; native board supplies GPIO/I2C samples and applies panel transitions.

- [x] Add raw-touch gesture and input coordination behavioral tests; observe RED.
- [x] Implement recognizer, standby sampling at 20ms, common wake method, held shutdown darkening and suppression of wake during shutdown.
- [x] Verify input cancellation, rearming, different-position double taps, movement/hold/gap limits, invalid reads, clock wrap and no automatic 60s wake.

### Task 3: Save retry and final verification

**Files:** `firmware/sdgoods/main/RoomSession.cpp`, `tools/sdgoods_room_host.cpp`, plan/artifact records.
**Interfaces:** `RoomSession::poll()` retries a failed save after 2000ms regardless of food changes.

- [x] Add a real-engine assertion to locked-standby journey: inject storage failure, keep food unchanged, recover storage and observe retry/persisted snapshot without unlocking; observe RED.
- [x] Implement failed-save retry and force 30s idle threshold during the 60s locked journey; verify PASS.
- [x] Run power/wake, native touch, architecture and locked-room checks; build only SDGOODS Debug with existing toolchain.
- [x] Obtain one fresh review of this change, fix important findings with regression tests, record final evidence and firmware hash.
