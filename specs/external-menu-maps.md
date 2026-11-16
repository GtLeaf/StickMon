# External menu icons and map layouts (2026-11-15)

User approved migrating nine main-menu RGB565 RLE icons and the five exploration layout families to resource files, preserving pixels, map output, seed/random sequence and gameplay. Previous uncommitted font optimization remains intact. No V1/OSTB builds, flash, automatic commit or push.

Menu contract: `MenuAssets::begin()` preloads and validates `ui/menu.smonmenu` once; `mainIconFrames()`/`mainIconRle()` only access resident data. `STICKMON_EXTERNAL_MENU_ICONS` is enabled for V2 and SDGOODS; other builds retain embedded data. BOX icons and postcard art are outside scope.

Map contract: V2 enables only `STICKMON_EXTERNAL_MAP_LAYOUTS`, retaining all other static content. Tables 10..20 reuse the SDGOODS explicit little-endian field codecs. SMCD schema2 retains the 16-byte header (decoded payload length) and adds encoded length/decoded CRC32 followed by raw-DEFLATE fields. SMCD schema1 and SDGOODS's existing catalog remain supported. Preload all eleven tables before loading/saving game state, check geometry/index/count semantics, retain typed objects in PSRAM for process lifetime, release temporary serialized map bytes. No lazy file IO during generation, collision checks or rendering.

## Plan / verification

- [x] Icons: TDD checked binary format, all-frame parity and stable in-memory access, generator integration (independent task).
- [x] Maps: TDD schema2 ContentStore decode; selective compile flags/generator adapters; preload and semantic checks. Compare embedded/external explicit map fields across seeds, areas, edges, segment contexts and valley nodes; prove cached access after removing source files.
- [x] Stage only required resources for V2/Web and keep SDGOODS catalog flow. Add startup gates before game state mutation and preserve OTA maintenance/recovery access.
- [x] Build V2/SDGOODS Debug, inspect ELF symbols/removal, resource images and RAM budget, package firmware, run affected tests and independent review.

Review focus: bounds before allocation, strict stream completion and CRC, no partial table cache, no dangling serialized pointers, missing/invalid resources block gameplay, no embedded copies remaining in external target, no per-frame IO, old font work preserved.

## Format and generation

- `python3 tools/generate_menu_icon_pack.py`: generates `data/packs/dev/ui/menu.smonmenu`; `prepare_menu_assets.py` also refreshes the pack while preserving embedded-build guards.
- `python3 tools/generate_map_layout_resources.py`: generates only tables10..20 in `data/packs/dev/catalog/` from authoritative static C++ fields. Run after changing any of the five layout families. Existing header generators preserve the selective-map switches via `content_tables.adapt_header`.
- V2/Web stage exactly those eleven tables plus the menu pack. SDGOODS continues exporting its complete raw schema1 catalog and adds the menu pack. Other targets retain embedded data and Web staging excludes these new packs.
- New app and resources must be installed together. Missing/invalid V2 resources stop before `app.begin`; pending OTA reboots for rollback, normal boot offers the existing OTA maintenance entry. SDGOODS stops before session startup through its existing recovery path.

## Verification (2026-11-15)

- V2 application: 2392240 -> 2082816 bytes (309424 B / ~302 KiB saved). ELF inspection confirms no MAIN_ICON_RLE or large static layout arrays remain. Algorithm constants and postcard art remain embedded.
- Menu pack: 4949 B; eleven map packs: 36376 B. Actual V2 SPIFFS image:190 files,471127 B (~460 KiB) free, previously516809 B (~505 KiB). All image entries matched the staging directory.
- Typed map cache:290310 B; menu decoded allocation:24319 B including guard. Existing allocator PSRAM preference/fallback policy is unchanged. Map cache intentionally has process lifetime, matching font/icon caches, so global destructors never call a destroyed host allocator. Temporary serialized schema2 records are freed after decoding; legacy string tables retain their storage.
- 37 targeted regression tests passed. Map专项4项在ASan/UBSan下通过，逐字段比较所有11表及6256组生成请求、30000组地形语义查询；移走目录后缓存仍可生成一致结果。菜单7项及生成器3项通过，既有原生菜单像素回归通过。
- Existing SDGOODS full catalog test passed:49 tables identical after decode/re-encode,28 malformed cases rejected, resource-only species/tile extension preserved. Native Web now opens its staged virtual-root paths and runs with both external flags enabled; startup, framebuffer, clock, cancelled touch and scoped reset pass.
- Independent review found zero animation frames and empty exits were not semantically rejected. Valid-CRC failure fixtures were observed RED, then metadata/route bounds tightened and the full map suite passed GREEN. Another review finding about unnecessary Stick Web packs was fixed with target/path allowlists and an inventory test.
- Initial V2 integration exposed table-dependent constexpr/static_assert assumptions in ExploreTileSemantics; those now use the selective-map constexpr guard, while embedded producer assertions remain active. Sanitized host shutdown exposed a global-allocator lifetime issue, fixed by the intentional process-lifetime map store. No gameplay/random calls changed.
- Final V2 Lite Debug and SDGOODS Debug builds and official SDGOODS development package/inspection passed. No V1/OSTB firmware builds, hardware flashing or hardware latency/FPS acceptance performed. Existing font changes preserved. No remote push.
- Evidence: `.stick-local/artifacts/external-menu-maps-20261115/`; bundles: `.stick-local/firmware/external-menu-maps-debug-20261115/`. Python asset checks used the existing primary Python/Pillow13 runtime; the older local Pillow11.3 environment lacks `get_flattened_data` already used by the menu generator before this change.
