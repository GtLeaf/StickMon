# StickMon AMOLED 1.75C bring-up

This is the Waveshare ESP32-S3-Touch-AMOLED-1.75C target for StickMon.
It uses the official BSP and the shared StickMon application with the logical
184x224 canvas centered and scaled 2x onto the 466x466 AMOLED:

- ESP32-S3R8, 8 MB PSRAM, 32 MB Flash
- CO5300 QSPI AMOLED, 466x466
- Official `waveshare/esp32_s3_touch_amoled_1_75c` BSP
- CO5300 display and CST9217 touch input
- StickMon resources, save data, audio, and ESP-NOW platform services
- Optional ESP-Claw remote chat and autonomous game bridge

Build with ESP-IDF 5.5.x from this directory:

```text
idf.py set-target esp32s3
idf.py build
```

For isolated profiles from the repository root:

```text
./tools/build_amoled_variant.sh 1_75c lite
./tools/build_amoled_variant.sh 1_75c claw
```

The Claw profile links the local ESP-Claw tree through `STICKMON_ESPCLAW_ROOT`
(or the sibling `../espclaw/esp-claw-master` path). It starts at boot and waits
for `wifi`, `coze_token`, `coze_bot_id`, and optional `coze_base_url` values in
NVS. The StickMon settings page exposes the setup portal and current portal
address when credentials are not yet present.
