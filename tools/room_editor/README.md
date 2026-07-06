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
   Set `Surface` to `floor`, `left wall`, or `right wall`, and keep
   `Receive shadows` enabled only for the polygons that should catch shadows.
   For window-light rooms, trace the green wall panels separately from wood
   frames/windows if only the wall panels should receive cast shadows.
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
12. Import furniture PNG files. Each file opens a cutout dialog:
    - `Cancel` skips the current file and does not import it.
    - `Use original` keeps the image unchanged.
    - click either image with `Pick color` to sample the background color.
    - adjust `Remove range` to control how close a color must be to the picked
      color before it is removed.
    - adjust `Edge feather` to soften edge pixels.
    - `Cutout` removes the selected background color from the current after
      image; repeated cutouts combine, which is useful for cleaning green edge
      pixels with a second picked color.
    - `Magic wand` removes one connected region whose color is close to the
      clicked pixel.
    - `Erase brush` and `Restore brush` edit the after image manually, down to a
      one-pixel brush.
    - `Reset after` restores the after image back to the original.
    - `Use cutout` imports the edited after image.
13. Check `Asset management` for the current background, all furniture, and all
    sprite preview items. Use the row `Delete` button to remove imported
    furniture or sprite preview items.
14. Drag furniture on the canvas and edit transform values in the `Transform`
    group. Use `Object semantics` to set `Kind`, `Height`, `Footprint`, shadow,
    receiving, and sprite occlusion behavior. Rugs and floor decals should keep
    `Height = 0` and usually disable `Cast shadow`; sofas, beds, and cabinets
    should use furniture/high-furniture kinds with a non-zero height. Wall
    shelves should use the wall-furniture kind or set `Shadow anchor` to wall.
    Floor furniture with `Footprint = ellipse` uses an oval contact shadow
    instead of the raw alpha-mask projection, which avoids rectangular shadows
    for round beds and similar bulky items.
    Wall-mounted objects use a 2.5D wall-plane projection: the editor maps the
    receiving wall face into local coordinates, projects the item's alpha mask
    from its `Wall depth` toward the light's `Light depth`, then adds a soft
    contact shadow below the object.
15. Use `Project sprites` to add the firmware's current Pokemon frame as a
    scale/placement preview.
16. Use the collapsible `Light and shadows` panel on the right to tune the shared
    day/night light source. Toggle `Day` / `Night` in the stage toolbar to
    preview the result. Night adds the extra tint/darken overlay; both modes use
    the configured light and projected shadows:
   - drag the `Light` handle on the edit canvas to change `Light X/Y`.
   - `Light profile`: `Shared` edits one light used by both modes. `Day` and
     `Night` edit separate lights and automatically switch the preview to the
     matching mode.
   - `Copy to Day` / `Copy to Night`: duplicate the currently edited light into
     the selected day/night profile before fine tuning it.
   - `Light shape`: use `Radial` for local glow or `Cone` for window-like beams.
   - `Light depth`: distance from the wall plane used by wall-mounted shadow
     projection. Smaller values exaggerate cast shadows; larger values move
     toward a parallel-light look.
   - `Light radius`: visual radius for the canvas handle.
   - `Cone angle` and `Cone spread`: tune the direction and width of cone light.
   - `Cast 2D shadows`: draws flattened alpha-mask shadows for visible items
     whose item-level `Cast shadow` is enabled.
   - `Shadow mode`: `Auto` keeps radial lights as point lights and cone lights
     as directional/window lights; use `Point light` or `Directional` to force a
     specific shadow model.
   - Shadow receiving is clipped by room faces when `Receive shadows` is enabled:
     floor surfaces use flattened floor shadows. Left/right wall surfaces use
     wall-plane projection for wall-mounted objects and the flattened fallback
     shadow for ordinary floor furniture.
   - `Strength` changes light and shadow intensity. `100` matches the original
     full-strength value; values above `100` are overdrive for stronger window
     light.
   - `Light radius` and the cone shape limit which objects can cast a shadow. In
     directional shadow mode, moving the light handle changes the lit cone/region,
     while `Cone angle` controls the parallel shadow direction.
   - `Shadow opacity`, `Shadow length`, and `Shadow blur`: tune the fake floor
     projection. `Shadow blur` supports `0.1px` steps for small soft-shadow
     adjustments. Click the preview canvas to zoom it.
17. Use `Toolbox` / `Measure H` to drag a vertical measurement on the edit
    canvas. The readout shows both original canvas pixels and target game pixels.
18. Use the export buttons:
   - `Save session`: stores the current editor session in the browser and writes
     a local JSON backup. The first save asks for the export file location; later
     saves check permission and update the same file. Browsers without file
     handle support fall back to downloading a `stickmon_room_editor_session_*.json`
     backup. The backup includes the last imported file names, images,
     furniture, room geometry, and UI settings.
   - `Load session`: restores the browser-saved session.
   - `Clear saved`: removes the browser-saved session.
   - `Layout JSON`: can import normal room layout exports or downloaded session
     backup files.
   - `Export layout`: writes `room_layout.json`, including room geometry and
     furniture.
   - `Export room data`: writes `room_geometry.json`, containing only the
     wall/floor faces for firmware import.
   - `Export preview PNG`: writes a screenshot of the current day/night preview.

Face endpoints are stored in original reference-image pixels inside
`sourcePoints`. Exported `points` are scaled to game pixels. Default game target
size is `240x135`.

Browser security does not expose absolute local file paths from normal file
inputs. The session save stores file names plus image data and configuration, so
the editor can restore the previous room without reselecting files in the same
browser profile.

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
5. Use the cutout dialog when the furniture image has a solid-color background.
   Cancel skips the file; magic wand and brush tools can clean up the after
   image before import.
6. Drag furniture on the canvas and edit transform values in `Transform`.
   `Scale` applies uniform resizing; `W` and `H` can be locked or unlocked with
   `Lock aspect ratio`. Target width and height can be reduced to a minimum of
   `1px`. `Object semantics` exports `kind`, `heightPx`, `footprint`,
   `shadowAnchor`, `wallShadowDepthPx`, `wallShadowOffsetY`, `castsShadow`,
   `receivesShadow`, `occludesSprite`, and `sortY`; the older `furnitureType`
   field is still exported for compatibility.
7. Enable `Show 8px grid` and `Snap furniture to 8px grid` for pixel-perfect
   placement.
8. For batch day/night previews, run the fixed `compose_room.py` script in this
   directory.

## Asset Management

The right-side `Asset management` list shows:

- the active reference/background image, including source size, trim size, and
  opacity
- imported furniture PNGs, including semantic kind, height, placement, target
  size, scale, visibility, and whether a cutout was applied
- project sprite previews, including selected action/frame and placement

Click a furniture or sprite row to edit its transform and layer properties. Use
the row `Delete` button to remove an imported item from the layout.

## Large Source Images

The editor separates source image size from target canvas size:

- The base image can be larger than `240x135`.
- Preview always draws the full target room, then displays it with CSS at a
  maximum width of `240px`; height follows the room aspect ratio.
- The white/cyan frame in preview marks the `240x135` game screen range.
- When a reference/background image is loaded, preview uses that image as the
  background at the target canvas size. Room geometry is only used as the
  fallback background when no image is loaded.
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

Generate project sprite previews after sprite assets change. The script needs
Pillow:

```bash
python3 -m pip install pillow
python3 \
  ./tools/room_editor/generate_sprite_previews.py
```

The script writes:

- `origin_asset/generated/pokemon_sprites/*.png` (one PNG per species/frame)
- `origin_asset/generated/pokemon_preview_assets.js`

Open `index.html`, then use `Project sprites` to select a species, action, and
frame before clicking `Add sprite preview`. The added sprite appears in the
asset list and can be:

- Moved by dragging (sprite previews do not respond to arrow keys in Furniture
  mode).
- Deleted with the right-panel `Delete` button.
- Changed to a different species/action/frame from the properties panel.

It uses the current `Sprite X/Y/W/H` guide as its initial placement.

## Runtime Interpretation

The exported layout uses this model:

- `roomGeometry`: wall/floor polygons for the generated room background.
- `base`: reference image metadata for editor reloads.
- `furniture`: ordered by `z`, then composited over the base.
- `guides`: sprite and HUD safe zones for preview only.
- `night`: night overlay plus shared day/night light-source and projected-shadow
  parameters for preview and generated script.

Furniture should be generated as isolated PNGs. Do not rely on extracting
furniture from a complete AI room image as the long-term source of truth.

## Editor Interaction Notes

- The status bar at the bottom shows the current workflow step:
  1. Load a reference image.
  2. Trace wall/floor faces in Shape mode.
  3. Import furniture in Furniture mode.
  4. Add sprite previews, tune lighting/shadows, and export.
- Shape-only tools (`New face`, `Undo point`, `Clear draft`) are disabled in
  Furniture mode; furniture `Duplicate` / `Delete` are disabled in Shape mode.
- When a vertex is shared by multiple faces, the status bar warns that moving
  it will affect all connected faces.
- Importing a layout only updates furniture that is already loaded; missing
  filenames are reported in the status bar.

## Compose Script

The fixed `compose_room.py` expects Pillow:

```bash
python3 -m pip install pillow
python3 ./tools/room_editor/compose_room.py \
  --layout room_layout.json \
  --furniture-dir furniture \
  --out room_preview
```

Pass `--base empty_room.png` when the command-line preview should use a
background image. Without `--base`, `roomGeometry.faces` are used as the fallback
background.
Project sprite preview items are resolved from
`origin_asset/generated/pokemon_sprites` automatically.

It writes:

- `room_preview_day.png`
- `room_preview_night.png`

The script is intended as a preview compositor. A later firmware asset
generator can consume the same `room_layout.json` and PNG files.
