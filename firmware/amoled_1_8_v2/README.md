# StickMon AMOLED V2

This is the ESP-IDF target for the Waveshare ESP32-S3-Touch-AMOLED-1.8 V2
board (CO5300 display and CST820 touch controller).

The application and renderer are intentionally shared with V1:

- `../amoled_1_8_v1/main/AmoledApp.cpp`
- `../amoled_1_8_v1/main/HomeScreen.cpp`
- `../../src/` shared game, save, resource and ESP-NOW code

Only the board layer is V2-specific: `main.cpp`, `AmoledPlatform.*` and
`TouchInput.*`. The V2 target uses the official Waveshare BSP `^2.0.3`,
which provides the CO5300 panel path and V2 touch auto-detection.

## Touch calibration

V2 uses its own raw-to-native-pixel calibration in `main/TouchInput.cpp`:
`x = round(rawX * 1.1179618 - 20.711235)` and
`y = round(rawY * 1.0494687 - 21.692282)`, clamped to 368x448.
The BSP applies no swap, mirror or coordinate scaling on this path.
The former V1 coefficients compressed V2 touches toward the screen center.

The coefficients are independent-axis least-squares fits of the user's
2026-10-13 nine-point DOWN measurements (session timestamps 660581-668621).
UP events retain the last sensor coordinates for diagnostics, rather than
reporting already-calibrated coordinates as raw.

| Target | Raw DOWN | Previous mapped DOWN |
| --- | --- | --- |
| 24,32 | 44,53 | 56,69 |
| 184,32 | 187,42 | 184,60 |
| 344,32 | 329,53 | 312,69 |
| 24,224 | 43,238 | 55,232 |
| 184,224 | 180,240 | 178,234 |
| 344,224 | 326,237 | 309,231 |
| 24,416 | 35,405 | 47,379 |
| 184,416 | 179,421 | 177,393 |
| 344,416 | 325,418 | 308,391 |

Replay mean Euclidean error changes from 35.2 to 6.2 native pixels, and
maximum error from 48.9 to 14.3 pixels. These are fit residuals on the same
samples, not a new hardware acceptance result. A full affine fit only reduces
mean replay error to 5.2 pixels; keep the simpler axis fit until repeat
measurements establish a consistent skew.
Run `python3 tools/test_amoled_v2_touch.py` for driver replay, boundaries and
release diagnostics. After loading the firmware separately, repeat
`Debug -> 触摸` in the same nine-point order and tap the crosshair centers
briefly without dragging. The diagnostic page does not save calibration.

## Shared application

V2 packages the same SPIFFS resources as V1, including all generated `.smonsp`
sprite packs, `zh16.smonfont` and `ascii16-unscii.smonfont`. The shared AMOLED
renderer loads the Chinese font for UTF-8 names and prompts, so the V1/V2
application code and font assets stay aligned.

Locking returns to the room and keeps the pet visible. Double-tap the pet
within 500 ms to unlock; taps elsewhere, single taps, holds and drags are ignored.
An active expedition returns through the normal home transition before locking.

The shared settings page includes `重置游戏` with a separate cancel/confirm page.
Confirmed resets clear only game data and restart with a new egg;
Wi-Fi and ESP-Claw credentials are preserved. V2 keeps `系统更新` as a separate
settings entry. Both entries fit on screen alongside the existing controls.

V1/V2 share the QMI8658 acceleration driver for egg interaction. Tap the egg
for a heart plus a 1 second wobble, or shake the device for a 1 second wobble; the bottom HP position shows
the numeric hatch countdown. Only hatching enables acceleration sampling;
menus and hatch completion put the sensors and oscillator into standby.
The gyroscope stays off. Shake sensitivity and power behavior await hardware QA.

First boot without a save also starts with an egg. The shared V1/V2 hatch flow
takes 180 seconds at 1x speed, pauses in menus, saves progress every 15 game
seconds and after interactions, and resumes after restarting. Taps and shakes
choose the level-5 starter using the StickS3 rules; hatching grants 1000 coins.
Existing completed saves continue directly to the room.

V2 uses the same trimmed ESP-Claw remote-chat integration and NVS keys as V1;
see `../amoled_1_8_v1/README.md#esp-claw-remote-chat`. The V2 board layer only
changes display and touch drivers.

The `电脑 -> ESP-Claw` page provides the same Wi-Fi, Coze, Telegram Bot Token,
and WeChat QR-login flow as V1. V2 hardware acceptance is still pending, so
verify the portal and touch behavior on the actual board after flashing.

Build with ESP-IDF 5.5.x:

```sh
cd /Users/gtleaf/project/esp/StickMon
./tools/build_amoled_variant.sh v2 claw
./tools/build_amoled_variant.sh v2 lite
```

Each successful non-debug V2 build also creates `build-claw/out/` or
`build-lite/out/` with the five files expected by the web flasher upload
form: `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`,
and `littlefs.bin`. The `boot_app0.bin` and `littlefs.bin` slot names refer
to V2's OTA initialization data and SPIFFS resources, respectively.
Choose all five files from one variant; do not mix Claw and Lite builds.
The packager checks `flasher_args.json` against the V2 offsets before copying.
To export an existing build without rebuilding, run:

```sh
python3 tools/package_amoled_v2_release.py firmware/amoled_1_8_v2/build-lite
```

Configure the Waveshare V2 device in the web flasher for 16MB ESP32-S3 with
offsets `0x0`, `0x8000`, `0xf000`, `0x20000`, and `0x620000` in that order.
After upload, inspect the generated manifest: a server-wide `PARTITIONS_CSV`
setting can override the device's application and resource offsets.

Flash V2 from its own target directory so the board selection cannot fall
back to the V1 display and touch drivers:

```sh
./firmware/amoled_1_8_v2/flash.sh \\
  --port /dev/cu.usbmodemXXXX \\
  --variant claw \\
  --debug
```

Use `--variant lite` for the firmware without ESP-Claw. Add `--erase` once
when recovering from a previously mixed V1/V2 image.

The Claw and Lite builds resolve dependencies into their own isolated
`build-claw` and `build-lite` directories, keeping machine-specific
ESP-Claw paths out of the shared source tree.

## Explore performance capture (debug build)

`[ExplorePerf]` reports successful walking-frame transfers once per second:
FPS, dirty rows, update/draw/LCD timings, and transfer-completion intervals
(average, nearest-rank p95, maximum, and count above 50 ms). `windowUs` gives
the actual sample duration; the final partial window can include scene-transition
time and should be excluded when comparing steady walking FPS.
`[ExploreMapPerf]` separates cached-map copies from animated tiles, and
`[ExploreSavePerf]` measures synchronous route saves. Per-step, camera, and
actor geometry logs require `STICKMON_ENABLE_TRACE_LOGS=1`; they are disabled
in the normal debug build to reduce measurement overhead. Performance summaries
still add some logging overhead and are absent from non-debug builds.

Exploration writes snapshots after pickups, on battle exit (including flee
and defeat), and on return home, rather than at every ordinary step. Ten-step
recovery is also kept in memory until a save event. Display locking saves after
settling any active expedition; the system-update restart saves before OTA.
Periodic care saves pause during exploration and battle, and remain active at home.
Item consumption and progression decisions still save immediately. Sudden
power loss can discard steps and recovery since the last save event.

The V2 debug firmware accepts `diag ping` and `diag explore 0` through its
USB Serial/JTAG port. The latter starts the normal departure and auto-walks
area 0; it does not bypass encounter rules. The command is absent from
non-debug builds. Close `idf.py monitor` before running the capture script,
since only one process can own the serial port at a time:

```sh
/Users/gtleaf/.espressif/python_env/idf5.5_py3.11_env/bin/python \
  tools/capture_amoled_v2_explore.py --port /dev/cu.usbmodemXXXX
```

Run this from the repository root with a living leader on the home screen.
The script prints its log path under `tools/out/` and exits after the first
`[EncounterPerf]` line (or a 180-second timeout). Flashing without `--erase`
preserves NVS, but the expedition itself can update the game save.

`diag status` compares selected in-memory game values with the stored snapshot
and reports successful saves since boot. `diag tap X Y` sends a normal down/up
touch through the app using native 368x448 coordinates. These commands and
`[SaveDiag]` save-event logs are debug-only and do not bypass gameplay gates.

The V2 Lite debug build has been flashed and exercised on hardware for an
automatic exploration and wild encounter. Full hardware acceptance, including
Claw connectivity, remains pending.
