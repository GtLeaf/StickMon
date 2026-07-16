# 148 Dragonair PMD processing spec

This spec is intentionally scoped to Dragonair only.

## Source

- Input sheet: `origin_asset/source_sheets/high/148_dragonair.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/148_dragonair/`
- Format: RGBA PNG with transparent background.
- Canvas: 88x82 px.
- Target scale: nearest-neighbor 2x, fixed without per-frame fit scaling.
- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 2 frames, indices `0..1`.
- `walking`: 1 frame, indices `0..0`.

- `sleeping`: 2 non-directional frames, indices `0..1`.

## Directions

- Source row order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.
- Exported direction order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.
- Mirrored directions: none; all 8 directions are cropped from source frames.

- Note: Walking uses the sheet's single Special Attack & Moving frame for each direction.
- Note: Frames use a fixed 2x source scale on an 88x82 canvas so wide moving poses are not shrunk to fit 64x64.

## Contact sheet

- File: `dragonair_idle_walking_contact.png`.
- Top-left label: output frame size, `frame 88x82 px`.
- Columns: exported directions in the order above.
- Rows: `idle_0`, `idle_1`, `walking_0`, `sleeping_0`, `sleeping_1`.
- Sleeping rows are non-directional; the frame is shown in the first grid cell only.
- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.

## Project preview mapping

- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.
- `FRONT`: `walking/front_0.png`.
- `BACK`: `walking/back_0.png`.

## Runtime state machine

- Runtime owner: `src/scenes/MainScene.cpp`.
- Scope: only species `148` uses this Dragonair state set.
- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.
- Direction input: the current AI velocity vector is mapped to the 8 generated directions.
- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.
- Sleeping behavior: sleeping frames ignore direction.
- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.
