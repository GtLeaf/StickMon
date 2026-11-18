# Visit UI icons

`visit-icons.png` is the transparent source sheet generated with image_gen for the approved StickMon visit preview, 2026-11-17. Cells are door, pawprints, visitor with door, and warning, in row-major order.

Regenerate firmware data with `python3 tools/generate_visit_icons.py`; check with `python3 tools/generate_visit_icons.py --check`.

The encoder crops each cell to its opaque bounds, resamples with nearest-neighbor, maps to the fixed nine-entry UI palette (including transparency), then emits RLE. Native frame sizes: 48x48, 48x48, 96x64, 64x64. The renderer uses these assets directly on both boards; no new resource partition payload is needed. The generated payload is 2,810 bytes plus 50 bytes of palette/frame metadata.
