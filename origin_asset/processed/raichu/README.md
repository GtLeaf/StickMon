# 26 Raichu PMD processing spec

This spec is intentionally scoped to Raichu only.

## Source

- Input sheet: `origin_asset/source_sheets/low/026_raichu.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/raichu/`
- Format: RGBA PNG with transparent background.
- Canvas: 64x64 px.
- Target scale: nearest-neighbor 2x, capped to fit the canvas without clipping.
- Placement: horizontally centered, bottom aligned with a 4 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 1 frame, indices `0..0`.
- `walking`: 3 frames, indices `0..2`.

- `sleeping`: 2 non-directional frames, indices `0..1`.

## Directions

- Source row order: `front`, `down_right`, `right`, `up_right`, `back`, `up_left`, `left`, `down_left`.
- Exported direction order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.
- Mirrored directions: none; all 8 directions are cropped from source frames.

- Note: Walking uses 24 source sprites: row 1 sprites 1..21 plus row 2 sprites 1..3.
- Note: Source direction order is front, down_right, right, up_right, back, up_left, left, down_left.
- Note: This source sheet has no separate idle action; idle uses walking frame 0 for each direction.
- Note: Sleeping uses row 2 sprites counted from the end: 3 and 2.

## Contact sheet

- File: `raichu_idle_walking_contact.png`.
- Top-left label: output frame size, `frame 64x64 px`.
- Columns: exported directions in the order above.
- Rows: `idle_0`, `walking_0`, `walking_1`, `walking_2`, `sleeping_0`, `sleeping_1`.
- Sleeping rows are non-directional; the frame is shown in the first grid cell only.
- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.

## Project preview mapping

- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.
- `FRONT`: `walking/front_0.png`.
- `BACK`: `walking/back_0.png`.

## Runtime state machine

- Runtime owner: `src/scenes/MainScene.cpp`.
- Scope: only species `26` uses this Raichu state set.
- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.
- Direction input: the current AI velocity vector is mapped to the 8 generated directions.
- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.
- Sleeping behavior: sleeping frames ignore direction.
- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.
