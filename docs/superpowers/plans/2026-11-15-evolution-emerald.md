# Emerald evolution implementation plan

> Execute inline with superpowers:executing-plans. User approved the preview and
> requested writing it into the project. No additional approval gate or commit.

**Goal:** Ship the approved Emerald-style evolution and cream result layout.
**Architecture:** Offline packed background data, streaming tinted FRONT scaling,
AMOLED presentation helper and existing progression controller with updated dirty
regions only. No rules/save changes.
**Tech Stack:** C++17, Canvas565, existing fonts/sprite packs, Python generator.
**Spec:** specs/evolution-emerald.md.

## Global constraints

4000ms/600ms hold/1000ms cancel/50ms frames and save/cry/input behavior preserved.
Only evolution changes. Preserve pending AGENTS.md. No V1 build/commit/push/flash.

## Review focus

- Transparency (including opaque black) and fractional nearest sampling match
  for raw565/indexed4; tint does not change sprite geometry or caller state.
- Source/target sizes and sparse/absent frames fit without clipping or stale
  pointers; no per-band allocations/decode or repeated palette preprocessing.
- Warm fades and hold progress repaint their whole changing area; stationary
  ready/cancelled pages stop animation work. Existing taps and held-release gates
  do not skip or commit twice.
- Cancellation restores original appearance before returning to normal flow;
  item/species state changes only on the existing confirmed-success path.
- Header/footer remain readable and free of flashes throughout transitions;
  full/dirty/swapped rendering is equivalent.

## Task1: Packed background and opt-in tinted sprite scaling

Files: origin_asset/evolution/emerald/*, tools/generate_evolution_background.py,
firmware/amoled_1_8_v1/main/ui/EvolutionBackgroundData.h;
src/presentation/TintedSpriteRle.h, src/assets/PokemonSprites.h/.cpp,
tools/generate_pokemon_sprites.py, tools/evolution_emerald_host.cpp,
tools/test_evolution_emerald.py.

- [x] Add native render/fit/tint/transparent/dirty/swap checks. Observe RED for
      missing tinted sprite API and old result appearance.
- [x] Add source provenance and deterministic offline packed-tile generator;
      ship generated data, about13KiB; check generated data against sources.
- [x] Implement `drawFrameTintedScaled(frame,x,y,scale,tint,amount,flipX)` as an
      opt-in streaming RLE path with no new allocations or bitmap caches. Test
      against independently decoded native color/mask images for fractional
      scales, both flips and tint endpoints/intermediates; reject invalid scale.

## Task2: Evolution rendering and lifecycle verification

Files: firmware/amoled_1_8_v1/main/ui/EvolutionPresentation.h/.cpp,
ui/sources.txt, ui/ProgressionScreens.cpp, app/ProgressionFlow.cpp;
tools/evolution_emerald_host.cpp, tools/test_evolution_emerald.py,
tools/amoled_page_controller_host.cpp, tools/growth_text_host.cpp.

- [x] Implement approved timing, portrait backdrop, white alternating forms,
      sparkles, reveal/cancellation, cream result text and Continue cue. Reuse
      per-state palette/row lookup; honor dirty clipping and byte order.
- [x] Extend controller dirty ranges for hold bar and warm fade while retaining
      existing50ms update gate and ready tap semantics. Verify actual App journey.
- [x] Compare six approved reference frames and normal animation captures; cover
      large species, full/dirty/swap/nativeText, cancellation and missing frames.
- [x] Run targeted progression/timeline/growth regressions and architecture/family
      checks; build V2 and inspect actual captures. No V1 build or flashing.
- [x] Request one final scoped read-only review, address blockers, record evidence
      in .stick-local/artifacts/emerald-evolution-live-20261115/ and update docs.

## Verification outcome

All scoped tasks complete. Evidence: .stick-local/artifacts/emerald-evolution-live-20261115/.
Native raw/indexed tint, transparency/black, nearest/flips, fractional scaling,
coordinate/layout/asset clip units, dirty bands, byte order, phases, cancellation,
large/missing resources pass. Actual App paths now run in50ms steps, verify
stationary results stop rendering and retain single confirmed save semantics.
Seven progression/timeline tests pass; growth catalog covers64 species and271 moves;
23 architecture/family tests pass with2 preexisting platform skips. V2 Lite Debug
build passes (0x247e40 bytes,24% app partition free). No V1 build or flashing.

RED evidence records missing tint API, old result palette, moved hold bar missing
from dirty rows, and logical/physical clip mismatch. All corrected checks pass.
Final independent read-only review found the clip-unit issue, confirmed its fix
and reported no remaining Critical/Important findings. Pending AGENTS.md diff
is preserved exactly. No commit, push, save-schema or game-rule changes.
