# Faint recovery and home presentation (batch 2c)

Authorized by the user's next-batch execution request after commit 6765233.
Goal: separate faint rest lifecycle and a coherent first home presentation slice,
keeping room gameplay, visual output, RNG and persistence behavior unchanged.

HomeRestFlow borrows both actors, Simulation and HomeSleepFlow for companion
placement. It owns no duplicate state. It handles forced home placement, ongoing
main/companion faint rest and recovery. App retains cancellation priority, main
idle-decision scheduling, animation feedback and damage. HomeSleepFlow continues
to own ordinary sleep. Care/HP recovery policy remains shared and unchanged.

HomePresenter first slice owns camera and SleepBreathClock, selects the expanded
sleep sprite once at animation start, and maps world to screen coordinates. App
supplies room geometry. Actor frame clocks, visual-action selection across flows,
HUD/model composition and damage aggregation remain later presentation slices.
No App/platform/storage/cache dependency or per-frame allocation in either module.

Keep placement and active-rest entry distinct: forced placement clears velocity
and routes and repositions already-resting actors; active-rest does not repeatedly
move the companion. Main rest corrects its bed pose at 0.01 tolerance. Preserve
main recovery's random idle scheduling and companion's fixed 700ms delay; main
faint breathing700ms, companion frame interval800ms, recovered frame delay520ms.
Keep camera integer rounding/clamps and unavailable-room projection fallbacks.
Use a bed-coordinate overload for awake bed-pose tests to avoid full layout/anchor
lookup each tick (deferred minor from batch2b).

Approach: two focused modules with borrowed authoritative state; do not introduce a
replacement room controller or move all presentation/frame fields in one step.
Existing work branch is isolated from main. No commit/push/flash unless requested;
only V2 firmware build, no V1/OSTB work. Shared and SDGOODS production untouched.

## Plan

- [x] Freeze HEAD6765233 sources, hosts and before release/debug binaries.
- [x] Add isolated tests and observe missing-module RED for both modules.
- [x] Extract rest lifecycle and presentation, preserve App order and source manifest.
- [x] Compare state/frame/RNG traces to baseline; run focused sanitizers, complete
      native suite and Debug sleep/food/save/lock regressions. Keep known9 failures.
- [x] Build V2 Lite Debug, measure app/object sizes and run fresh final review.
- [x] Record results, limits and remaining presentation scope without auto-commit.

Review focus: repeat faint updates vs forced placement, both-fainted ordering,
failed companion placement, restored/ordinary sleep with faintRestActive set,
RNG ordering, camera rounding, uint32 breath wrap, partial damage and lock/wake.
Evidence: `.stick-local/artifacts/home-rest-presenter-20261116/`.

## Verification (2026-11-16)

| Check | Result |
| --- | --- |
| Rest/Presenter isolated tests | Missing-module RED, then normal and ASan/UBSan pass |
| Flow and shared home core tests | 9 pass |
| Home behavior, architecture, source manifest and render demand | 39 pass |
| Complete native render suite | 86 run: 77 pass, same 9 pre-existing failures |
| Debug faint/sleep/food/save/locked-room regressions | 5 pass |
| Frozen before/after comparison | 16 cases in each of Release and Debug match |
| V2 Lite Debug firmware | Build passes; no V1 or OSTB build |
| Independent final review and diff whitespace check | No actionable findings; clean diff check |

The frozen baseline is clean commit `6765233b7be6c0a6dad34cd96075628a4fe4d75f`.
Comparisons retain 487 existing sleep/care/room records and 15 frame digests, plus
133 new faint-placement records including 48 frame digests. Both build flavors
also match 671 food records, 2264 explore-transition records, 576 ambient/pair/visitor
sequence rows and the three existing abort signatures. The new checked-in fixture
was generated from frozen pre-extraction production code; existing fixtures were
not regenerated. New coverage includes forced and ordinary entry, both actors,
repeated rest, independent recovery and an inactive companion.

The nine existing native failures remain: explore button pixels, water/bridge
occlusion, restored food-exit satiety, and ambient/pair/visitor hashes under normal
and wrapping clocks. This batch does not resolve them; the final failure names
match the preceding batch and the frozen before/after outputs match.

| Measurement | Before | After | Delta |
| --- | ---: | ---: | ---: |
| AmoledApp.cpp lines | 9234 | 9146 | -88 |
| Xtensa Debug sizeof(App), bytes | 18904 | 18920 | +16 |
| V2 Lite Debug application bin, bytes | 2081152 | 2081536 | +384 |

Firmware SHA-256:
`8dfc4764f1da4bcd0b384d4f0d2809e69c8ca9e5b3002d6999e6727a92d8b649`.
The ownership split is not a claim of lower runtime memory, frame time or power.
No hardware testing, flashing, commit or push was performed for this batch.
Actor frame clocks, visual-action selection, HUD/model composition and damage
aggregation remain in App for later HomePresenter work.
