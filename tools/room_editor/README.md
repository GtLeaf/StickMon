# StickMon Room Layout Tool

Local browser tool for composing room backgrounds and replaceable furniture.

## Why Web

Use a static Web tool for this workflow:

- Drag/drop image import is built in.
- Canvas preview matches the target `240x135` coordinate system.
- Exporting `layout.json` and a quick preview PNG is simple.
- No desktop GUI framework or build step is required.

Open:

```bash
open ./tools/room_editor/index.html
```

## Recommended Asset Flow

1. Generate or prepare one empty room base image.
2. Generate furniture as separate transparent PNG files.
3. Open `index.html`.
4. Import the base image. Large sources are sampled to the target canvas.
5. Confirm `Source scale`. A `960x540` base for a `240x135` target should use
   `4x`; a `720x405` base should use `3x`.
6. Import furniture PNG files.
7. Drag furniture on the canvas and edit `x`, `y`, `z`, `scale`, `slot`.
8. Toggle day/night preview and tune night overlay values.
9. Use the export buttons:
   - `Export layout`: writes `room_layout.json`, the source of truth.
   - `Export preview PNG`: writes a screenshot of the current day/night preview.
10. For batch day/night previews, run the fixed `compose_room.py` script in this
    directory.

Coordinates are saved in target pixels. Default target size is `240x135`.

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

## Runtime Interpretation

The exported layout uses this model:

- `base`: fixed empty room image.
- `furniture`: ordered by `z`, then composited over the base.
- `guides`: sprite and HUD safe zones for preview only.
- `night`: overlay parameters for preview and generated script.

Furniture should be generated as isolated PNGs. Do not rely on extracting
furniture from a complete AI room image as the long-term source of truth.

## Export Buttons

The editor intentionally keeps only two export buttons:

- `Export layout`: required. Save this after each layout change.
- `Export preview PNG`: optional. Use it for a quick visual check of the
  currently selected `Day` or `Night` mode.

There is no `Export script` button. The script is project tooling and should be
versioned as a normal file instead of downloaded from the browser.

## Compose Script

The fixed `compose_room.py` expects Pillow:

```bash
python3 -m pip install pillow
python3 ./tools/room_editor/compose_room.py \
  --layout room_layout.json \
  --base empty_room.png \
  --furniture-dir furniture \
  --out room_preview
```

It writes:

- `room_preview_day.png`
- `room_preview_night.png`

The script is intended as a preview compositor. A later firmware asset
generator can consume the same `room_layout.json` and PNG files.
