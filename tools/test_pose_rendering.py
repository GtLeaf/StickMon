#!/usr/bin/env python3
"""Compare cropped-pose drawing to the original virtual-canvas renderers."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PoseRenderingTests(unittest.TestCase):
    def test_virtual_canvas_raster_parity(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / 'pose_rendering_host'
            subprocess.run(['c++', '-std=c++17', '-O2', '-ffunction-sections',
                '-fdata-sections', '-Isrc', 'tools/pose_rendering_host.cpp',
                'src/presentation/Canvas565.cpp', 'src/presentation/PixelRenderer.cpp',
                '-Wl,--gc-sections', '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)

if __name__ == '__main__':
    unittest.main()
