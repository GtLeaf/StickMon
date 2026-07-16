# 195 Quagsire PMD processing spec

This spec is intentionally scoped to Quagsire only.

## Source

- Input sheet: `origin_asset/source_sheets/medium/194_wooper_195_quagsire.png`
- Background removal: pixels matching the top-left source color become transparent.
- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.

## Output frames

- Output root: `origin_asset/processed/quagsire/`
- Format: RGBA PNG with transparent background.
- Canvas: 68x72 px.
- Target scale: nearest-neighbor 2x, fixed without per-frame fit scaling.
- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.
- Naming: `{action}/{direction}_{frame_index}.png`.

## Actions

- `idle`: 1 frame, indices `0..0`.
- `walking`: 2 frames, indices `0..1`.

- `sleeping`: 2 non-directional frames, indices `0..1`.

- `attack`: 2 frames per direction; source directions are `front`, `down_left`, `left`, `up_left`, `back`. Extracted for future use and not wired into the current runtime state machine.

## Directions

- Source row order: `front`, `down_left`, `left`, `up_left`, `back`.
- Exported direction order: `front`, `down_left`, `left`, `up_left`, `back`.
- Mirrored directions: right-side directions are drawn by runtime mirroring; only source direction PNGs are exported.

- Note: Row 2 contains ten moving frames: five directions, two frames each.
- Note: Idle uses attack frame 0 from row 1 for each direction.
- Note: Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.
- Note: Row 1 contains ten attack frames: five directions, two frames each; these are reserved for future use.
- Note: Only five source directions are exported; right-side directions are mirrored at runtime.
- Note: A 68x72 canvas preserves fixed 2x scaling for the widest sleeping pose and tallest attack pose.

## Contact sheet

- File: `quagsire_idle_walking_contact.png`.
- File: `quagsire_attack_contact.png`.
- Top-left label: output frame size, `frame 68x72 px`.
- Columns: exported directions in the order above.
- Rows: `idle_0`, `walking_0`, `walking_1`, `sleeping_0`, `sleeping_1`.
- Sleeping rows are non-directional; the frame is shown in the first grid cell only.
- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.

## Project preview mapping

- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.
- `FRONT`: `walking/front_0.png`.
- `BACK`: `walking/back_0.png`.

## Runtime state machine

- Runtime owner: `src/scenes/MainScene.cpp`.
- Scope: only species `195` uses this Quagsire state set.
- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.
- Direction input: the current AI velocity vector is mapped to the 8 generated directions.
- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.
- Sleeping behavior: sleeping frames ignore direction.
- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.
