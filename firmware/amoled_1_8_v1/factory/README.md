# V1 factory firmware

Place the official image below in this directory:

`ESP32-S3-Touch-AMOLED-1.8-FactoryXiaozhi_250805.bin`

- Board: V1, SH8601 display and FT3168 touch controller
- Flash address: `0x0`
- Size: `16732160` bytes
- SHA-256: `033ba27f0d1824835e90fe6b41d2db8c1f13cda7e1d80c82b3f7537dafb8dc8d`

The source image is available in the [official Waveshare Firmware directory](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8/tree/main/Firmware).

From the repository root:

```bash
./firmware/amoled_1_8_v1/flash_factory.sh --port /dev/cu.usbmodem2101
```

The script verifies the image and erases the full flash before writing. Use `--skip-erase` only when a full erase is not needed.
