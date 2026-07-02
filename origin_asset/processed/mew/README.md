# 151 Mew PMD processing spec

This spec is intentionally scoped to Mew only.

## Source

- Input sheet: `origin_asset/source_sheets/medium/151_mew.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/mew/`
- Format: RGBA PNG with transparent background.
- Canvas: 64x64 px.
- Target scale: nearest-neighbor 2x, capped to fit the canvas without clipping.
- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 3 frames, indices `0..2`; all 8 directions are source frames, no mirroring.
- `walking`: 2 frames, indices `0..1`; source directions are `front`, `down_left`, `left`, `up_left`, `back`.
- `walking` mirrored directions: `up_right` from `up_left`, `right` from `left`, `down_right` from `down_left`.
- `sleeping`: 2 non-directional frames, indices `0..1`, sourced from the first two sprites on row 6.

## Directions

- Runtime direction order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.
- Idle source row 1: `front`, `down_right`, `right`, `up_right`.
- Idle source row 2: `back`, `up_left`, `left`, `down_left`.

## Contact sheet

- File: `mew_idle_walking_contact.png`.
- Top-left label: output frame size, `frame 64x64 px`.
- Rows: `walking_0`, `walking_1`, `idle_0`, `idle_1`, `idle_2`, `sleeping_0`, `sleeping_1`.

## Project preview mapping

- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.
- `FRONT`: `walking/front_0.png`.
- `BACK`: `walking/back_0.png`.
