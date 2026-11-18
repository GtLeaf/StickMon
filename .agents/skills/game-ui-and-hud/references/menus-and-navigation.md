# Menus, navigation and UI motion

## Menu structure

Keep the tree shallow. Three levels is comfortable, four is the limit:

```
Title
  Continue / New game / Load
  Options
    Gameplay | Video | Audio | Controls | Accessibility | Language
  Extras (credits, gallery)
  Quit
```

Rules:

- The first item is the one most players want (Continue, not New game).
- Pause menu order: Resume, Options, Restart/Checkpoint, Quit - destructive items last and confirmed.
- Group settings by what the player is trying to fix, not by engine subsystem.
- Show current values next to every setting; a slider with no number is guesswork.
- Apply changes immediately where safe, with a clear Apply/Revert pair where not.

## Focus and navigation

For controller and keyboard support, every screen needs:

1. **A default focused element** on open - never open a menu with nothing focused.
2. **An explicit focus order** (up/down/left/right neighbours per element), not an automatic guess.
3. **A visible focus state** far stronger than a mouse hover: outline, offset, scale, arrow, or a
   moving selector sprite.
4. **Wrap rules** defined: does the list wrap at the ends? Does moving right from the last column
   jump to the next row?
5. **Consistent confirm/cancel**: south button confirms, east button cancels on Xbox layouts;
   Nintendo's default is mirrored, so read the platform convention and allow a swap option.
6. **Back always works** from every screen, including the deepest submenu, and never exits the game
   without confirmation.
7. **Shoulder buttons for tabs** - it is the expected pattern for category switching.

Also support mouse and keyboard properly if the game is on PC: Escape closes, Enter confirms, Tab
moves focus, scroll wheel scrolls, and clicking outside a modal does not silently discard input.

## Scrolling lists

- Keep the focused item away from the edges: scroll when the selection reaches 1-2 items from the
  end of the visible range.
- Show a scrollbar or an explicit "more below" indicator. Players miss content that silently
  continues off-screen.
- Support page jumps (shoulder buttons, PageUp/PageDown) for long lists.
- Preserve scroll position and selection when returning from a submenu.

## UI motion

| Transition | Duration | Easing |
| --- | --- | --- |
| Button hover/focus | 60-120 ms | Ease out |
| Button press | 40-80 ms | Linear or ease in |
| Panel open/close | 150-250 ms | Ease out / ease in |
| Screen transition | 200-350 ms | Ease in-out |
| Value tween (bar, counter) | 150-300 ms | Ease out |
| Toast / notification | 200 ms in, 2-4 s hold, 300 ms out | Ease out / ease in |

Rules:

- Motion communicates relationship: panels should come from where they conceptually live.
- Never block input during a transition; allow the next input to skip it.
- Anything longer than 400 ms on a frequently-used element becomes an annoyance within an hour.
- Provide a reduced-motion option that shortens or removes transitions
  (`references/accessibility.md`).
- Pixel UI: animate in whole-pixel steps; sub-pixel tweens shimmer.

## Sound feedback

- Four sounds cover most UI: move focus, confirm, cancel, error. Add one for "unavailable".
- Keep them short (under 150 ms) and quieter than gameplay audio; UI sounds fire hundreds of times
  per session.
- Randomise pitch slightly on repeated navigation sounds to avoid fatigue.
- Every visual state change that matters should have an audio counterpart for players who are not
  looking directly at it.

## Loading and transitions between game states

- Show progress or at least motion; a frozen screen reads as a crash.
- Use loading screens for tips, lore, or control reminders - but keep them skippable if loading is
  already finished.
- Preserve player context: after loading, tell them where they are and what the objective is.

## First-time experience

- Introduce UI elements as they become relevant, not all at once.
- Highlight a new element the first time it appears (pulse, arrow, or a brief callout).
- Keep a permanent reference (controls screen, glossary) so the tutorial is not the only source of
  truth.

## Menu checklist

1. Default focus set on every screen.
2. Focus state visible from 2 metres on a TV.
3. Back/cancel works everywhere and never quits without confirmation.
4. All settings show their current value.
5. Destructive actions confirmed with a named button.
6. Navigation works with gamepad, keyboard and mouse if all are supported.
7. Every transition skippable; nothing blocks input longer than 400 ms.
8. Selection and scroll position preserved across submenu returns.
