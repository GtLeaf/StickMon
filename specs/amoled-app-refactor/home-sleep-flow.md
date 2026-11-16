# Room sleep flow extraction (batch 2b)

User authorized continuing firmware maintainability batches on 2026-11-16.
Goal: give ordinary sleep one testable owner without changing gameplay or timing.
This follows HomeFoodFlow and preserves all preceding uncommitted work.

## Design and scope

Extract `app/HomeSleepFlow` for companion sleeping geometry/anchor/grid selection,
leader bed lease and direct/routed entry, companion seek/target retention, both
actors' wake transitions and nocturnal food retry. Reuse Home::ActorController
intents and Home::Simulation tasks/resources. Borrow actors and Simulation, with
no duplicate actor state, allocations, platform/storage/resource singleton access.
App supplies a plain room Layout and synchronous Movement adapter; it retains
sprite animations, frame clocks, camera, damage and diagnostic formatting.
Faint recovery, scene cancellation priority, restored-pose wander selection, and
shared gameplay policies stay in their existing owners. Faint positioning reuses
the extracted companion spot selector through the existing App wrapper.

Implementation choice: one focused ordinary-sleep module rather than combining
food/sleep/faint/animation into a new room controller. Splitting geometry and
lifecycle into separate modules would add interfaces with no independent owner.
The user's continuing execution instruction authorizes this next planned batch;
no new design approval, commit, push, flash or environment publish is requested.

Preserve exact tolerances (bed pose 2.5 x 6), clearance clamps, inclusive bed bounds,
3-unit Y-major grid and first-minimum tie order. Preserve main/companion wake
asymmetry: main continues the same update tick, companion returns; companion food
wake retry is 60000ms and ordinary idle delay700ms. Move callbacks retain existing
turning and route behavior, including companion ingress allowance and failure stop.
No random calls may be added, removed or reordered. Keep rendering boundaries.

## Implementation plan

Use executing-plans for inline production implementation with independent baseline
and test preparation, followed by one fresh final review. C++17, native host and
ESP-IDF V2 Lite Debug. Existing `work` checkout is isolated from main; retain it.

- [x] Freeze current App/modules/hosts and binaries in ignored task artifacts.
- [x] Write isolated production-Flow tests; observe missing module RED.
- [x] Extract geometry and lifecycle; adapt App, source manifest and source-bound
      tests. Test direct and routed entry, failures, obstacles, wake/wrap.
- [x] Compare real App sleep/state/render traces against frozen source; retain
      known native failures instead of regenerating historical fixtures.
- [x] Run isolated ASan/UBSan, relevant source/architecture/scheduling checks,
      native release suite and Debug sleep/food/save/lock journeys.
- [x] Build only affected authorized V2 Lite Debug; record bin and App object sizes.
- [x] Independent review; address material regressions and record evidence.

Review focus: unsuccessful movement releases bed; direct entry does not generate
empty-route cancellation; companion sleep excludes main route during turns;
wake-for-food retries survive uint32 rollover; actor frame/damage and tick ordering
stay identical. Test frozen baseline from uncommitted prior batch, not Git HEAD.

No V1 or OSTB firmware builds or UI parity work. SDGOODS production untouched.
No resources, save schema or partition changes. Hardware acceptance remains separate.
Evidence: `.stick-local/artifacts/home-sleep-flow-20261116/`.

## Verification — 2026-11-16

Implemented `HomeSleepFlow` with layout and synchronous movement adapter. App remains
responsible for sleeping frames/breathing, faint recovery, cancellation priorities,
damage/camera and ordinary movement presentation. No random calls were introduced.

| Check | Result |
| --- | --- |
| Frozen source/host capture | 73 files; trace-disabled output equals pre-batch binaries |
| Real-App sleep/faint/wake/food/room motion trace | Release and Debug: all 487 rows / 15 full-frame digests byte-identical |
| Sleep/food/transition flows and shared home-core | 8 pass; all three Flow hosts exercised with ASan/UBSan |
| Native release full suite | 85 tests: 76 pass, same 9 pre-existing failures |
| Debug sleep/food traces, saved turns and locked care | 4 pass |
| Home source, architecture, manifest, render scheduling | 39 pass |
| Fresh independent production review | No material correctness findings |
| V2 Lite Debug firmware | Build and app partition-size check pass |

Known failures are the explore-button pixel assertion, fish-bridge resource assertion,
restored food-exit assertion, and six old ambient/pair/visitor hash fixtures. Their
terminal assertions and all 576 sequence records are unchanged against the frozen
pre-batch source. None of those fixtures was updated. New sleep fixture comes only
from the frozen old implementation and does not require local artifacts to run.

Size comparison for this batch: App 9,424 -> 9,234 lines; Xtensa Debug App object
18,896 -> 18,904 bytes (+8); app binary 2,080,864 -> 2,081,152 bytes (+288).
Firmware SHA256: `a59c9ea97465d6bba519fd5234cc55b4398478a3b1aa6cafb90c1dff3bb6f2d3`.
No runtime memory/high-water, frame-time or hardware performance improvement is claimed.

Deferred review minor: the ordinary awake bed-pose check currently builds full sleep
layout and scans behavior anchors. A bed-only path could avoid this extra work;
it does not change results, but its device CPU cost has not been measured.

Only V2 firmware was built. No V1/OSTB/SDGOODS build, flashing, device acceptance,
commit, push or environment publication. SDGOODS application and shared gameplay
production sources remain unchanged. Previous uncommitted batches are preserved.

Evidence: `.stick-local/artifacts/home-sleep-flow-20261116/`, especially
`release-comparison.json`, `debug-comparison.json`, `flows-and-core.log`,
`native-final.log`, `debug-focused.log`, `boundaries.log`, `sizes.json` and
`amoled-v2-build.log`.
