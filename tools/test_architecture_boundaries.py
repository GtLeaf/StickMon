#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp"}


def source_files(folder):
    return [
        path
        for path in folder.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    ]


class ArchitectureBoundaryTests(unittest.TestCase):
    def assert_no_patterns(self, folder, patterns):
        violations = []
        for path in source_files(folder):
            text = path.read_text(encoding="utf-8")
            for label, pattern in patterns:
                if re.search(pattern, text):
                    violations.append(
                        f"{path.relative_to(ROOT)} depends on {label}"
                    )
        self.assertEqual([], violations, "\n" + "\n".join(violations))

    def test_game_logic_is_host_buildable(self):
        self.assert_no_patterns(
            ROOT / "src" / "game",
            [
                ("Arduino", r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
                ("M5 SDK", r"#\s*include\s*[<\"]M5(?:Unified|GFX)"),
                ("hardware layer", r"#\s*include\s*[<\"]hardware/"),
                ("concrete platform", r"#\s*include\s*[<\"]platform/m5stick_s3/"),
                ("ESP-IDF", r"#\s*include\s*[<\"](?:esp_|driver/|freertos/)"),
            ],
        )

    def test_presentation_is_device_independent(self):
        self.assert_no_patterns(
            ROOT / "src" / "presentation",
            [
                ("Arduino", r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
                ("M5 SDK", r"#\s*include\s*[<\"]M5(?:Unified|GFX)"),
                ("GPIO or ESP-IDF", r"#\s*include\s*[<\"](?:esp_|driver/|freertos/)"),
                ("concrete platform", r"#\s*include\s*[<\"]platform/m5stick_s3/"),
            ],
        )

    def test_m5_sdk_is_confined_to_platform_adapter(self):
        allowed = Path("src/platform/m5stick_s3/M5StickS3Platform.cpp")
        violations = []
        for path in source_files(ROOT / "src"):
            text = path.read_text(encoding="utf-8")
            if re.search(r"#\s*include\s*[<\"]M5(?:Unified|GFX)", text):
                relative = path.relative_to(ROOT)
                if relative != allowed:
                    violations.append(str(relative))
        self.assertEqual([], violations, "\n" + "\n".join(violations))

    def test_arduino_header_is_confined_to_entrypoint_and_platform_adapter(self):
        allowed = {
            Path("src/main.cpp"),
            Path("src/platform/m5stick_s3/M5StickS3Platform.cpp"),
        }
        violations = []
        for path in source_files(ROOT / "src"):
            text = path.read_text(encoding="utf-8")
            if re.search(r"#\s*include\s*[<\"]Arduino\.h[>\"]", text):
                relative = path.relative_to(ROOT)
                if relative not in allowed:
                    violations.append(str(relative))
        self.assertEqual([], violations, "\n" + "\n".join(violations))

    def test_sprite_generator_uses_platform_services(self):
        generator = (
            ROOT / "tools" / "generate_pokemon_sprites.py"
        ).read_text(encoding="utf-8")
        forbidden = (
            "#include <Arduino.h>",
            "Serial.",
            "ESP.",
            "psramFound()",
            "ps_malloc(",
            "fs::File",
        )
        self.assertEqual([], [token for token in forbidden if token in generator])

    def test_core_storage_and_resources_use_platform_services(self):
        self.assert_no_patterns(
            ROOT / "src" / "core",
            [
                ("Preferences", r"#\s*include\s*[<\"]Preferences\.h[>\"]"),
                ("LittleFS", r"#\s*include\s*[<\"]LittleFS\.h[>\"]"),
                ("ESP NVS", r"#\s*include\s*[<\"]nvs\.h[>\"]"),
                ("ESP heap", r"#\s*include\s*[<\"]esp_heap_caps\.h[>\"]"),
                ("Arduino FS handles", r"\bfs::File\b"),
            ],
        )

    def test_link_protocol_is_transport_independent(self):
        self.assert_no_patterns(
            ROOT / "src" / "hardware",
            [
                ("ESP-NOW", r"#\s*include\s*[<\"]esp_now\.h[>\"]"),
                ("WiFi", r"#\s*include\s*[<\"]WiFi\.h[>\"]"),
                ("FreeRTOS", r"#\s*include\s*[<\"]freertos/"),
            ],
        )
        link_source = "\n".join(
            (ROOT / "src" / "hardware" / name).read_text(encoding="utf-8")
            for name in ("EspNowLink.h", "EspNowLink.cpp")
        )
        for label, pattern in (
            ("Arduino", r"#\s*include\s*[<\"]Arduino\.h[>\"]"),
            ("legacy Hal", r"#\s*include\s*[<\"]hardware/Hal\.h[>\"]"),
        ):
            self.assertIsNone(
                re.search(pattern, link_source),
                f"EspNowLink depends on {label}",
            )


if __name__ == "__main__":
    unittest.main()
