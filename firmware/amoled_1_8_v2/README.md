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

Build with ESP-IDF 5.5.x:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

This target is compile-ready. Hardware acceptance is still pending because a
V2 board is not currently available in the lab.
