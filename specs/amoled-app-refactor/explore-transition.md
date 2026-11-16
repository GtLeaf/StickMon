# Explore transition flow extraction

Approved scope: first batch of the firmware maintainability proposal (2026-11-16).
Baseline: b5c99c5. Targets: AMOLED V2 Lite Debug; SDGOODS shared-source regression.

## Contract

Move departure/return phases, request data, door anchors, visibility and movement
orchestration from AmoledApp to app/ExploreTransitionFlow. Keep GameState and
Home::Actor/Simulation authoritative. Keep gameplay settlement, destination startup,
resource lifetime, music, persistence, camera and sprite animation in App.
The flow has no App, platform, storage or resource-singleton dependency. Inputs are
room geometry, current actors/game and home/clock observations. Outputs are bounded,
one-shot effects. It allocates no heap. SceneFade retains its presentation-acknowledged
clock; markRendered forwards the successful presentation notification.

Black-frame boundaries require explicit handshakes: start destination only after
fade-out presentation; settle return before initializing return actors; ask App for
legacy random clearance fallback only if deterministic candidates fail. Repeated
updates while awaiting acknowledgement never repeat these effects. Preserve the
existing RNG order, fade endpoints, speeds (30/42), 120 ms movement clamp, 8 s return
fallbacks, 140 ms walking frames and 700 ms idle deadlines. Do not add departure
timeouts or change cancellation semantics in this behavior-preserving extraction.

Retain separate app session-active state: it describes gameplay, not the transient
visual flow. Existing direct defeat return clears only transition visibility.
Do not expose writable flow state; adapt test fixtures to a test-only friend when
seeding private states is necessary. Replace obsolete location-specific tests with
behavior checks and dependency-direction checks.

## Delivery steps

1. Capture existing native debug regression and new transition trace baseline.
2. Add standalone host cases: serial departure, presentation fence, one-shot load
   and settlement, cancellation, failed load, malformed geometry, healthy/fainted
   actors, clearance fallback, timeout and uint32 wrap. Confirm missing module RED.
3. Implement Flow and App adapter, update authoritative source manifest and fixtures.
4. Run isolated ASan/UBSan cases and full native regression, market lock/save/source
   checks, V2 Debug build (SDGOODS build if shared production sources change).
5. Fresh independent review, fix material findings, document evidence and limits.

No resource/schema/partition changes; no automatic V1/OSTB builds, flashing, push,
or environment publication. Work remains reviewable and uncommitted for the user.

## Verification (2026-11-16)

- `AmoledApp.cpp`: 10,329 -> 9,736 lines. Flow owns phase/request/anchor/visibility
  state; App retains external effects. Sources come from the existing shared manifest.
- Frozen production baseline `b5c99c5`: 2,264 tick/presentation records (actor pose,
  tasks, frames, animation clocks, camera, visibility, scene and framebuffer hash)
  match exactly. Portable regression fixture:
  `tools/fixtures/amoled_explore_transition_baseline.json`.
- Old expedition fixture entered the area list without selecting a destination,
  accidentally retaining the default MARKET selection. Both baseline and new host
  explicitly select grass through the production selector before the touch sequence.
- Isolated ASan/UBSan host: serial departure, failed load, missing doorway,
  cancellation, presentation fence, one-shot settlement/load/clearance requests,
  stale acknowledgements, faint masks, all three 8-second return fallbacks and wrap.
- Debug full-App transition/save probes: 2 pass, including direct defeat visibility.
- Architecture/source manifest/save regression: 42 tests, 40 pass, 2 existing skips.
- Seven market lock modes pass: idle30, idle120, solo, manual, tutor, never, muted.
- Full native suite: 83 tests, 74 pass, 9 pre-existing failures reproduced with
  the baseline executable. No new failures. Existing failures are:
  `test_explore_button_pixels_and_hit_boundaries`,
  `test_explore_fish_bridge_occlusion_and_wildlife_grass`,
  `test_home_fed_companion_clears_bowl_for_hungry_leader_after_restore`,
  `test_home_ambient_clock_wrap_matches_baseline`,
  `test_home_ambient_routes_frames_damage_and_rng_match_baseline`,
  `test_pair_flow_matches_pre_refactor_behavior`,
  `test_pair_flow_matches_pre_refactor_clock_wrap`,
  `test_visitor_matches_pre_refactor_behavior`,
  `test_visitor_matches_pre_refactor_clock_wrap`.
  The six frozen home/pair/visitor traces already disagree with this baseline;
  their fixtures were not regenerated to hide those failures.
- V2 Lite Debug build succeeds: application 2,082,816 -> 2,081,856 bytes (-960).
  Xtensa debug `sizeof(AmoledApp)` 18,824 -> 18,872 bytes (+48). The flow adds no
  heap allocation. These size results do not establish device FPS or power usage.
- No shared `src/` production changes; SDGOODS code/build not changed. No V1/OSTB
  firmware build, hardware flash, resource/schema/partition change or remote push.
- Evidence: `.stick-local/artifacts/explore-transition-20261116/verification.json`
  and its adjacent logs. No hardware acceptance performed.
