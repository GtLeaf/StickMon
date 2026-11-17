# Accessibility for game UI

Accessibility is not a separate feature set; most of it is ordinary good UI done deliberately. These
are the requirements worth treating as non-negotiable.

## Contrast

The WCAG thresholds are the practical reference:

| Content | Minimum contrast ratio |
| --- | --- |
| Body text | 4.5:1 against its background |
| Large text (about 18 pt+, or 14 pt bold) | 3:1 |
| UI components and meaningful graphics (icons, bar fills, focus rings) | 3:1 |

For games, apply them against the **worst-case** background, not a mock-up. Text over gameplay needs
an outline or a backing plate to guarantee the ratio, because the background is not under your
control. Check ratios with a contrast tool, not by eye.

## Colour vision deficiency

- Roughly 8% of men and 0.5% of women have some form of colour vision deficiency; red-green types
  (deuteranomaly most commonly) dominate.
- **Never encode meaning in colour alone.** Add a second channel: shape, icon, pattern, position,
  number, or text label. This applies to team colours, rarity tiers, status effects, damage types,
  faction markers and puzzle elements.
- Do not rely on red-versus-green pairs for critical distinctions (health/damage, valid/invalid,
  friend/foe).
- Vary lightness as well as hue between important colours: if two colours have the same lightness,
  they may be indistinguishable to some players and in greyscale.
- Test by converting screenshots to greyscale and through a CVD simulator. If information disappears,
  it was colour-only.
- Colour-blind "modes" that remap palettes are a bonus, not a substitute for redundant encoding.

## Text size and readability

- Provide a text-size option (at least 100% / 125% / 150%) for subtitles and UI text, and build
  layouts that grow rather than clip.
- Default body text: at least 16-18 px at 1080p on PC; larger for console/TV viewing distances.
- Use a legible font for body text, not a decorative one. Avoid thin weights and low-contrast greys.
- Left-align long text; centred paragraphs are harder to read.

## Subtitles and audio

- Subtitles on by default is increasingly standard. Include speaker names and a background plate
  with adjustable opacity.
- Caption meaningful non-speech audio (footsteps behind, door opening, enemy tell) for anything the
  player must react to.
- Provide separate volume sliders for master, music, SFX, voice and UI.
- Any critical audio cue needs a visual counterpart - a directional indicator, a flash, a HUD icon.

## Motion, shake and flashing

- Provide options to reduce or disable screen shake, camera sway, motion blur, and parallax.
- Avoid rapid full-screen flashing; repeated high-contrast flashes are a photosensitivity risk.
  Where flashes are part of the design, offer a reduced-flash option.
- Keep UI animation short and skippable; allow reduced motion to shorten transitions.

## Input

- Fully remappable controls, including for keyboard, mouse and every gamepad button.
- No mandatory button mashing or holds without an alternative (toggle instead of hold, tap instead
  of mash).
- Touch targets: at least 44 x 44 pt on iOS, 48 x 48 dp on Android, with spacing between them.
- Support both hold and toggle for aiming, sprinting, crouching and similar states.
- Avoid timing-critical multi-button inputs as the only way to progress.

## Assist and difficulty

- Separate the axes players actually struggle with: enemy damage, player health, timing windows,
  puzzle hints, platforming assists, aim assist.
- Allow changing them mid-game without restarting or losing achievements.
- Name options descriptively ("Reduce enemy damage"), not judgementally.

## Cognitive load

- Keep objectives visible or one button away; never rely on the player remembering instructions from
  10 minutes ago.
- Provide a glossary for game-specific terms and a controls reference at any time.
- Allow pausing anywhere it is technically possible, including during cutscenes and dialogue.
- Keep iconography and terminology consistent across the whole game.

## Accessibility checklist

1. All text meets 4.5:1 (3:1 for large text and UI components) against the worst-case background.
2. No information conveyed by colour alone, anywhere.
3. Greyscale screenshot test passes for every HUD state.
4. Text size option available, and layouts grow without clipping.
5. Subtitles with speaker names and an opacity setting.
6. Screen shake, flashing and motion can be reduced.
7. All inputs remappable; no mandatory mashing or holds.
8. Touch targets at least 44 pt / 48 dp.
9. Options are reachable from the pause menu, not just the title screen.
10. Every audio cue that affects gameplay has a visual equivalent.
