"""Compile a native executable against the production AMOLED application."""

from pathlib import Path
import subprocess
import sys
import tempfile

from amoled_source import amoled_runtime_sources

ROOT = Path(__file__).resolve().parents[1]


def build_amoled_host(output: Path, host_sources: list[str | Path], debug: bool = False) -> Path:
    """Callers provide only their probes; production sources come from manifests."""
    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    sources = [Path(source) for source in host_sources] + amoled_runtime_sources(ROOT)
    sources.append(ROOT / "src/platform/desktop/DesktopPlatform.cpp")
    with tempfile.TemporaryDirectory(prefix="amoled-host-") as directory:
        temporary = Path(directory)
        # Only touch-device handle declarations are needed on a native host.
        (temporary / "esp_err.h").write_text("#pragma once\nusing esp_err_t = int;\n")
        (temporary / "esp_lcd_touch.h").write_text(
            "#pragma once\nusing esp_lcd_touch_handle_t = void*;\n")
        objects = []
        for index, source in enumerate(sources):
            if source.suffix == ".c":
                obj = temporary / f"portable-{index}.o"
                subprocess.run(["cc", "-O1", "-c", str(source), "-o", str(obj)],
                               cwd=ROOT, check=True)
                objects.append(str(obj))
        subprocess.run([
            "c++", "-std=c++17", "-O1", "-ffunction-sections", "-fdata-sections",
            "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
            "-Isrc", "-Ifirmware/amoled_1_8_v1/main", "-I" + directory,
            "-DSTICKMON_HAS_CLAW=0", f"-DSTICKMON_ENABLE_DEBUG_FEATURES={int(debug)}",
            *[str(source) for source in sources if source.suffix == ".cpp"],
            *objects, "-o", str(output),
        ], cwd=ROOT, check=True)
    return output
