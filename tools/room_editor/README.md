# StickMon Room Layout Tool

Local browser tool for composing room geometry, room previews, and replaceable
furniture.

## Why Web

Use a static Web tool for this workflow:

- Drag/drop image import is built in.
- Canvas editing matches the target room coordinate system.
- Exporting `layout.json` and a quick preview PNG is simple.
- No desktop GUI framework or build step is required.

Open:

```bash
open ./tools/room_editor/index.html
```

## Files

- `index.html`: DOM structure only.
- `styles.css`: editor layout, panels, canvas, face list, and controls.
- `app.js`: image import, shape editing, furniture placement, preview, import,
  and export logic.
- `compose_room.py`: command-line day/night preview compositor.
- `generate_sprite_previews.py`: extracts current firmware sprite frames from
  `src/assets/PokemonSprites.cpp` into editor-loadable PNG previews.

## Recommended Asset Flow

1. Generate or prepare one reference room image.
2. Import the reference image.
3. Confirm the game preview size. `Room W` and `Room H` are target game pixels;
   keep `Lock room aspect ratio` enabled when changing only one dimension.
4. Adjust `Reference opacity` so the original-size image is useful as a tracing
   guide in the edit area.
5. Switch the editor to `Shape`.
6. Click points on the edit canvas to draw wall/floor outlines. The edit canvas
   uses the reference image's original pixel size.
7. Click the first point to close the shape into a face.
8. Select each closed face on the right and set its type:
   - `wall`
   - `floor`
9. Keep endpoints editable:
   - drag an endpoint in the edit canvas
   - select a point in the right panel and edit `X` / `Y`
   - use `Add point` to insert a point on the selected edge
   - use `Delete point` to remove the selected point
   - draft points that have not formed a face can also be dragged, edited, and
     deleted
   - when drawing a new face, click an existing endpoint to reuse it in the new
     face
10. Generate furniture as separate transparent PNG files.
11. Switch the editor to `Furniture`.
12. Import furniture PNG files.
13. Drag furniture on the canvas and edit `x`, `y`, `z`, `scale`, `slot`.
14. Use `Project sprites` to add the firmware's current Pokemon frame as a
    scale/placement preview.
15. Toggle day/night preview and tune night overlay values. Click the preview
    canvas to zoom it.
16. Use the export buttons:
   - `Export layout`: writes `room_layout.json`, including room geometry and
     furniture.
   - `Export room data`: writes `room_geometry.json`, containing only the
     wall/floor faces for firmware import.
   - `Export preview PNG`: writes a screenshot of the current day/night preview.

Face endpoints are stored in original reference-image pixels inside
`sourcePoints`. Exported `points` are scaled to game pixels. Default game target
size is `240x135`.

Closed faces and draft lines share a global endpoint pool in the editor. Moving
a reused endpoint updates every face or draft line that references it. Deleting a
point removes it from the selected face or draft only; the point remains if
another face still references it.

## Room Geometry

Closed line loops become faces. A face is exported as a polygon:

```json
{
  "id": "face1",
  "type": "floor",
  "drawOrder": 0,
  "points": [[6, 88], [120, 63], [234, 88], [234, 121]],
  "sourcePoints": [[24, 352], [480, 252], [936, 352], [936, 484]]
}
```

The firmware-side importer should interpret:

- `type = "wall"` as a wall polygon filled by the selected wall material.
- `type = "floor"` as a floor polygon filled by the selected floor material.
- `drawOrder` as the paint order within the room background.
- `points` as the firmware/game-coordinate polygon.
- `sourcePoints` as editor-only original-reference coordinates.

The reference image is not required at runtime. It is only a tracing source for
the editor.

## Furniture Flow

1. Generate furniture as separate transparent PNG files.
2. Open `index.html`.
3. Confirm `Source scale`. A `960x540` base for a `240x135` target should use
   `4x`; a `720x405` base should use `3x`.
4. Import furniture PNG files.
5. Drag furniture on the canvas and edit `x`, `y`, `z`, `scale`, `slot`.
6. For batch day/night previews, run the fixed `compose_room.py` script in this
   directory.

## Large Source Images

The editor separates source image size from target canvas size:

- The base image can be larger than `240x135`.
- Preview always draws into the target canvas.
- Furniture can also be generated larger.
- In the default furniture import mode, furniture display size is divided by
  `Source scale`.

Example:

- Target canvas: `240x135`
- AI base image: `960x540`
- `Source scale`: `4`
- Furniture source PNG: `128x96`
- Furniture target preview size: `32x24`

Use `Furniture import = Native pixels` only when the imported furniture PNG is
already authored at target pixel size.

## Project Sprite Preview

Generate project sprite previews after sprite assets change:

```bash
python3 \
  ./tools/room_editor/generate_sprite_previews.py
```

The script writes:

- `generated/pokemon_sprites/*.png`
- `generated/pokemon_preview_assets.js`

Open `index.html`, then use `Project sprites` -> `Add sprite preview`. The added
sprite is a normal draggable preview item with `scale`, `z`, and visibility
controls. It uses the current `Sprite X/Y/W/H` guide as its initial placement.

## Runtime Interpretation

The exported layout uses this model:

- `roomGeometry`: wall/floor polygons for the generated room background.
- `base`: reference image metadata for editor reloads.
- `furniture`: ordered by `z`, then composited over the base.
- `guides`: sprite and HUD safe zones for preview only.
- `night`: overlay parameters for preview and generated script.

Furniture should be generated as isolated PNGs. Do not rely on extracting
furniture from a complete AI room image as the long-term source of truth.

## Compose Script

The fixed `compose_room.py` expects Pillow:

```bash
python3 -m pip install pillow
python3 ./tools/room_editor/compose_room.py \
  --layout room_layout.json \
  --furniture-dir furniture \
  --out room_preview
```

For old layouts without `roomGeometry.faces`, also pass `--base empty_room.png`.

It writes:

- `room_preview_day.png`
- `room_preview_night.png`

The script is intended as a preview compositor. A later firmware asset
generator can consume the same `room_layout.json` and PNG files.
