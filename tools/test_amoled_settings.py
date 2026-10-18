#!/usr/bin/env python3
"""Settings integration behavior, independent of application source layout."""

import unittest
from amoled_page_cases import run_page_case


class AmoledSettingsTests(unittest.TestCase):
    def test_saved_brightness_is_applied_during_startup(self):
        run_page_case("settings-sliders")


if __name__ == "__main__":
    unittest.main()
