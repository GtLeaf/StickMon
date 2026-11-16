# Room food flow extraction (batch 2a)

Scope authorized 2026-11-16: food ownership/arbitration, approach selection,
feeding clocks and one-bite progression, post-feed clearance. Sleep is a separate
later batch. Preserve the preceding uncommitted ExploreTransitionFlow work.

## Boundaries

`app/HomeFoodFlow` owns retry/pending state and the two bite deadlines. It borrows
GameState, Home::Actor and Simulation, consumes a household observation and bowl
geometry, and delegates route start through a narrow synchronous Movement interface.
The interface only chooses a main approach and starts existing actor movement;
turning, sprite frames, camera, hardware and save remain in App. Generic blocker
yielding also serves sleep and stays in App; failed arbitration emits a yield request.
Existing HomeFlow owns the leader's post-feed room action, Simulation owns the bowl
clearance lease, and the companion's exit target search belongs to HomeFoodFlow.

Keep exact RNG call order (main bite-delay before duration; companion duration before
bite-delay), pre-bite eligibility for completion, and reward/save before release.
Do not unify the asymmetric main/companion behavior by accident: only failed main
consumption truncates its session; companion excludes visitors; animation clocks differ.
Only const state access; test fixture seeding is isolated in tools. No per-frame heap.

## Steps

1. Freeze current uncommitted sources and native binaries under the task evidence
   folder; capture real-App food traces before changing production.
2. Write standalone tests for exclusive bowl ownership, hunger selection, retry,
   visitor exclusion, failed approach cleanup, per-bite mutation, repeated same-time
   updates, rollover, and clearance. Confirm missing Flow RED.
3. Implement Flow + movement adapter, migrate food clocks/decisions, update manifest
   and test access. Keep RNG and visual traces unchanged.
4. Run ASan/UBSan host, full native suite against the frozen baseline, shared home
   simulation/care tests, transition regression, V2 Lite Debug build and size check.
5. Independent production review; fix material regressions and record evidence.

No V1/OSTB builds, no resources/save/partition change, no push or flashing. Do not
commit prior work implicitly. Existing native suite has 9 known failures, including
home-food-exit-stall; distinguish fixture assumptions from runtime defects before
changing any behavior. Evidence: .stick-local/artifacts/home-food-flow-20261116/.

## Verification — 2026-11-16

Implemented `HomeFoodFlow`, its synchronous movement adapter, const food state and
fixture-only clock seeding. All preceding uncommitted ExploreTransitionFlow changes
remain intact. `AmoledApp.cpp` decreased from 9,736 to 9,424 lines in this batch.
No shared gameplay, resources, save schema, partition, or sleeping policy changes.

| Check | Result |
| --- | --- |
| Isolated food flow, ASan/UBSan | Pass, also independently rerun by reviewer |
| Real-App food/clearance/partial-render trace | 671 rows exactly match frozen pre-batch source |
| Full native release suite, before and after | Both 84 tests: 75 pass, same 9 existing failures |
| Existing ambient/pair/visitor normal and wrap traces | All 576 rows match before/after, including RNG output |
| Debug food, rewards/reboot, saved routes, locked care, exploration transitions | 5 pass |
| Home-core focused suite | 10 pass, 1 pre-existing HomeViewState host linker error (see below) |
| Home source checks / architecture / source manifest | 21 / 13 / 5 pass |
| ExploreTransitionFlow and render scheduling | 3 pass |
| V2 Lite Debug firmware | Build and partition-size check pass |
| Fresh independent review | No Critical or Important findings |

Native failures retained: explore button pixels, fish bridge resources, restored
food-exit test, plus six historical ambient/pair/visitor hash fixtures. Both frozen
and current sources reproduce the same failures. No old fixtures were regenerated.
The new food fixture is generated only from the frozen pre-batch implementation.

Additional old test-harness limitations surfaced while choosing regressions:
`test_home_hud.py` requests C++11 despite ContentData requiring C++17;
`test_home_view_state.py` does not link uzlib_crc32/uzlib_adler32 on this Linux host.
The shared source and these runners are unchanged. Broad home discovery was stopped
because it includes legacy OSTB UI parity; the explicit portable home-core suite
replaced it. No OSTB acceptance is claimed.

Firmware app: 2,081,856 -> 2,080,864 bytes (-992). Xtensa Debug `sizeof(AmoledApp)`:
18,872 -> 18,896 (+24 bytes), not a runtime heap/high-water measurement. App SHA256:
`ef9fd35e9881c8b429d7322b8096783e6691ba7f1dcdb05788800b0271c1a325`.
Only V2 firmware was built; no V1, OSTB or SDGOODS firmware build. The change is in
the AMOLED application used by V2, so SDGOODS production sources are unaffected.
No device/hardware acceptance, flashing, push, publish or commit was performed.

Deferred review minor: add explicit isolated assertions for `!finished` on the final
successful bite and rejected consumption for each actor. These paths were checked
against the old source; real-App feeding/reward/save journeys pass, but the two
specific isolated assertions remain follow-up coverage.

Evidence (ignored local artifacts): `.stick-local/artifacts/home-food-flow-20261116/`.
The checked-in fixture and tests do not require the local frozen source to run.
