# Native test cleanup

Authorized by the user's request to implement the preceding test audit.
Work stays on the existing isolated work branch, preserving the uncommitted
HomeRestFlow/HomePresenter batch. No production firmware, resource, save,
partition or behavior changes. No firmware build, flash, commit or push.

## Plan and contracts

- [x] Replace the six Home/Pair/Visitor raw-float hash assertions with named
  field comparisons. Keep all seeds, scenarios and normal/wrapping clocks.
  Integer state, frame clocks, rewards and PRNG remain exact; finite floating
  fields use absolute tolerance 0.001 logical pixel (zero relative tolerance).
  Keep per-tick normal/wrap comparison. Reference snapshots retain first/last,
  phase/task/motion/route transitions and one-second trajectory samples.
  Retain partial-vs-full rendering and runtime invariants on every journey.
- [x] Merge five repeated sleep/food journeys: run each once, applying both
  behavior assertions and existing trace fixtures. Keep both fixture families.
- [x] Mark three Debug-only tests skipped under Release instead of passing
  without running; move two header compilation tests to a separate suite.
  Move unknown-case CLI rejection into native harness tests with a shared build.
- [x] Verify comparator with positive and mutation cases, run Release and Debug
  journeys and independent header/harness checks, then review the final diff.

## Reference provenance and scope

The investigation at `.stick-local/artifacts/home-sequence-diagnosis-20261116/`
proved that enabling fused floating calculations on historical 7c46bb4 matches
all 576 old summaries, and reverting the navigation changes from 11d6b77 on
the current snapshot matches all 4,076,014 historical recorded fields.
The user reviewed that diagnosis before authorizing this cleanup.

The replacement structured reference is extracted from the investigation's
pre-cleanup current source snapshot and original instrumented traces; it does
not come from newly modified tests. Its provenance includes production source
hashes and the reviewed navigation revision. Old raw hash fixtures stay intact
as historical evidence. Baseline changes must be explicit reviewed behavior
changes, never an automatic consequence of a failing test.

Selected historical snapshots protect routes/state/timing/rewards at important
boundaries and regularly sampled positions; per-tick normal/wrap comparisons
protect rollover behavior between snapshots. This intentionally replaces
platform-dependent bit-for-bit historical hashes, not every previous tick with
a new full-size golden dump. Float tolerance is far below one logical pixel.

The three existing exploration/food assertion failures are out of scope and
must remain visible. Do not delete old scenarios, loosen integer/PRNG checks,
or treat Debug/release rendering differences as interchangeable.

Evidence and execution ledger:
`.stick-local/artifacts/native-test-cleanup-20261116/`.

## Verification (2026-11-16)

The native regression class now has 81 methods, down from 86: two aggregate
tests were merged into their existing five journeys, two compilation tests
moved to the header suite, and one CLI test moved to the harness suite.
All scenario coverage is retained; the five journeys each execute once.

| Check | Result |
| --- | --- |
| Release native + harness + comparison helpers | 88 run: 82 pass, 3 Debug skips, 3 existing errors |
| Debug native + harness + comparison helpers | 88 run: 85 pass, 3 existing errors |
| Standalone header compilation | 2 pass, no application link |
| Typed comparison / C++ trace helpers (included above) | 6 pass |
| Independent final review | No actionable findings |

The six former hash failures are resolved. The unchanged errors are
`explore-buttons`, `explore-water`, and `home-food-exit-stall`; they remain
ordinary failures, not skipped or expected failures. Full suite commands
therefore still exit 1.

Typed references cover 288 seed/scenario combinations, 4611 historical
snapshots, and 41315 ticks per clock mode. The two clock modes compare all
ticks. The comparator observed missing-module RED then GREEN; additional
checks reject visible float drift, integer changes, missing fields/frames and
nonfinite values. The C++ clock test also observed RED for an active deadline
that wraps exactly to zero, then GREEN with an explicit active flag.

The first run exposed two test-fixture epoch problems: Pair's unused initial
deadline used desktop time rather than simulated time, and Visitor's cleared
deadline was subtracted from the simulated base. Pair setup now uses
`base + 3600000`; Visitor output uses its motion to distinguish an inactive
deadline from a valid active timestamp zero. Only these documented clock
fields were transformed in the pre-cleanup reference. See the
[reference guide](../../tools/fixtures/amoled_home_sequence_fields.md).

Production source hashes match the pre-cleanup snapshot. No firmware or
hardware verification was needed for these host-only changes; no commit,
push or flash was performed. Original raw-hash fixture files remain intact.
