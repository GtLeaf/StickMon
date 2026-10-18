#!/usr/bin/env python3
"""Build independent StickS3 / AMOLED V2 WASM targets from this source tree."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

from amoled_source import amoled_runtime_sources

ROOT = Path(__file__).resolve().parents[1]


def sources_for(target):
    if target == "amoled-v2":
        sources = amoled_runtime_sources(ROOT)
    else:
        # Directory allowlist: never sweep ESP-IDF-only brain/platform sources.
        sources = [p for name in ("presentation", "assets", "core", "hardware", "scenes")
                   for p in sorted((ROOT / "src" / name).glob("*.cpp"))]
        sources += sorted((ROOT / "src/game").glob("*.cpp"))
        sources += [ROOT / "src/platform/api/PlatformServices.cpp",
                    ROOT / "src/third_party/uzlib/tinflate.c"]
    sources += [ROOT / "src/platform/web/WebPlatform.cpp",
                ROOT / ("src/platform/web/main_amoled.cpp" if target == "amoled-v2"
                        else "src/platform/web/main_web.cpp")]
    return sources


def stage_resources(target, stage):
    """Mirror the firmware resource mapping without packing a flash image."""
    if stage.exists():
        shutil.rmtree(stage)
    data = ROOT / "data"
    if target == "amoled-v2":
        subprocess.run(["python3", str(ROOT / "tools/validate_amoled_ui_assets.py"),
                        str(data / "packs/dev/game/ui.smonfx"),
                        str(data / "packs/dev/game/ui_amoled.smonfx")], check=True)
    # Only runtime files; exclude source PNGs and unused target-specific UI packs.
    extensions = {".json", ".smonsp", ".smonroom", ".smonfx", ".smonfont", ".smonaudio", ".smoncry"}
    for source in sorted(data.rglob("*")):
        if not source.is_file() or source.suffix not in extensions:
            continue
        relative = source.relative_to(data)
        if source.name == "ui_amoled.smonfx":
            continue
        if target == "amoled-v2" and relative.as_posix() == "packs/dev/game/ui.smonfx":
            source = source.with_name("ui_amoled.smonfx")
        destination = stage / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    fingerprint = hashlib.sha256()
    for p in sorted(stage.rglob("*")):
        if p.is_file():
            fingerprint.update(p.relative_to(stage).as_posix().encode())
            fingerprint.update(p.read_bytes())
    return fingerprint.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("sticks3", "amoled-v2"), default="sticks3")
    args = parser.parse_args()
    target = args.target
    out = ROOT / "build/web" / target
    objects = ROOT / "build/web-obj" / target
    out.mkdir(parents=True, exist_ok=True)
    objects.mkdir(parents=True, exist_ok=True)
    compiler = subprocess.check_output(["emcc", "--version"], text=True).splitlines()[0]
    flags = ["-O2", "-DNDEBUG", "-DSTICKMON_HAS_CLAW=0", "-DSTICKMON_ENABLE_DEBUG_FEATURES=0",
             "-DSTICKMON_ENABLE_TRACE_LOGS=0", "-MMD", "-MP", "-I" + str(ROOT / "src"),
             "-I" + str(ROOT / "firmware/amoled_1_8_v1/main")]
    signature = json.dumps([compiler, flags])
    signature_file = objects / "flags.json"
    force = not signature_file.exists() or signature_file.read_text() != signature
    object_files = []
    for source in sources_for(target):
        obj = objects / (source.relative_to(ROOT).as_posix().replace("/", "_") + ".o")
        dep = obj.with_suffix(".d")
        dependencies = []
        if dep.exists():
            dependencies = [Path(p) for p in shlex.split(dep.read_text().replace("\\\n", " ").split(":", 1)[1].split("\n", 1)[0])]
        if force or not obj.exists() or not dependencies or any(
                not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns
                for p in [source, *dependencies]):
            print("Compile", source.relative_to(ROOT), flush=True)
            subprocess.run(["emcc" if source.suffix == ".c" else "em++", *flags,
                            *([] if source.suffix == ".c" else ["-std=c++17"]),
                            "-c", str(source), "-o", str(obj)], cwd=ROOT, check=True)
        object_files.append(str(obj))
    signature_file.write_text(signature)
    resources = out / "resources"
    fingerprint = stage_resources(target, resources)
    exports = (["_main", "_stickmon_web_tick", "_stickmon_web_touch", "_stickmon_web_save", "_stickmon_web_pause", "_stickmon_web_resume"]
               if target == "amoled-v2" else
               ["_main", "_stickmon_button_down", "_stickmon_button_up", "_stickmon_set_acceleration"])
    subprocess.run(["em++", *object_files, "-O2", "-sINITIAL_MEMORY=67108864",
                    "-sALLOW_MEMORY_GROWTH=1", "-sMAXIMUM_MEMORY=268435456",
                    "-sEXPORTED_FUNCTIONS=" + json.dumps(exports),
                    '-sEXPORTED_RUNTIME_METHODS=["ccall","cwrap"]',
                    "-sFORCE_FILESYSTEM=1", "-sEXIT_RUNTIME=0", "--preload-file", str(resources) + "@/",
                    "-o", str(out / "stickmon.js")], cwd=ROOT, check=True)
    shell = ROOT / ("web/amoled-v2/index.html" if target == "amoled-v2" else "web/index.html")
    shutil.copyfile(shell, out / "index.html")
    if target == "sticks3":
        shutil.copyfile(ROOT / "web/device-body.webp", out / "device-body.webp")
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()
    metadata = {"target": target, "sourceCommit": git("rev-parse", "HEAD"),
                "sourceDirty": bool(git("status", "--porcelain", "--", "src", "firmware", "data", "web", "tools")), "compiler": compiler,
                "resourceSha256": fingerprint}
    import re
    for key, path, constant in (("saveVersion", "src/game/GameState.h", "SAVE_VERSION"),
                                ("saveSchema", "src/core/SaveCodec.h", "SCHEMA_VERSION")):
        metadata[key] = int(re.search(r"\b" + constant + r"\s*=\s*(\d+)", (ROOT / path).read_text())[1])
    (out / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print("Built", target, "→", out, flush=True)


if __name__ == "__main__":
    main()
