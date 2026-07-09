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

1. Generate or prepare a day room image and, optionally, a matching night room
   image.
2. Import `Day background`. Import `Night background` when you have a separate
   night image. Night preview falls back to the day image until the night image
   is loaded.
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
13. Check `Asset management` for the day/night background variants, all
    furniture, and all sprite preview items. Day and Night backgrounds share one
    trim, one coordinate system, room faces, and furniture layout. Use the row
    `Delete` button to remove imported furniture or sprite preview items.
14. Drag furniture on the canvas and edit transform values in the `Transform`
    group. Use `Object semantics` to set `Kind`, `Height`, `Footprint`, shadow,
    receiving, and sprite occlusion behavior. Rugs and floor decals should keep
    `Height = 0` and usually disable `Cast shadow`; sofas, beds, and cabinets
    should use furniture/high-furniture kinds with a non-zero height. Wall
    shelves should use the wall-furniture kind or set `Shadow anchor` to wall.
    Floor furniture with `Shadow polygon` uses the 2.5D caster projection:
    polygon vertices near the top of the sprite receive more height and are
    projected away from the light source, while bottom vertices stay close to
    the object. If no shadow polygon is set, `Footprint = ellipse` still falls
    back to an oval contact shadow to avoid rectangular shadows for round beds
    and similar bulky items.
    Wall-mounted objects use a 2.5D wall-plane projection: the editor maps the
    receiving wall face into local coordinates, projects the item's alpha mask
    from its `Wall depth` toward the light's `Light depth`, then adds a soft
    contact shadow below the object.
15. Use `Project sprites` to add the firmware's current Pokemon frame as a
    scale/placement preview.
16. Use the collapsible `Light and shadows` panel on the right to tune the
    active day/night light source. Toggle `Day` / `Night` in the stage toolbar to
    preview the result. Day uses `Day background`; Night uses `Night background`
    when loaded and otherwise falls back to the day image. Both modes use the
    configured light and projected shadows:
   - drag the `Light` handle on the edit canvas to change `Light X/Y`.
   - The right-side light controls follow `Preview Day` / `Preview Night`
     directly. Day edits the day light; Night edits the night light.
   - `复制到白天` is shown in Day mode, and `复制到夜晚` is shown in Night mode.
   - `Light shape`: use `Radial` for local glow or `Cone` for window-like beams.
     The shape controls the lit region only; it does not force shadow direction.
   - `Light depth`: 2.5D height of the light source. Smaller values exaggerate
     cast shadows; larger values move toward a parallel-light look.
   - `Light radius`: visual radius for the canvas handle.
   - `Cone angle` and `Cone spread`: tune the direction and width of cone light.
   - `Cast 2D shadows`: draws 2.5D projected shadows for visible items whose
     item-level `Cast shadow` is enabled.
   - `Shadow mode`: `Auto` uses the light handle as a point light. Use
     `Directional` only when you intentionally want `Cone angle` to control a
     parallel shadow direction.
   - Shadow receiving is clipped by room faces when `Receive shadows` is enabled:
     floor surfaces use 2.5D floor projection when `Shadow polygon` is present.
     Left/right wall surfaces use wall-plane projection for wall-mounted objects
     and clipped 2.5D caster projection for ordinary floor furniture.
   - `Strength` changes light and shadow intensity. `100` matches the original
     full-strength value; values above `100` are overdrive for stronger window
     light.
   - `Light radius` and the cone shape limit which objects can cast a shadow. In
     directional shadow mode, moving the light handle changes the lit cone/region,
     while `Cone angle` controls the parallel shadow direction.
   - Furniture-level `Opacity`, `Length`, and `Blur` in each item's `Shadow`
     section tune that specific object's projected shadow. `Blur` supports
     `0.1px` steps for small soft-shadow adjustments. Click the preview canvas
     to zoom it.
17. Use `Toolbox` / `Measure H` to drag a vertical measurement on the edit
    canvas. The readout shows both original canvas pixels and target game pixels.
18. Use the export buttons:
   - `Save session`: stores the current editor session in the browser and writes
     `sessions/stickmon_room_editor_session.json` under the selected
     `tools/room_editor` folder. Browsers without directory handle support fall
     back to downloading a `stickmon_room_editor_session_*.json` backup. The
     backup includes the last imported file names, images, furniture, room
     geometry, and UI settings.
   - `Load session`: restores the browser-saved session.
   - `Clear saved`: removes the browser-saved session.
   - `Layout JSON`: can import normal room layout exports or downloaded session
     backup files.
   - `Export layout JSON`: writes `room_layout.json`, including room geometry,
     furniture, guides, and lighting. Treat this as the single source of truth.
   - `Export preview PNG`: writes a screenshot of the current day/night preview.
     Project sprite preview frames are embedded as data URLs so this also works
     when the editor is opened directly as a local file. If export still fails,
     reload the editor and re-import any path-based external images.
   - `Add to Library`: on a selected annotated furniture item, writes that item
     directly into `generated/furniture_library` under the selected
     `tools/room_editor` folder, and regenerates `generated/furniture_library_assets.js`.
   - `Delete` in the Furniture library list removes that item from the room
     editor furniture library. Already placed scene items remain in the room and
     are detached from the removed `libraryId`.
   - `Change tool folder`: reselects the `tools/room_editor` folder used by
     `Save session`, `Add to Library`, and library `Delete`.
   - `Export furniture bundle`: writes the currently annotated furniture as a
     portable fallback bundle for browsers that cannot use local directory
     handles.

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
   `shadowOpacity`, `shadowLength`, `shadowBlur`, `receivesShadow`,
   `occludesSprite`, and `sortY`; the older `furnitureType`
   field is still exported for compatibility.
7. Enable `Show 8px grid` and `Snap furniture to 8px grid` for pixel-perfect
   placement.
8. For batch day/night previews, run the fixed `compose_room.py` script in this
   directory.

## Asset Management

The right-side `Asset management` list shows:

- the day/night background variants, including source size, shared trim size,
  active preview mode, and opacity
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
- When a background image is loaded, preview uses the current Day/Night variant
  as the background at the target canvas size. Room geometry is only used as the
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

- `tools/room_editor/generated/pokemon_sprites/*.png` (one PNG per species/frame)
- `tools/room_editor/generated/pokemon_preview_assets.js`

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
- `backgrounds`: day/night background metadata plus the shared trim/fit
  transform.
- `base`: primary background metadata retained for old tooling compatibility.
- `furniture`: ordered by `z`, then composited over the base.
- `guides`: sprite and HUD safe zones for preview only.
- `night`: shared day/night light-source and projected-shadow parameters for
  preview and generated script.

The previous standalone `room_geometry.json` export is now folded into
`room_layout.json` as `roomGeometry`.

Furniture should be generated as isolated PNGs. Do not rely on extracting
furniture from a complete AI room image as the long-term source of truth.

## Furniture Library

Annotated furniture can be promoted into a reusable library:

1. Select or configure furniture in the scene.
2. Set its semantic data: preset/category, height, shadow anchor, footprint
   polygon, and shadow polygon as needed.
3. Use `Add to Library` from the selected furniture's `Library` section. The
   first write asks for the `tools/room_editor` folder; later writes reuse the
   stored directory handle and update the tool files directly.
4. Use `Delete` in the library list to remove an item from the reusable library.
   This rewrites the manifest and editor preview data, then removes the item's
   JSON and image file from the tool-local library. Placed instances stay in the
   current scene as local furniture.
5. Use `Export furniture bundle` only as a fallback or batch handoff.

The tool-local library lives in `tools/room_editor/generated/furniture_library/`.
Direct writes also regenerate `tools/room_editor/generated/furniture_library_assets.js`
with embedded image data so the editor can browse the library and export preview
PNGs even when opened through `file://`.

Room layouts store `libraryId` for library furniture, while keeping a snapshot
of placement and annotation fields. If the library is present when importing a
layout, missing furniture images can be restored from the library automatically.

## Editor Interaction Notes

- The status bar at the bottom shows the current workflow step:
  1. Load a day or night background image.
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

Pass `--day-base empty_room_day.png --night-base empty_room_night.png` when the
command-line preview should use separate day/night background images. `--base`
still works as a shared fallback for old layouts. Without a background path,
`roomGeometry.faces` are used as the fallback background.
Project sprite preview items are resolved from
`tools/room_editor/generated/pokemon_sprites` automatically.

It writes:

- `room_preview_day.png`
- `room_preview_night.png`

The script is intended as a preview compositor. A later firmware asset
generator can consume the same `room_layout.json` and PNG files.
