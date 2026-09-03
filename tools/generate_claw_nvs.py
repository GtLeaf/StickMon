#!/usr/bin/env python3
"""Generate the StickMon ESP-Claw NVS image without storing secrets in git."""

import argparse
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile


NVS_SIZE = 0x6000
DEFAULT_BASE_URL = "https://api.coze.cn"
DEFAULT_BOT_ID = "7679649015453597711"
DEFAULT_WECHAT_BASE_URL = "https://ilinkai.weixin.qq.com"
DEFAULT_WECHAT_CDN_URL = "https://novac2c.cdn.weixin.qq.com/c2c"


def nvs_generator() -> Path:
    idf_path = os.environ.get("IDF_PATH")
    candidates = []
    if idf_path:
        candidates.append(
            Path(idf_path)
            / "components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"
        )
    candidates.append(
        Path.home()
        / ".espressif/v5.5.4/esp-idf/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py"
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise SystemExit(
        "nvs_partition_gen.py not found; source ESP-IDF export.sh first or set IDF_PATH"
    )


def add_blob(writer, key: str, value: str) -> None:
    if len(key) > 15:
        raise ValueError(f"NVS key {key!r} exceeds the 15-character limit")
    if "\x00" in value:
        raise ValueError(f"NVS value for {key!r} contains a NUL byte")
    writer.writerow((key, "data", "hex2bin", value.encode("utf-8").hex()))


def add_optional_blob(writer, key: str, value: str) -> None:
    if value:
        add_blob(writer, key, value)


def write_csv(path: Path, args: argparse.Namespace) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("key", "type", "encoding", "value"))
        writer.writerow(("wifi", "namespace", "", ""))
        add_blob(writer, "ssid", args.ssid)
        add_blob(writer, "password", args.password)
        writer.writerow(("claw", "namespace", "", ""))
        add_blob(writer, "coze_token", args.coze_token)
        add_blob(writer, "coze_bot_id", args.bot_id)
        add_blob(writer, "coze_base_url", args.base_url)
        add_optional_blob(writer, "wechat_token", args.wechat_token)
        add_optional_blob(writer, "wechat_base_url", args.wechat_base_url)
        add_optional_blob(writer, "wechat_cdn_url", args.wechat_cdn_url)
        add_optional_blob(writer, "wechat_acct_id", args.wechat_account_id)
        add_optional_blob(writer, "telegram_token", args.telegram_token)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ssid", required=True, help="Wi-Fi SSID")
    parser.add_argument("--password", required=True, help="Wi-Fi password")
    parser.add_argument("--coze-token", required=True, help="Coze access token")
    parser.add_argument("--bot-id", default=DEFAULT_BOT_ID, help="Coze bot ID")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument(
        "--wechat-token",
        default="",
        help="Optional pre-issued ESP-Claw WeChat iLink token; QR login is preferred",
    )
    parser.add_argument("--wechat-base-url", default=DEFAULT_WECHAT_BASE_URL)
    parser.add_argument("--wechat-cdn-url", default=DEFAULT_WECHAT_CDN_URL)
    parser.add_argument("--wechat-account-id", default="default")
    parser.add_argument(
        "--telegram-token",
        default="",
        help="Optional Telegram BotFather token",
    )
    parser.add_argument("--output", required=True, type=Path, help="Output NVS image")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generator = nvs_generator()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".csv", prefix="stickmon-claw-", delete=False
    ) as temp:
        csv_path = Path(temp.name)
    try:
        write_csv(csv_path, args)
        subprocess.run(
            [
                sys.executable,
                str(generator),
                "generate",
                str(csv_path),
                str(args.output),
                hex(NVS_SIZE),
            ],
            check=True,
        )
    finally:
        csv_path.unlink(missing_ok=True)
    print(f"Generated {args.output} ({NVS_SIZE:#x} bytes)")
    print("Warning: flashing this image replaces the whole NVS partition, including StickMon saves.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
