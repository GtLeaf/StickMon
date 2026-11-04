#!/usr/bin/env python3

import argparse
import json
import shutil
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


UPLOAD_FILES = {
    "0x0": ("bootloader/bootloader.bin", "bootloader.bin"),
    "0x8000": ("partition_table/partition-table.bin", "partitions.bin"),
    "0xf000": ("ota_data_initial.bin", "boot_app0.bin"),
    "0x20000": ("stickmon_amoled_1_8_v2.bin", "firmware.bin"),
    "0x620000": ("resources.bin", "littlefs.bin"),
}


def release_output_dir(build_dir: Path) -> Path:
    return ROOT / '.stick-local/firmware/amoled-v2' / build_dir.resolve().name


def package_release(build_dir: Path, output_dir: Path | None = None) -> Path:
    build_dir = build_dir.resolve()
    flash_args = json.loads((build_dir / "flasher_args.json").read_text())
    if flash_args.get("flash_settings", {}).get("flash_size") != "16MB":
        raise ValueError("V2 release requires a 16MB flash build")
    if flash_args.get("flash_files") != {
        offset: source for offset, (source, _) in UPLOAD_FILES.items()
    }:
        raise ValueError("V2 flash files or offsets differ from the upload layout")

    for source, _ in UPLOAD_FILES.values():
        artifact = build_dir / source
        if not artifact.is_file() or artifact.stat().st_size == 0:
            raise FileNotFoundError(f"missing or empty build artifact: {artifact}")

    output = output_dir if output_dir is not None else release_output_dir(build_dir)
    if output.is_symlink():
        raise ValueError(f"output path is not a directory: {output}")
    output = output.resolve()
    if output == build_dir or build_dir.is_relative_to(output):
        raise ValueError("output directory cannot contain the source build")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".out-", dir=output.parent) as staging_dir:
        staging = Path(staging_dir)
        for source, destination in UPLOAD_FILES.values():
            shutil.copy2(build_dir / source, staging / destination)
        if output.exists():
            if not output.is_dir() or output.is_symlink():
                raise ValueError(f"output path is not a directory: {output}")
            shutil.rmtree(output)
        shutil.move(str(staging), str(output))
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Export V2 web-flasher upload files.")
    parser.add_argument('build_dir', type=Path)
    parser.add_argument('--output-dir', type=Path,
                        help='Export directory (default: .stick-local/firmware/amoled-v2/BUILD_NAME)')
    args = parser.parse_args()
    try:
        output = package_release(args.build_dir, args.output_dir)
    except (FileNotFoundError, ValueError) as error:
        raise SystemExit(f"V2 release packaging failed: {error}") from error
    print(f"V2 web-flasher upload files: {output}")
    for _, destination in UPLOAD_FILES.values():
        print(f"  {destination}")
