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

The Claw and Lite builds resolve dependencies into their own isolated
`build-claw` and `build-lite` directories, keeping machine-specific
ESP-Claw paths out of the shared source tree.

This target is compile-ready. Hardware acceptance is still pending because a
V2 board is not currently available in the lab.
