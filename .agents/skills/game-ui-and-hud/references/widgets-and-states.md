# Widgets, states and panel construction

## Required states

Every interactive element needs the full set. Missing states are the most common reason UI feels
unfinished:

| State | Trigger | Must differ by |
| --- | --- | --- |
| Default | Idle | - |
| Hover | Mouse over | Value or fill change, not just a tint |
| Focused | Controller/keyboard selection | Strong, unmistakable: outline + offset + scale or arrow |
| Pressed | Held down | Visible displacement (1-2 px down) and/or darker fill |
| Disabled | Unavailable | Lower contrast, desaturated, and a reason available on hover |
| Selected / active | Current tab, equipped item | Persistent marker distinct from focus |
| Error / warning | Invalid input | Icon + text, not colour alone |
| Loading | Async work | Spinner or progress, plus disabled input |

Hover and focus are different things. With a gamepad there is no hover, so focus must be legible
from across the room; with a mouse, focus may be invisible unless you support both inputs properly.

## Buttons

- Anatomy: background (9-slice), label, optional icon, optional shortcut hint, optional badge.
- Minimum size: 44 x 44 pt on iOS and 48 x 48 dp on Android for touch; for pixel UI, at least
  text height + 8 px of padding vertically.
- Label text: verbs, title or sentence case consistently, no ellipsis unless it opens a dialog.
- Icon-only buttons need a tooltip or label; icons alone are guessed wrong more often than designers
  expect.

## Bars (health, mana, XP, progress)

- Fill direction must be consistent across the game (left to right, bottom to top).
- Add segmentation ticks for countable resources so players can read exact values at a glance.
- Animate value changes over 100-250 ms with easing; instant jumps hide what happened.
- Use a "ghost" trailing bar in a lighter colour to show the damage just taken.
- Low state must be signalled by more than colour: pulsing, a shape change, a sound, or a number.
- Show numbers alongside bars whenever the player must make arithmetic decisions.

## 9-slice panels

A 9-slice (or 9-patch) splits a panel image into corners, edges and centre: corners stay fixed,
edges stretch or tile in one axis, the centre fills.

Rules for pixel art:

- Corner size must be at least the visual corner radius plus the border width.
- Edges should **tile** rather than stretch for patterned borders; stretching a 1 px pattern smears.
- Keep the slice boundaries on whole pixels and the panel size a multiple of the pixel scale.
- Design one base panel, then derive variants (tooltip, dialogue, modal) by colour and border weight
  rather than new art.
- Provide an inner padding value with the asset; UI built without documented padding drifts.

## Spacing scale

Pick one unit and multiply. For pixel UI a 4 px base works well: 4, 8, 12, 16, 24, 32. For scalable
UI use 8 px. Rules:

- Inner padding of a panel >= the gap between elements inside it.
- Group related items with a smaller gap; separate groups with 2-3x that gap.
- Align everything to the grid, including icon baselines and bar ends.
- Never mix two spacing systems in one screen; inconsistent spacing is what people read as "amateur".

## Lists, grids and inventories

- Row height must fit the largest content (icon + two text lines) and stay constant.
- Inventory grids: 1 slot = icon size + padding; show slot borders so empty space reads as capacity.
- Support keyboard/controller wrap behaviour explicitly (does moving right from the last column wrap
  to the next row, or stop?).
- Show sort and filter state persistently; players forget they applied a filter.
- Stack counts bottom-right of the slot, rarity as a border or background, equipped state as a
  distinct badge.

## Tooltips and item cards

Fixed order, always:

1. Name (+ rarity colour, with a shape/label backup)
2. Type / slot
3. Primary numbers (damage, armour, cost)
4. Modifiers, with deltas versus the currently equipped item (green up / red down plus arrows)
5. Flavour text, visually de-emphasised, last

Tooltips must never cover the thing they describe; flip side when near a screen edge. Delay 200-400
ms on hover for mouse, instant for controller focus.

## Dialogue boxes

- Reserve space for the longest expected line plus 20%; localisation grows text by 30-40%.
- Speaker name in a fixed position, portrait on a fixed side.
- Text speed configurable, with instant-complete on input and a visible "continue" indicator.
- Never reflow text as it types; lay out the full block, then reveal it.

## Modals and confirmations

- Dim the background 40-60% and disable underlying input.
- Destructive actions need an explicit confirm with the action named in the button ("Delete save"),
  not "OK".
- Default focus goes to the safe option.
- Always provide a cancel path that the back button reaches.

## Empty, loading and error states

Design these before shipping, not after QA finds them:

- Empty inventory, empty quest log, zero currency: explain how to get things, not just "Empty".
- Loading: something animates, and no input is silently swallowed.
- Errors: what happened, what the player can do, and whether progress was lost.
