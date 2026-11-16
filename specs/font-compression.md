# Lossless font resources (2026-11-15)

User approved per-target font selection and lossless compression. Keep all glyphs, dimensions, metrics, bitmaps and A4 coverage unchanged. V2 firmware excludes round-zh16-a4 and round-zh20-a4; SDGOODS retains them. Do not implement dynamic ASCII32 synthesis or reduce glyph coverage. No V1 or OSTB builds, flashing, publishing, automatic commit or push.

Retain SMFN v1 bitmap / v2 A4 headers and add flag 0x80000000 for raw-DEFLATE record payloads. A 12-byte extension after the 16-byte header stores decoded record bytes, encoded bytes and CRC32 of decoded records. Load directly into existing glyph storage, clear the transport flag before normal font validation, validate lengths and CRC and keep legacy uncompressed packs readable. Rendering and steady-state glyph lookup remain unchanged.

Whole-payload compression was selected over per-glyph compression because the existing runtime already loads whole fonts; whole-payload compression has better ratio without per-frame decode. Per-target selection avoids shipping unused round fonts on V2. Existing raw font readers/writers in tools must decode/re-encode the transport without changing glyphs.

## Plan (writing-plans / executing-plans / TDD)

- [x] Add host tests for every glyph in compressed and legacy fonts, known corruption/truncation/oversize failures, and observe compressed-format failure first.
- [x] Add lossless Python transport helper; implement bounded, CRC-checked record loading in FontResource and preserve old parsing checks. Update generators and font-consuming tools/tests.
- [x] Compress existing six fonts, prove decompressed files equal pre-change bytes, and select only V2 fonts in native/Web resource staging. Preserve SDGOODS fonts.
- [x] Run affected font/text/resource tests and V2/SDGOODS Debug builds, inspect actual staged fonts and SPIFFS capacity, package and independently review.

Review focus: decoded size checked before allocation; invalid flags/dimensions/count rejected; corrupted payload never exposed; all six font families preserve exact glyph bytes and widths; tooling idempotence; stale round files removed from V2 stage; SDGOODS keeps both A4 fonts.

## Verification and decisions

- All six decoded files match the pre-change bytes exactly (4020 glyph records). Global font files total 405888 -> 208302 bytes; V2 packages four fonts totaling 103966 bytes. SDGOODS retains all six, verified against its actual resource stage.
- V2's 4 MiB SPIFFS image has 178 files and 516809 bytes free by offline `SPIFFS_info` accounting, previously 212597 bytes after cry compression. Native and Web staging exclude exactly the two round A4 fonts.
- 58 targeted regression tests pass, including ASan/UBSan font/cry runtime cases, every glyph, legacy fonts, corruption, font consumers, Web, music and architecture. Actual text rendering checks pass for bitmap parity, dirty bands, byte order, clipping and typewriter layout.
- Independent review found that the legacy DEFLATE decoder could accept a full output buffer without checking the final marker. Reproduced with consistent encoded lengths, then fixed RED->GREEN via `inflateFileExact`: one guard byte, final-marker/EOF checks and exact encoded consumption. Existing callers keep legacy behavior; font loading uses strict mode. The expanded suite and both firmware builds passed after this fix.
- V2 Lite Debug and SDGOODS Debug build/package validation passed. No V1 or OSTB firmware build or hardware flashing was performed. Evidence: `.stick-local/artifacts/font-compression-20261115/`; bundles: `.stick-local/firmware/font-compression-debug-20261115/`.
- An isolated round-font generator trial reproduced all existing glyphs and also included the already-available source character `举` in its 16px output (1255 vs shipped 1254 glyphs). This task preserves the shipped glyph set; those regenerated files were not installed. The 20px trial matched exactly.
- New font resources require the updated application; install app and resources together. No remote push was performed.
