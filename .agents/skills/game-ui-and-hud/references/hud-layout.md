# HUD layout and information design

## The four UI classes

A widely used taxonomy splits game interfaces along two axes: whether the element exists in the
game's fiction (narrative), and whether it exists in the game's 3D/2D world space (spatial).

| Class | In the fiction? | In the world? | Examples |
| --- | --- | --- | --- |
| **Non-diegetic** | No | No | Classic health bars, ammo counters, minimaps, score. Screen-space overlay |
| **Diegetic** | Yes | Yes | Dead Space's health spine on the character's suit; in-world radios, watches, gauges |
| **Spatial** | No | Yes | Floor markers, objective outlines, enemy highlights, waypoint beams drawn in the world |
| **Meta** | Yes | No | Blood splatter on the screen, cracked-visor damage, rain on the camera lens |

Notes from practice:

- Diegetic UI is not automatically better. Dead Space's holographic inventory is the famous example
  of immersion costing usability; the same game's floor locator line is spatial and works brilliantly
  because it is instantly readable.
- Most shipped games mix classes. A common good pattern: spatial for navigation, non-diegetic for
  numbers the player must read fast, meta for damage and status, diegetic for flavour.
- If information is urgent, prefer non-diegetic or meta. If it is ambient, diegetic or spatial can
  carry it.

## Information inventory

Before layout, tabulate the data:

| Information | Frequency | Urgency | Needed during action? | Class | Placement |
| --- | --- | --- | --- | --- | --- |
| Health | Constant | Critical | Yes | Non-diegetic + meta | Fixed corner + vignette |
| Ammo / resource | Constant | High | Yes | Non-diegetic | Near health or near the crosshair |
| Cooldowns | Constant | High | Yes | Non-diegetic | Bottom centre, close to the action |
| Objective | Occasional | Medium | Sometimes | Spatial + non-diegetic | Marker + small text |
| Map | Occasional | Low | Rarely | Non-diegetic | Corner, or full-screen on demand |
| Currency / XP | Rare | Low | No | Non-diegetic | Fade in on change only |
| Inventory | Rare | Low | No | Non-diegetic | Dedicated screen |

Anything with "Needed during action: No" should not be permanently on screen.

## Corner conventions

Players carry expectations from the genres they play. Following them is free usability:

| Genre | Health | Resource / ammo | Map | Abilities |
| --- | --- | --- | --- | --- |
| Shooter | Bottom-left or centre | Bottom-right | Top-right (small) | Bottom-centre |
| Action RPG | Top-left | Top-left, under health | Top-right | Bottom-centre bar |
| Platformer | Top-left | Top-left | None | Rarely any |
| Strategy | Top bar | Top bar | Bottom-left | Bottom-right panel |
| Fighting | Top corners per player | Bottom bars | None | None |
| Survival | Bottom-left cluster | Bottom-right | Corner or key-toggled | Radial menu |

Centre-screen space is reserved for gameplay. Put only transient, high-urgency messages there
("WAVE INCOMING", level names, death messages).

## Safe areas and screen edges

- On TVs, keep all critical UI inside roughly the central 90% of the screen (about 5% margin per
  edge) so overscan and bezels cannot cut it off. Many console certification requirements are
  stricter for text.
- On handhelds and phones, respect notches, rounded corners and gesture bars; do not place
  interactive elements in the bottom gesture strip.
- Anchor UI clusters to corners and edges rather than absolute coordinates, so different aspect
  ratios (16:9, 16:10, 21:9, 4:3) reflow rather than break.
- Ultra-wide: keep HUD clusters near the centre-ish area or anchored to edges with a maximum offset,
  otherwise the player must physically turn their head.

## Reducing clutter

In order of effectiveness:

1. **Remove**: does the player ever act on this number? If not, delete it.
2. **Show on change**: currency, XP and pickups can fade in for 2-3 seconds and then disappear.
3. **Show on demand**: a held key or button reveals the full map, stats, or objective list.
4. **Merge**: combine several indicators into one widget (health + shield as one segmented bar).
5. **Contextualise**: show ammo only when a weapon is drawn; show stamina only when it is not full.
6. **Shrink and mute**: reduce opacity and size of non-urgent elements during action.

## Making UI survive a busy scene

- Give every UI element a 1-2 px dark outline, or place it on a subtle dark scrim (20-40% black).
- Reserve 2-3 colours for UI only, never used in the environment
  (`../../color-and-palettes/references/palette-construction.md`).
- Test over the brightest scene, the darkest scene, and the most saturated effect in the game.
- Text over gameplay needs an outline *and* a shadow, or a solid backing plate.

## Minimaps and markers

- Decide between a rotating (player-forward) and fixed-north map; rotating helps navigation, fixed
  helps memorisation.
- Marker priority: objective > threats > loot > flavour. Cap the number of simultaneous markers and
  cluster offscreen ones at the screen edge with a count.
- Off-screen indicators should show direction and distance, not just presence.
- Spatial markers drawn in the world beat corner maps for immediate navigation, but need occlusion
  handling so they do not float through walls confusingly.

## Damage numbers and floating text

- Randomise position slightly and stagger timing so numbers do not stack into an unreadable column.
- Encode type by colour *and* size *and* shape: crits larger with a different outline, healing with
  a plus sign.
- Lifetime 0.5-1.0 s, rise and fade, ease out.
- Cap the number of simultaneous labels; aggregate the rest ("x12").

## HUD review checklist

1. Can a new player find health, resources and the objective in under 2 seconds?
2. Is every permanent element needed during action?
3. Does everything critical sit inside the safe area at the smallest supported resolution?
4. Is every state distinguishable without colour?
5. Does the HUD stay readable over the busiest scene in the game?
6. Does the layout survive 4:3 and 21:9?
7. Can the player hide the HUD for screenshots without breaking the game?
