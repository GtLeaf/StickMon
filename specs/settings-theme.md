# Settings grouped cards and shared theme

Approved by the user: implement the grouped-card preview and extract theme colors.

- AMOLED 368x448; preserve the shared header/back anchors.
- Cream page #f7ecd9, ink #59432e, title #805339, controls #ead7ba,
  card #fff6e7, accent #a85938, on-accent #fff7eb, slider track #e5d3b8.
- Slider card (20,84,328,176), two 88px rows. Labels and percentages above
  tracks; track endpoints x40/328, centers y148/236. Labels do not change values.
- Options card (20,272,328,104): speed and power-save rows, 52px each.
- Reset entry (20,388,328,44), with explicit confirmation. On builds supporting
  system update, divide this footer into two 158px entries with a 12px gap.
- Keep brightness32..255, volume0..100, four speeds, five timeouts, save timing,
  cancellation, lock lifecycle and update availability.
- Render and hit testing share geometry; press/release must belong to one control.
- Introduce one semantic RGB565 palette and common themed header/toast helpers.
  Settings and the cream exploration selector consume it, including disabled and
  scene overlay colors. Map art and sprite colors retain their original palettes.
- Palette replacement copies values; the caller requests a full redraw. No new
  skin picker, save fields, resource packs, display frame policy, or other-page redesign.
- Verify real application gestures, save/restore, reset protection, release/debug
  update availability, alternate palette rendering, clipped rows and byte order.

Approaches: grouped cards give the sliders a wide touch area and keep the reset
entry quiet (selected); separate capsules emphasize equal-sized rows but give
less space to the sliders. Runtime palette values enable later skins without
changing this page layout; compile-time-only colors would be less flexible.

Theme API: `firmware/amoled_1_8_v1/main/ui/UiTheme.h` defines the semantic
`Palette`, default `CREAM_APRICOT`, `current()`, `setPalette()` and `resetPalette()`.
Only the settings page and cream exploration selector consume this palette in
this change. Shared native16/32px text, header and toast helpers live in `ThemedUi.h`.

```cpp
auto palette=AmoledV1::UiTheme::CREAM_APRICOT;
palette.accent=AmoledV1::UiTheme::rgb565(176,114,67);
AmoledV1::UiTheme::setPalette(palette);
app.forceFullRender();
```

Verification: `python3 tools/test_settings_theme.py --output <artifact-directory>`;
add `--debug` for the update footer. These tests export actual native renderer
frames, check both pages under an alternate palette, and preserve reset/save gates.
