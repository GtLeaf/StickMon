"""Share one native render harness build across test classes in a process."""
import atexit
from contextlib import ExitStack
from functools import lru_cache
import os
from pathlib import Path
import shutil
import tempfile

from amoled_host import build_amoled_host

_temporary_builds = ExitStack()
atexit.register(_temporary_builds.close)


@lru_cache(maxsize=2)
def native_render_binary(debug=False):
    directory = _temporary_builds.enter_context(tempfile.TemporaryDirectory())
    binary = Path(directory) / "amoled-native-render"
    build_amoled_host(binary, [
        "tools/amoled_checksum_guard.cpp",
        "tools/amoled_native_render_host.cpp",
        "tools/amoled_battery_host.cpp",
        "tools/amoled_ecology_host.cpp",
        "tools/amoled_ui_behavior_host.cpp",
        "tools/amoled_expedition_host.cpp",
        "tools/amoled_battle_flow_host.cpp",
        "tools/amoled_shower_host.cpp",
        "tools/amoled_bath_hatch_controller_host.cpp",
        "tools/amoled_care_host.cpp",
        "tools/amoled_lock_power_host.cpp",
    ], debug=debug)
    keep = os.environ.get("AMOLED_RENDER_KEEP_BINARY")
    if keep:
        target = Path(keep).resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(binary, target)
        target.chmod(0o755)
    return str(binary)
