#!/usr/bin/env python3
"""Select PR build targets from NUL-delimited repository paths on stdin."""

import json
from pathlib import PurePosixPath
import sys

ALL = {"sticks3", "amoled-v1", "amoled-v2", "web-sticks3", "web-amoled-v2"}
AMOLED = {"amoled-v1", "amoled-v2", "web-amoled-v2"}
WEB = {"web-sticks3", "web-amoled-v2"}


def build_targets(paths: list[str]) -> set[str]:
    targets = set()
    for path in paths:
        if not path or path.endswith(".md") or path.startswith(("doc/", "specs/")):
            continue
        if path in (".github/workflows/checks.yml", "tools/ci_build_scope.py"):
            targets.update(ALL)
        elif path.startswith(("data/", "src/game/", "src/assets/", "src/core/",
                              "src/presentation/", "src/hardware/", "src/third_party/",
                              "src/platform/api/")):
            targets.update(ALL)
        elif path.startswith("src/platform/web/"):
            targets.update({"web-amoled-v2"} if path.endswith("main_amoled.cpp") else
                           {"web-sticks3"} if path.endswith("main_web.cpp") else WEB)
        elif path.startswith("src/scenes/") or path == "src/main.cpp":
            targets.update({"sticks3", "web-sticks3"})
        elif path.startswith(("src/platform/m5stick_s3/", "src/hal/")):
            targets.add("sticks3")
        elif path.startswith("src/brain/"):
            targets.update({"amoled-v1", "amoled-v2"})
        elif path.startswith("firmware/amoled_1_8_v1/main/"):
            name = PurePosixPath(path).name
            if name in {"AmoledPlatform.cpp", "AmoledPlatform.h", "TouchInput.cpp", "TouchInput.h",
                        "main.cpp", "idf_component.yml", "CMakeLists.txt", "Kconfig.projbuild"}:
                targets.add("amoled-v1")
            else:
                targets.update(AMOLED)
        elif path.startswith("firmware/amoled_1_8_v1/"):
            targets.add("amoled-v1")
        elif path.startswith("firmware/amoled_1_8_v2/"):
            targets.add("amoled-v2")
        elif path.startswith("firmware/common/"):
            targets.update(AMOLED if PurePosixPath(path).name.startswith("amoled_") else
                           {"amoled-v1", "amoled-v2"})
        elif path in {"tools/amoled_source.py", "tools/amoled_host.py"}:
            targets.update(AMOLED)
        elif path.startswith("tools/build_web"):
            targets.update(WEB)
        elif path == "tools/build_amoled_variant.sh":
            targets.update({"amoled-v1", "amoled-v2"})
        elif path.startswith(("tools/generate_ota", "tools/package_amoled_v2")):
            targets.add("amoled-v2")
        elif path.startswith("web/amoled-v2/"):
            targets.add("web-amoled-v2")
        elif path.startswith("web/"):
            targets.add("web-sticks3")
        elif path == "platformio.ini" or path.startswith("partitions_stickmon"):
            targets.add("sticks3")
    return targets


def main():
    paths = sys.stdin.buffer.read().decode("utf-8", errors="surrogateescape").split("\0")
    targets = build_targets(paths)
    print("sticks3=" + str("sticks3" in targets).lower())
    print("amoled=" + json.dumps([board for board in ("v1", "v2")
                                 if "amoled-" + board in targets]))
    print("web=" + json.dumps([target for target in ("sticks3", "amoled-v2")
                              if "web-" + target in targets]))


if __name__ == "__main__":
    main()
