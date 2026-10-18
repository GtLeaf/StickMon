# Tasks

Pair roaming refinement (2026-10-17):

- [x] Replace compact loops with safe destination walks and recent-stop avoidance. (17)
- [x] Preserve actual follower trails, waits, reactions and pause during replanning. (16, 17)
- [x] Verify movement range, constrained habitats, actual dirty frames and builds. (4-6, 17)

Roaming validation (2026-10-17):

- Added 72 open/passage/dead-end/narrow-loop scenarios across both pairs.
  In 120 active seconds each visited at least 24 distinct cells and spanned
  at least five tiles. Verified the young stays on the adult's traversed trail,
  no immediate reversal into the young, and no entry into cul-de-sac branches.
  Tiny four-cell pockets and dead-end-only corridors fall back to individuals.
- Existing 96 synthetic pairs, 64 real-map pairs, 768 generated maps and gull
  scenarios passed habitat, spacing, continuity, pause and secrecy checks.
- Focused native ecology rendering passed, including 864 real-app pair dirty
  frames compared with complete redraws, land/water presentation, player
  reactions and gameplay state/random isolation. Exported roaming previews
  were visually checked with a stationary camera.
- Architecture (12) and exploration screen (18) checks passed. StickS3
  release and AMOLED V1/V2 Lite builds passed; diff whitespace check passed.
- No firmware flashed; device FPS and V1/V2 behavior remain unverified.

Adult/young pairs (2026-10-17):

- [x] Select eligible first-batch pairs without increasing density or revealing species. (15)
- [x] Add safe shared trails, delayed following, waits, reunions and player response. (16)
- [x] Verify real maps, pause, rendered/dirty frames, gameplay isolation and builds. (4-6, 15-16)

Pair validation (2026-10-17):

- Ecology simulation passed existing 768 generated maps and new pair cases:
  96 synthetic pairs across land/water for 120 active seconds each, plus 64
  real-map pairs for 60 seconds each. Checked deterministic selection, the
  eligibility frequency, both hidden masks, missing members, six-entry pool
  density, fragmented-habitat fallback, following/waiting/reunions, player
  approach, safe spacing/movement, offscreen progress and pause/resume.
- Focused AMOLED ecology suite passed, including 972 new real-app dirty/full
  frame comparisons across both pairs on land and water, with no gameplay
  state, encounter history or gameplay random changes. Existing six-area,
  cover, observation, episode and gull flight rendering checks also passed.
- All 12 architecture and 18 exploration screen checks passed. M5StickS3
  release and AMOLED V1/V2 Lite builds passed; diff whitespace check passed.
- Exported actual-map pair frames and animated previews were visually checked
  for natural sprite sizes, ground anchors, cover and water effects.
- No firmware flashed; hardware FPS and V1/V2 device behavior remain unverified.

- [x] Implement deterministic safe spawning and ambient behavior. (1, 2, 3, 6)
- [x] Connect map lifetime, pause handling, cache use and local redraw. (4, 5)
- [x] Add depth-sorted wildlife and hidden-species grass movement. (3, 5)
- [x] Validate simulation, full/partial frames and affected builds. (1-6)

Validation (2026-10-16):

- `python3 tools/test_explore_ecology.py`: passed across 128 generated maps.
- `python3 tools/test_amoled_native_render.py`: all 46 tests passed, including
  26 ecology partial-frame comparisons, pause/camera checks, overlapping actors,
  hidden/revealed species and gameplay random/state isolation.
- Architecture boundaries (12), render demand (2) and explore pool (1) checks
  passed.
- M5StickS3 release and AMOLED V1/V2 Lite builds passed.
- Exported grass-path, hidden and revealed previews visually checked.
- No firmware flashed. Hardware FPS and V1/V2 device behavior remain unverified.

All-area extension:

- [x] Classify safe vegetation, rock, ice and horizontal water habitats. (1)
- [x] Respect species habitat and preserve the existing grass behavior. (1, 2, 6)
- [x] Render habitat-specific unknown-species effects and water ripples. (3, 5)
- [x] Validate all areas, routes, cave levels, partial frames and firmware builds. (1-6)

Extension validation (2026-10-16):

- Simulation passed on 768 generated maps (128 per area), varying routes,
  entry edges, pool rotations and frost cave levels. All maps populated;
  671 supported at least three actors. Covered 143 water, 480 ice and 312 rock
  actors, plus fish-only habitat and hazardous tile rejection checks.
- All 46 AMOLED native tests passed. Each area verifies ecology partial frames,
  pause, camera persistence, state/random isolation, and full/segmented/local
  redraw equivalence for all four surfaces, hidden/revealed and all effect frames.
- All 12 architecture and 18 AMOLED exploration screen checks passed.
- M5StickS3 release and AMOLED V1/V2 Lite builds passed.
- Six-area screenshots visually checked. No firmware flashed; hardware FPS
  and V1/V2 device behavior remain unverified.

First behavior batch (2026-10-17):

- [x] Add temperament-driven episodes, pacing, safe return and player reactions. (7, 8)
- [x] Add sleep/idle frame selection, lift, cover cropping and small effects. (7, 9)
- [x] Verify all phases, phase pause/resume, scheduling and rare-species secrecy. (7-9)
- [x] Verify dirty frames, visually review behavior previews and build all targets. (5, 9)

Behavior validation (2026-10-17):

- Simulation passed on 768 generated maps, with synthetic scenarios covering all
  20 phases, pause/resume at each phase, safe return, episode scheduling,
  cooldowns and unknown rare-species secrecy.
- All 47 AMOLED native tests passed. The final focused behavior run verified
  1,133 episode partial frames against full redraws, plus all six real areas,
  pause/camera persistence and gameplay state/random isolation.
- All 12 architecture and 18 AMOLED exploration screen checks passed.
- M5StickS3 release and AMOLED V1/V2 Lite builds passed.
- Sleep/wake, approach, cover/peek/emerge, flight and water-jump previews use
  existing assets. Sprite anchoring, cover cropping, shadows and small effects
  were visually reviewed; export samples retain the 200 ms simulation cadence.
- No firmware flashed. Hardware FPS and V1/V2 device behavior remain unverified.

Cover and locomotion refinement (2026-10-17):

- [x] Restore real grass/tree/shrub/rock cover across all areas. (10)
- [x] Choose safe hide/peek positions and remove floor-sinking concealment. (10)
- [x] Separate grounded/flapping/floating locomotion and preserve wing timing. (11)
- [x] Verify habitat, visible flight motion, pause, occlusion/dirty frames and builds. (4-6, 10-11)

Refinement validation (2026-10-17):

- All 768 real generated maps passed deterministic habitat/movement and pause
  checks, including object footprints. Added six flapping species and three
  ground bird species on open/isolated habitats; every airborne tick moves,
  wing timing remains continuous across states/pause, and rare actors stay
  hidden. Open ground never permits hiding; supported cover enables safe peeks.
- All 47 AMOLED native tests passed. Six-area occlusion checks compare exact
  opaque/transparent prop pixels before/behind their shared depth boundary;
  dense grass preserves gaps between blades. Compared 1,440 cover-edge ground/
  air partial frames plus 1,563 episode partial frames against full redraws.
- All 12 architecture and 18 AMOLED exploration screen checks passed.
- Final M5StickS3 release and AMOLED V1/V2 Lite builds passed.
- Exported tree/rock/shrub/grass versus open-ground screenshots and continuous
  flapping/return frames were visually checked. Existing packs were reused.
- No firmware flashed; device FPS and V1/V2 behavior remain unverified.

Observation pacing refinement (2026-10-17):

- [x] Reduce typical population and spread the occasional third actor. (1, 12)
- [x] Prefer movable habitat; shorten quiet waits and visibly recover blocked walks. (12)
- [x] Verify stationary observation using rendered pixels, density, isolation and builds. (4-6, 12)

Observation validation (2026-10-17):

- All 768 generated maps passed habitat, movement, pause, secrecy and episode
  checks; 750 maps supported two or three actors, and the others supported one.
  Third-actor home bounds span at least nine tiles horizontally or eight
  vertically. A separate 120-second stationary-observer sample on the same
  map count averaged 2.13 actors per map, with the first body/turn change
  within 2.0 seconds and longest unchanged body pose of 4.5 seconds. These
  timing measurements exclude hidden actors and idle frame counters.
- All 47 AMOLED native tests passed, including 12 open/isolated habitat cases
  observed for 90 seconds each with a fixed camera, fixed map frame and
  offscreen player. Actual rendered pixels first changed within 1.8 seconds;
  the longest identical frame lasted 3.0 seconds. Existing cover checks and
  1,998 episode partial-frame comparisons also passed.
- All 12 architecture and 18 AMOLED exploration screen checks passed.
- M5StickS3 release and AMOLED V1/V2 Lite builds passed. Exported six-area
  screenshots and the isolated Pidgey turn sequence were visually checked.
- No firmware flashed; device FPS and V1/V2 behavior remain unverified.

Wide flight (2026-10-17):

- [x] Cache flight surfaces/regions and add distant, varied gull routes. (13)
- [x] Add continuous turns, edge departure, absence and reentry. (13, 14)
- [x] Verify shadows, cover, partial redraw, pause/secrecy and all builds. (4-6, 13, 14)

Wide-flight validation (2026-10-17):

- Existing 768-map ecology simulation passed. Added 64 gull scenarios covering
  open terrain, a wall with a passage, enclosed terrain and isolated habitat.
  Open/passage cases visited at least 66 distinct tiles in 240 active seconds.
  Checked water/road crossing, explicit terrain semantics, departure/reentry,
  12-25 second absence, offscreen progress, phase pause and hidden/revealed art.
- Added 192 real generated maps across all areas, with 180-second checks for
  movement, wall/region boundaries and escape from the original home radius.
- All 47 AMOLED native tests passed. Added 771 gull partial/full comparisons,
  eight complete-sprite departures and eight edge reentries across both gull
  species and two cameras. An app-level run compared 440 actual dirty frames;
  415 absent ticks caused no redraw or gameplay state/random changes. Existing
  observation, local flight and cover checks also passed.
- All 12 architecture and 18 AMOLED exploration screen checks passed.
- M5StickS3 release and AMOLED V1/V2 Lite builds passed. Exported Wingull
  roaming frames were visually checked; existing sprite packs were reused.
- No firmware flashed; device FPS and V1/V2 behavior remain unverified.


## Ordinary and high-grass feedback (2026-10-17)

- [x] Add shared movement contacts, per-cell rebound and pause lifecycle. (18, 19)
- [x] Reuse grass artwork with matching map/body-cover deformation. (18, 19)
- [x] Connect party/wildlife shadow anchors and local dirty rectangles. (18, 19)
- [x] Verify stationary, departure, paired contacts, flight and deterministic state. (18)
- [x] Finish native rendering/idle/pause checks, visual review and affected builds. (19)

Device animation smoothness and V1/V2 display behavior require hardware review;
no firmware flashing is part of this change.


Grass validation (2026-10-17):

- Shared grass simulation passed movement thresholds, standing, post-departure
  rebound, top/base anchoring, pause/resume, cooldown, flying exclusion,
  relocation, map reset, paired contacts and deterministic five-slot checks.
- All 47 AMOLED native tests passed, including 216 new ordinary/high-grass
  pair frames at two camera positions with exact partial/full pixel equality
  and settled/static equivalence. App checks also passed grass-only dirty
  frames, stationary completion with no redraw, pause/resume, party flight
  exclusion and gameplay state/random isolation.
- Existing ecology simulation, 12 architecture and 18 exploration-screen
  checks passed. StickS3 release and AMOLED V1/V2 Lite builds passed.
- Ordinary/high-grass pair previews were exported and visually checked;
  existing assets were reused. No hardware validation or flashing performed.


## More frequent adult/young pairs (2026-10-18)

- [x] Increase eligibility from 25% to 50%, allowing either member of each
  existing family in the active pool. Preserve hidden-member exclusion and
  safe shared trails; keep encounter pools/history unchanged. (15, 16)
- [x] Share the family table with route-entry sprite preloading/pinning. (6, 15)
- [x] Add adult-only, young-only and both-member frequency/safety/secrecy cases. (15)
- [x] Verify actual pair dirty frames and affected firmware builds. (4-6, 15)


Pair-frequency validation (2026-10-18):

- For each family, adult-only, young-only and both-member pools produced
  505 groups in 1024 suitable-map seeds each (49.3%). All three inputs gave
  identical eligibility, with exactly two actors, safe movement and unchanged
  pool bytes. Unrelated pools and every explicit hidden-member mask were
  rejected. Real-map single-member eligibility matched both-member inputs.
- Existing group range tests passed 112 terrain/seed scenarios with at least
  24 cells visited in 120 seconds; 120 synthetic and 120 real-map groups
  passed movement, pause, secrecy and density checks. The full 768-map ecology
  simulation plus flight/episode/cover checks passed.
- Focused AMOLED ecology tests passed, including 1890 actual app dirty/full
  comparisons across both families and adult-only/young-only/both pools.
  Encounter history, game state and gameplay RNG remained unchanged.
- Architecture (12) and exploration screen (18) checks passed; StickS3 release
  and AMOLED V1/V2 Lite builds passed. Whitespace check passed.
- No firmware flashing or hardware validation performed.
