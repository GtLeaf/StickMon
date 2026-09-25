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

V2 packages the same SPIFFS resources as V1, including all generated `.smonsp`
sprite packs, `zh16.smonfont` and `ascii16-unscii.smonfont`. The shared AMOLED
renderer loads the Chinese font for UTF-8 names and prompts, so the V1/V2
application code and font assets stay aligned.

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

The V2 Lite debug build has been flashed and exercised on hardware for an
automatic exploration and wild encounter. Full hardware acceptance, including
Claw connectivity, remains pending.
