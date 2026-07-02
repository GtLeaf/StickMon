# 129 Magikarp PMD processing spec

This spec is intentionally scoped to Magikarp only.

## Source

- Input sheet: `origin_asset/source_sheets/high/129_magikarp.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/magikarp/`
- Format: RGBA PNG with transparent background.
- Canvas: 64x64 px.
- Target scale: nearest-neighbor 2x, capped to fit the canvas without clipping.
- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 2 frames, indices `0..1`.
- `walking`: 1 frame, indices `0..0`.

- `sleeping`: 2 non-directional frames, indices `0..1`.

## Directions

- Source row order: `front`, `down_right`, `right`, `up_right`, `back`, `up_left`, `left`, `down_left`.
- Exported direction order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.
- Mirrored directions: none; all 8 directions are cropped from source frames.

- Note: `helpless flopping` is stored as `walking` so the runtime movement state can reuse it.
- Note: This sheet has no dedicated sleeping action; sleeping uses the two front idle frames as a non-directional fallback.
- Note: Runtime movement speed is reduced for this species to make the flopping movement slow.

## Contact sheet

- File: `magikarp_idle_walking_contact.png`.
- Top-left label: output frame size, `frame 64x64 px`.
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
- Scope: only species `129` uses this Magikarp state set.
- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.
- Direction input: the current AI velocity vector is mapped to the 8 generated directions.
- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.
- Sleeping behavior: sleeping frames ignore direction.
- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.
