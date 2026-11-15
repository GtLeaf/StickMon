# AMOLED Emerald-style evolution

Approved reference: `.stick-local/artifacts/emerald-evolution-preview-20261115/`
overview.png and emerald-evolution.gif. This approval applies to evolution;
the earlier level-up/new-move mockups are not part of this implementation.

- Preserve the shared4000ms sequence,600ms cancellation hold,1000ms cancellation,
  cry timing,50ms animation frame interval, pause/input consumption, item/species
  persistence and full-page ready tap. No change to learning/replacement rules.
- Native368x448. Cold base RGB(9,30,43), original Emerald two-layer tilemaps and
 50palette rows, opposite4px camera offsets, portrait1.55x backdrop sampling.
  Envelope fades in y64..114 and out y246..292; no image or light cuts text.
- Primary32px centered title at y32;32px species name y296;16px cancel instruction
  y394. Header/footer stay free of animation effects. Warm ready palette uses
  existing UiTheme. Continue rectangle x24/y364/320x64/r22 is a visible entrance
  to the existing full-page tap; render pressed feedback where available.
- Both FRONT images share an upper1.5x base scale fitted inside244x168px and a
  fixed(184,184) visible-pixel center. Color and white silhouettes use the same
  geometry, nearest sampling and tint path. No decode-to-full-frame scratch.
-0..600ms original color;600..1000ms whitening/inward sparkles;1000..2840ms
  alternating white form growth/shrink over durations460/360/300/240/200/160/120ms,
  relative scale0.18..1 with cosine easing.2840..3180ms target white/outward
  sparkles;3180..3580ms reveal;3700..4000ms full-page fade to cream.
- Cancellation restores the source image and collapses light, then fades to
  cream over650..1000ms. CANCELLED shows original name and Continue. Finished
  evolution shows target name and Continue. No early species/item mutation.
- Hold progress x124/y376/120x4, adjacent to instruction; dirty ranges must
  include the bar while held and the full page during warm transitions. Preserve
  full/dirty/swap equivalence and caller clip/native-text state. Handle absent
  sprite resources without crashes; keep new graphics APIs opt-in.
- Precompute frame palette pairs and row-color lookups once per visual state,
  reuse across dirty bands. Draw only intersecting rows. Retain no Canvas/model
  pointers; no per-frame heap allocations, filesystem reads or re-decoding.
- Source reference files belong under origin_asset/evolution/emerald; generated
  packed tile/palette header comes from an offline Pillow converter. No network
  dependency in generation, tests or runtime. Preserve original source provenance.
- Keep existing pending AGENTS.md preference edit. No commit/push/flash. Build
  AMOLED V2 only; never build V1 without an explicit user request.

Approach: embed packed original tile data plus maps/palette (about13KiB), render
them through cached color lookups. A sequence of full-screen RGB frames would
cost substantially more storage; procedural replacement would diverge from the
approved original-art preview. A streaming tinted sprite renderer avoids adding
two mutable sprite rasters/full-screen scratch buffers.
