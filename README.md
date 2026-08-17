# StickMon

StickS3 electronic pet prototype for M5Stack StickS3 / ESP32-S3.

## Build

```bash
# Debug firmware (includes the Debug menu and diagnostics)
pio run -e m5stick-s3-debug

# Release firmware
pio run -e m5stick-s3
```

The project follows the PokeBug StickS3 PlatformIO baseline and enables PSRAM through `BOARD_HAS_PSRAM`.

## External Assets

Asset generation tools look for Pokemon Essentials under the ignored
`external/pokemon-essentials` directory by default. To keep those files outside
the repository, point the tools at another location:

```bash
export ESSENTIALS_DIR=/path/to/pokemon-essentials
```

## Flash Firmware and LittleFS

`tools/upload_firmware_and_fs.sh` builds and uploads both the firmware and the
complete `data/` directory as a LittleFS image. Close any running PlatformIO
serial monitor before flashing so it does not occupy the device port.

```bash
# Build and upload Debug firmware plus LittleFS (default)
./tools/upload_firmware_and_fs.sh

# Build and upload Release firmware plus LittleFS
./tools/upload_firmware_and_fs.sh --release

# Use a specific serial port
./tools/upload_firmware_and_fs.sh --port /dev/cu.usbmodem2101

# Preview all commands without building or uploading
./tools/upload_firmware_and_fs.sh --dry-run
```

The script auto-detects the serial port when `--port` is omitted. It also
exports `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`,
and `littlefs.bin` to `tools/out/` for the web flasher. Run
`./tools/upload_firmware_and_fs.sh --help` for all options and environment
overrides.

## Current Prototype

- 135x240 vertical room scene.
- Color-block placeholder monster rendering with z/layer ordering.
- Main scene A: pet the monster.
- Main scene B: add food.
- Main scene long A: open the CyberGardener-style vertical menu.
- Menu B: move cursor.
- Menu A: confirm.
- Menu long A/B: return to the room.
- Social scene has an ESP-NOW room MVP for battle/trade/evolution discovery.
- Shop scene sells balls, food, and candy placeholders.
- Explore scene has step-threshold encounters, ball capture, and coin rewards.
- Settings scene follows the CyberGardener list style with values on the same row.
- Chinese UI text uses a generated 16px bitmap table derived from Sarasa
  Gothic SC Regular. All ASCII letters, numbers, punctuation, and spaces use
  Unscii at its native 8x16 size; the gender symbols use Unscii as well.

After changing UI copy in `src/core/UiStrings.h`, regenerate the glyph table:

```bash
python3 tools/generate_font16cn.py
```

The source fonts and their SIL Open Font Licenses are stored under
`third_party/fonts/sarasa-gothic-sc/`. Unscii source and licensing details are
stored under `third_party/fonts/unscii/`.

## Asset Notes

- Room asset pipeline: [doc/房间资源生成与运行时绘制说明.md](doc/房间资源生成与运行时绘制说明.md)
