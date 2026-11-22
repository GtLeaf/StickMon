# Original SpriteCollab animation timing

Approved scope: restore the pinned Idle/Walk/Sleep durations of the 29 expansion
species, on Waveshare V2 and SDGOODS. Keep original images, registration, motion
scripts, render cadence and legacy species behavior. No new pack encoding,
deduplication, save schema or V1/OSTB builds.

## Shared contract

`game/PokemonAnimationTiming.h` provides `Action::{Idle,Walk,Sleep}`,
`hasTiming(speciesId)`, `frameAt(speciesId, action, elapsedMs, fallback=0)` and
an optional per-actor `Clock`. `Clock::update(speciesId, action, nowMs,
active=true)` returns the selected frame; `reset()` restarts its lifecycle.
Species/action changes restart at frame zero; direction is deliberately absent.
Paused calls freeze the phase; first resumed call excludes the paused interval.
Use an existing pause-adjusted elapsed clock directly where available.

The generator emits bounded arrays of cumulative ticks in original 1/60-second
units. Sampling uses 64-bit integer rational time, with no rounded per-frame
interval accumulation and no allocation or elapsed-time catch-up loops.

## Work packages

1. Generate timing data from pinned XML (including CopyOf resolution); add
   shared sampler and tests against XML, exact boundaries, wrap, pause and
   action reset. Regenerate and verify image packs remain byte-identical.
2. Integrate V2 home leader/companion and sleeping paths, retaining dirty rows
   and legacy breathing. Verify actual presenter/app behavior.
3. Integrate SDGOODS home leader/visitor paths; retain existing movement and
   pause semantics. Verify actual scene behavior.
4. Integrate shared exploration ecology and both route renderers; eliminate
   fixed six/two-frame truncation only for expansion species. Keep gameplay,
   random draws and legacy visual behavior unchanged.
5. Integrate market per-actor clocks and performance-local tutor animation;
   preserve choreography and Delibird's existing market NPC asset mapping.
6. Run related native tests and V2/SDGOODS SDK builds. Review timing integration
   independently and fix findings. Export old/new Idle/Walk/Sleep comparisons
   for Heracross 214, Linoone 264 and Duskull 355 from actual packed pixels.

## Acceptance

- All 29 species use every source frame for its original duration (display
  sampling can skip sub-refresh-duration frames; refresh frequency is unchanged).
- Direction changes preserve phase; action/species changes restart; paused
  animations do not advance; large elapsed gaps are bounded work.
- No sprite resizing, canvas/anchor changes, pose dedup or legacy animation
  retiming. No changes to progression, movement speed or save content.
- Host checks and authorized SDK builds pass; device smoothness remains a
  hardware acceptance item. Artifacts go under `.stick-local/artifacts/`.

## Implementation notes

- Generated data contains 392 cumulative frame ends across 87 timelines;
  compact timing tables occupy 1,190 bytes, with no sprite-pack format change.
- Wildlife keeps separate behavior/effect and artwork frames so water ripples,
  grass effects and sleep lift retain their existing choreography.
- V2 lock-screen breathing remains on its existing independent clock. Reading
  a sleep pose does not advance the new actor clock.
- Tutor scripts retain authored static/impact poses; their 16 cycling windows
  use Walk durations measured from each window's start.
- `test_pokemon_animation_timing.py`, `test_amoled_home_animation_timing.py`,
  `test_sdgoods_home_animation_timing.py`, `test_market_tutor_timing.py`, and
  the new ecology/route cases cover source timing and caller integration.
- Acceptance evidence and comparisons are under
  `.stick-local/artifacts/animation-timing-20261122/`. Tests do not depend on
  these local acceptance snapshots. No hardware flashing is part of this work.
