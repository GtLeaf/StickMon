---
name: game-ui-and-hud
description: Design game UI, HUDs, menus and interface art that players can read under pressure. Use whenever the user is building a HUD, health bar, minimap, inventory, dialogue box, shop, pause or settings menu, title screen, tooltip, damage number, cursor, button, icon set or any in-game interface, asks where HUD elements should go, asks why their UI looks amateur or cluttered, asks about diegetic versus non-diegetic UI, or needs fonts, icons, 9-slice frames, controller navigation, scaling across resolutions, or UI accessibility. Covers the diegetic, non-diegetic, spatial and meta taxonomy, HUD layout and screen-corner conventions, safe areas, information hierarchy, widget anatomy with all interaction states, 9-slice panels, bitmap and pixel fonts with legibility minimums, icon design at small sizes, menu structure and controller and keyboard navigation, UI motion and juice, resolution scaling strategies, and accessibility requirements including contrast ratios, colour-blind safety, text size and touch targets. Always asks about platform, input method, resolution range and what the player must read mid-action before proposing layouts.
version: 1.0.0
---

# Game UI and HUD

Game UI is read in fractions of a second, while the player is busy doing something else. That single
constraint separates it from app UI: hierarchy, contrast and position matter far more than beauty,
and anything the player must read during combat has to be legible at a glance, in peripheral vision.

## When to use

- Designing or reviewing a HUD: health, resources, ammo, timers, minimaps, objective markers.
- Building menus: title, pause, inventory, shop, settings, save/load, dialogue.
- Drawing UI art: panels, frames, buttons, icons, cursors, bars, fonts.
- The UI is described as cluttered, amateur, hard to read, or "placeholder-looking".
- Supporting controllers, keyboards, touch, or multiple resolutions.
- Accessibility work: contrast, colour-blind safety, text size, reduced motion.

Drawing craft for pixel UI art is in `../pixel-art-fundamentals/SKILL.md`; colour choices are in
`../color-and-palettes/references/readability-and-contrast.md`.

## Ask first

1. **Platform and screen** - PC, console on a TV, handheld, mobile? Viewing distance and safe areas
   differ.
2. **Input** - mouse, gamepad, keyboard, touch, or all of them? This decides navigation, focus
   states and hit-target sizes.
3. **Resolution range** - a fixed pixel resolution with integer scaling, or 720p to 4K?
4. **What must be read mid-action** versus what can wait for a pause? Only the first group belongs
   in the HUD.
5. **Genre conventions** - players arrive with expectations (health bottom-left in shooters,
   top-left in RPGs); break them only deliberately.
6. **Art direction** - pixel UI, clean vector, hand-painted, diegetic in-world panels?

## Core rules

1. **The HUD is the smallest set of elements the player cannot play without.** Everything else goes
   in a menu, a tooltip, or an on-demand overlay.
2. **Position by frequency and urgency.** Constantly-needed, urgent information goes near the action
   or in the corners the player already watches; rare information goes further away.
3. **Redundant encoding, always.** Never carry meaning in colour alone - add shape, icon, number,
   position or text.
4. **Contrast against the worst case.** UI must survive the brightest, busiest, most chaotic scene in
   the game. Test there, not on a grey background.
5. **Every interactive widget needs every state**: default, hover, focused, pressed, disabled,
   selected. Missing states are the top reason UI feels unfinished.
6. **Consistency beats novelty.** One panel style, one button style, one font pair, one corner
   radius, one spacing unit across the whole game.
7. **Diegetic UI is a design choice, not a quality upgrade.** It costs readability; use it when
   immersion is worth more than speed.

## Workflow

**Step 0 - inventory the information.** List every piece of data the player needs, with frequency,
   urgency, and whether it is needed during action. See `references/hud-layout.md`.

**Step 1 - choose the presentation class** for each item: diegetic, non-diegetic, spatial or meta.

**Step 2 - block out the layout** in grey boxes at the smallest supported resolution, inside the
   safe area. Do not draw art yet.

**Step 3 - test the blockout over real gameplay screenshots**, including the busiest scene.

**Step 4 - build the widget kit**: panel (9-slice), button, bar, slider, toggle, list row, tooltip,
   with all interaction states. See `references/widgets-and-states.md`.

**Step 5 - type and icons**: pick the font pair and sizes, draw the icon set on a fixed grid. See
   `references/typography-and-icons.md`.

**Step 6 - navigation**: define focus order, controller mapping, back/cancel behaviour, and default
   focus per screen. See `references/menus-and-navigation.md`.

**Step 7 - motion and juice**: transitions, bar tweens, hit feedback, notification timing.

**Step 8 - accessibility and scaling pass**: contrast ratios, colour-blind check, text size options,
   reduced motion, touch targets. See `references/accessibility.md`.

## References

| File | Read it for |
| --- | --- |
| `references/hud-layout.md` | The diegetic / non-diegetic / spatial / meta taxonomy with real examples, corner conventions by genre, safe areas, information inventory method, clutter reduction, minimaps and markers, damage numbers |
| `references/widgets-and-states.md` | Anatomy and required states for buttons, bars, sliders, toggles, lists, tooltips, dialogue boxes, inventory grids; 9-slice construction; spacing scale; empty and loading states |
| `references/typography-and-icons.md` | Bitmap vs vector fonts, minimum legible sizes, font pairing, number formatting, icon grids, silhouette-first icon design, currency and status icon sets |
| `references/menus-and-navigation.md` | Menu structure, focus order and default focus, controller and keyboard navigation, back/cancel rules, settings menu contents, UI motion timings, sound feedback |
| `references/accessibility.md` | WCAG contrast ratios applied to games, colour-blind design, text scaling, subtitles, remappable input, reduced motion and shake, difficulty and assist options |

## Companion skills

| Skill | Hand off when |
| --- | --- |
| `../pixel-art-fundamentals/SKILL.md` | Drawing pixel panels, frames, icons and cursors; keeping 1 px lines crisp |
| `../color-and-palettes/SKILL.md` | UI palette, status colours, contrast ratios, keeping UI off the environment palette |
| `../sprite-animation/SKILL.md` | Animated UI elements, idle motion on menus, icon animations |
| `../vfx-and-lighting-2d/SKILL.md` | Damage flashes, vignettes, low-health effects, screen-space feedback |
| `../game-art-pipeline/SKILL.md` | UI atlases, 9-slice export, scaling modes, font rendering and filtering settings |

## Failure modes

| Symptom | Real cause | Fix |
| --- | --- | --- |
| "UI looks amateur" | Inconsistent spacing, mixed fonts, missing states, default engine assets | One spacing scale, one font pair, full state kit, one panel style |
| HUD unreadable in fights | No contrast against bright/busy scenes | Add outlines or a dark scrim behind UI; reserve UI-only colours |
| Players miss low health | Colour-only signal at the screen edge | Add a vignette, a sound, a pulse, and a number |
| Cluttered screen | Everything is always on | Fade non-urgent elements, show on change, move detail to menus |
| Text illegible on TV or handheld | Font too small, outside safe area | Larger minimum sizes, keep UI inside ~90% of the screen |
| Controller navigation feels broken | No explicit focus order or default focus | Define focus per screen and make the focus state unmistakable |
| UI breaks at other resolutions | Absolute pixel positions | Anchor to corners/edges, scale by integer steps or use anchors + margins |
| Pixel UI looks blurry | Non-integer UI scaling or filtered fonts | Integer scale, point filtering, snap to whole pixels |
| Tooltips unreadable | Long prose, no hierarchy | Name, one-line effect, numbers in a fixed order |

## Answering style

- Propose the information inventory before any layout: what, how often, how urgent.
- Give concrete numbers: px sizes, spacing units, contrast ratios, safe-area percentages.
- Name the UI class (diegetic, non-diegetic, spatial, meta) when recommending a presentation.
- Always include the accessibility consequences of a choice, not as an afterthought.
- Suggest the cheapest fix first: outlines, scrims and spacing fix most "amateur" UI before new art.
