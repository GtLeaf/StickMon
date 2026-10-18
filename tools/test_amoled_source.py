#!/usr/bin/env python3
"""Check that board and desktop targets compile the same explicit runtime."""

from pathlib import Path
import subprocess
import tempfile
import unittest

from amoled_source import amoled_runtime_sources, amoled_shared_sources, read_source_manifest
from build_web import sources_for

ROOT = Path(__file__).resolve().parents[1]


class AmoledSourceTests(unittest.TestCase):
    def test_cmake_and_python_manifests_match_exactly(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\nproject(source_manifest NONE)\n'
                f'include("{ROOT}/firmware/common/amoled_sources.cmake")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" '
                '"${STICKMON_AMOLED_APP_SOURCES};${STICKMON_AMOLED_UI_SOURCES};'
                '${STICKMON_AMOLED_SHARED_SOURCES}")\n')
            subprocess.run(["cmake", "-S", str(base), "-B", str(base / "build")],
                           check=True, capture_output=True, text=True)
            cmake_sources = (base / "build/sources.txt").read_text().split(";")
            self.assertEqual(cmake_sources, [str(path) for path in amoled_runtime_sources(ROOT)])

    def test_web_uses_every_shared_source_once_and_only_adds_web_adapter(self):
        runtime = amoled_runtime_sources(ROOT)
        web = sources_for("amoled-v2")
        self.assertEqual(web[:len(runtime)], runtime)
        self.assertEqual(web[len(runtime):], [ROOT / "src/platform/web/WebPlatform.cpp",
                                             ROOT / "src/platform/web/main_amoled.cpp"])
        self.assertEqual(len(web), len(set(web)))

    def test_both_board_components_consume_every_manifest_entry_once(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\nproject(source_consumers NONE)\n'
                'macro(idf_component_register)\n'
                '  cmake_parse_arguments(component "" "" "SRCS;INCLUDE_DIRS;REQUIRES" ${ARGN})\n'
                '  file(WRITE "${CMAKE_BINARY_DIR}/${board}.txt" "${component_SRCS}")\n'
                '  return()\n'
                'endmacro()\n'
                'foreach(board IN ITEMS v1 v2)\n'
                f'  include("{ROOT}/firmware/amoled_1_8_${{board}}/main/CMakeLists.txt")\n'
                'endforeach()\n')
            subprocess.run(["cmake", "-S", str(base), "-B", str(base / "build")],
                           check=True, capture_output=True, text=True)
            for board in ("v1", "v2"):
                sources = (base / f"build/{board}.txt").read_text().split(";")
                for source in amoled_runtime_sources(ROOT):
                    with self.subTest(board=board, source=source):
                        self.assertEqual(sources.count(str(source)), 1)
                extras = set(sources) - {str(source) for source in amoled_runtime_sources(ROOT)}
                self.assertEqual(len(extras), 5 if board == "v1" else 6)

    def test_shared_manifest_covers_game_rules_and_excludes_device_implementations(self):
        shared = amoled_shared_sources(ROOT)
        self.assertEqual({path for path in shared if path.parent == ROOT / "src/game"},
                         set((ROOT / "src/game").glob("*.cpp")))
        allowed = ("src/assets/", "src/core/", "src/game/", "src/hardware/",
                   "src/presentation/", "src/platform/api/", "src/third_party/")
        for source in shared:
            with self.subTest(source=source):
                self.assertTrue(source.relative_to(ROOT).as_posix().startswith(allowed))

    def test_invalid_manifests_fail_before_compilation(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / "real.cpp").touch()
            manifest = base / "sources.txt"
            manifest.write_text("# Comment\n\nreal.cpp\n")
            self.assertEqual(read_source_manifest(manifest, base), [base / "real.cpp"])
            for entries in ("missing.cpp\n", "real.cpp\nreal.cpp\n", "../real.cpp\n",
                            "/tmp/real.cpp\n", "header.h\n"):
                with self.subTest(entries=entries), self.assertRaises(ValueError):
                    manifest.write_text(entries)
                    read_source_manifest(manifest, base)


if __name__ == "__main__":
    unittest.main()
