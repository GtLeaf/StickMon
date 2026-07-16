# 362 Glalie PMD processing spec

This spec is intentionally scoped to Glalie only.

## Source

- Input sheet: `origin_asset/source_sheets/medium/362_glalie.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/362_glalie/`
- Format: RGBA PNG with transparent background.
- Canvas: 64x72 px.
- Target scale: nearest-neighbor 2x, fixed without per-frame fit scaling.
- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 1 frame, indices `0..0`.
- `walking`: 1 frame, indices `0..0`.

- `sleeping`: 2 non-directional frames, indices `0..1`.

## Directions

- Source row order: `left`, `up_left`, `front`, `down_left`, `back`.
- Exported direction order: `front`, `down_left`, `left`, `up_left`, `back`.
- Mirrored directions: right-side directions are drawn by runtime mirroring; only source direction PNGs are exported.

- Note: The left three columns contain visually identical moving frames; only frame 0 is retained for each direction.
- Note: Source row order is left, up_left, front, down_left, back.
- Note: This sheet has no separate idle action; idle uses moving frame 0 for each direction.
- Note: Sleeping uses the final two sprites on row 1.
- Note: Only five source directions are exported; right-side directions are mirrored at runtime.
- Note: A 64x72 canvas preserves fixed 2x scaling for the tallest moving pose.
- Note: Idle and walking remain separate logical actions, but their identical RLE payload is deduplicated when packed.

## Contact sheet

- File: `glalie_idle_walking_contact.png`.
- Top-left label: output frame size, `frame 64x72 px`.
- Columns: exported directions in the order above.
- Rows: `idle_0`, `walking_0`, `sleeping_0`, `sleeping_1`.
- Sleeping rows are non-directional; the frame is shown in the first grid cell only.
- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.

## Project preview mapping

- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.
- `FRONT`: `walking/front_0.png`.
- `BACK`: `walking/back_0.png`.

## Runtime state machine

- Runtime owner: `src/scenes/MainScene.cpp`.
- Scope: only species `362` uses this Glalie state set.
- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.
- Direction input: the current AI velocity vector is mapped to the 8 generated directions.
- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.
- Sleeping behavior: sleeping frames ignore direction.
- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.
