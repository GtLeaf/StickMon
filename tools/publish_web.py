#!/usr/bin/env python3
"""Build, validate and copy simulator artifacts to a local flasher checkout."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORIES = {"sticks3": "", "amoled-v2": "stickmon/waveshare-amoled-1-8-v2"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--flasher", type=Path, required=True)
    parser.add_argument("--target", choices=(*DIRECTORIES, "all"), default="all")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    flasher = args.flasher.resolve()
    targets = list(DIRECTORIES) if args.target == "all" else [args.target]
    hasher = flasher / "scripts/hash-sim-assets.js"
    if not hasher.is_file():
        parser.error("--flasher must point to the flasher checkout")
    for target in targets:
        if not args.skip_build:
            subprocess.run(["python3", str(ROOT / "tools/build_web.py"), "--target", target], check=True)
    with tempfile.TemporaryDirectory(prefix="stickmon-web-publish-") as tmp:
        stage = Path(tmp)
        # Verify every target before touching the serving directories.
        for target in targets:
            folder = stage / DIRECTORIES[target]
            folder.mkdir(parents=True, exist_ok=True)
            files = ["index.html", "stickmon.js", "stickmon.wasm", "stickmon.data", "build-info.json"]
            if target == "sticks3":
                files.append("device-body.webp")
            for filename in files:
                shutil.copyfile(ROOT / "build/web" / target / filename, folder / filename)
            subprocess.run(["node", str(hasher), "--root", str(stage), "--dir", DIRECTORIES[target]], check=True)
            subprocess.run(["node", "-e", "const m=require(process.argv[1]);const r=m.inspectSimulator(process.argv[2],process.argv[3]);if(!r.available)throw Error(r.error)",
                            str(flasher / "src/simulators.js"), DIRECTORIES[target], str(stage)], check=True)
        for target in targets:
            folder = stage / DIRECTORIES[target]
            destination = flasher / "public/sim" / DIRECTORIES[target]
            destination.mkdir(parents=True, exist_ok=True)
            files = [p for p in folder.iterdir() if p.is_file() and p.name != "index.html"]
            for source in [*files, folder / "index.html"]:
                # The entry switches last; old hashes remain for in-flight loads.
                temporary = destination / (source.name + ".tmp")
                shutil.copyfile(source, temporary)
                os.replace(temporary, destination / source.name)
            print("Published local simulator:", target, "→", destination)


if __name__ == "__main__":
    main()
