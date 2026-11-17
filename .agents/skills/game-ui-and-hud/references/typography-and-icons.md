# Typography and icons for games

## Choosing fonts

| Type | Best for | Watch out |
| --- | --- | --- |
| Bitmap / pixel font | Pixel-art games, fixed resolutions | Only legible at its native size and integer multiples |
| Vector (TTF/OTF) | Scalable UI, long text, localisation | Needs hinting and good rendering; can look wrong in pixel games |
| Display / decorative | Titles, logos, headers only | Never for body text, numbers or subtitles |

A two-font system is enough for almost every game: one display font for titles and one highly
legible font for everything else. A third font is usually a mistake.

Legibility minimums:

- Pixel fonts: cap height of 5-7 px works for short labels; use 7-9 px for anything the player must
  read under pressure. Never scale a pixel font by non-integer factors.
- Vector body text: at least 16-18 px at 1080p on PC; console text should be bigger still because of
  viewing distance - many teams target the equivalent of ~24 px at 1080p for TV.
- Line length 45-75 characters; line height 1.3-1.5x font size.
- Avoid all-caps for anything longer than a couple of words; it slows reading.

## Numbers

- Use tabular (monospaced) figures for anything that changes: timers, scores, health, currency. Non-
  tabular digits make values jitter.
- Fix the digit count or the alignment so a value going from 9 to 10 does not shift the layout.
- Abbreviate consistently (1.2K, 3.4M) and document rounding.
- Right-align numeric columns; left-align text columns.

## Text over gameplay

- Outline (1-2 px dark) plus a soft shadow, or a solid/semi-solid backing plate.
- Never rely on the background staying dark - gameplay changes.
- Keep floating combat text short: numbers and one word maximum.

## Localisation-safe layout

- Assume 30-40% text expansion from English into German or Russian; some strings double.
- Never bake text into images; keep it as a string with a separate asset.
- Allow text boxes to grow vertically rather than clipping, and test with the longest string.
- CJK needs different font files and a larger minimum size for legibility; Arabic and Hebrew need
  right-to-left support and mirrored layouts.
- Avoid concatenating sentences from fragments; grammar breaks in other languages.

## Icon design

- Draw on a fixed grid: 16x16 or 32x32 for pixel UI; 24 px on a 24 px grid for scalable UI.
- **Silhouette first.** An icon must be identifiable as a black shape. Test it filled solid.
- One visual metaphor per icon, one style rule set for the whole set: same stroke weight, same
  corner treatment, same perspective (all flat, or all 3/4), same optical weight.
- Leave 1-2 px padding inside the grid so icons do not touch each other when packed.
- Do not encode meaning in colour alone; the same icon in red and green is unreadable to many
  players (`references/accessibility.md`).
- At 16x16, drop all internal detail. A sword is a blade and a guard; a potion is a bottle and a
  liquid line. Anything more becomes mud.

## Icon sets you will need

- Resources: health, mana, stamina, currency (one per currency), ammo per type.
- Status effects: poison, burn, freeze, stun, buff, debuff - each with a unique silhouette.
- Equipment slots and item categories.
- Input prompts: one set per platform (Xbox, PlayStation, Switch, keyboard/mouse), swapped at
  runtime by detected input device.
- System: settings, close, back, confirm, warning, lock, new/unread badge.

## Input prompt glyphs

- Show the glyph for the device currently in use, and switch live when the player changes device.
- Never hardcode "Press A" in text; use a token that resolves to a glyph plus a label.
- Keyboard prompts must reflect remapped bindings, not defaults.

## Cursors and pointers

- Hotspot at the top-left tip for arrows, centred for crosshairs; document it.
- Provide a contrasting outline so the cursor survives any background.
- Distinct cursors for states (default, interact, attack, invalid) beat colour-only changes.

## Typography checklist

1. Two fonts maximum, one of them highly legible.
2. Tabular figures for all changing numbers.
3. Outlines or plates behind all text over gameplay.
4. Body text at least 16-18 px at 1080p, larger for TV.
5. Longest localised string tested in every box.
6. Icons readable as solid silhouettes at their smallest display size.
7. Input glyphs match the active device and current bindings.
