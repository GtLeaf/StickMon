# V2 factory firmware

Place the official image below in this directory:

`ESP32-S3-Touch-AMOLED-1.8-V2-FactoryXiaozhi_260601.bin`

- Board: V2, CO5300 display and CST820 touch controller
- Flash address: `0x0`
- Size: `16777216` bytes
- SHA-256: `6f188fb9d35ee793a3423934a4fa4e7c1fef9cc9dae76f9f177dabe854a6cdb3`

The source image is available in the [official Waveshare Firmware directory](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8/tree/main/Firmware).

From the repository root:

```bash
./firmware/amoled_1_8_v2/flash_factory.sh --port /dev/cu.usbmodem2101
```

The script verifies the image and erases the full flash before writing. Use `--skip-erase` only when a full erase is not needed.
