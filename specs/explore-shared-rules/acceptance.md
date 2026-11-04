# Exploration shared rules acceptance

2026-10-23: the first structural improvement batch is complete. It extracts
shared exploration tuning and pickup rules; it does not merge the two session
controllers. No device was flashed and no commit was created.

## Scope

Relative to the frozen 536-file working tree, exactly four existing production
files changed: ExploreAreaCatalog.h, ExploreRunRules.h, ExploreScene.cpp and the
AMOLED ExploreSession.cpp. ExplorePickupRules.h is the only new production file.
The other 503 frozen production files and all 22 existing fixtures are unchanged.
Save schema, protocol and existing UI implementations are unchanged. All 159
resource files match the earlier application-UI baseline; this batch did not
take its own resource snapshot.

The catalog owns map-count bounds and encounter probability. Run rules own
cooldown, encounter gates, pickup chance and map retry constants. Pickup rules
own numeric IDs, ordered weights, step eligibility, coin bounds and item mapping.
Both consumers retain their original RNG draws, short circuits and effects.
Horizontal final rewards still fall back to coins when the bag is full;
AMOLED still reports BAG_FULL. Existing invalid-area handling also stays intact.

One existing pickup-table test now reads the shared header. New tests include
independent frozen values and a real horizontal scene probe with source hashes.

## Verification

| Check | Result |
| --- | --- |
| Pure rules | 365,335 checks; strict C++ compilation and ASan/UBSan passed; zero RNG calls |
| Actual horizontal adapters | Stick/OSTB release/debug: 4 frozen, 4 current and 4 sanitized runs all equal; 3,600 cases per configuration |
| Actual AMOLED adapter | All 10 scenarios execute on frozen/current sources; 36 route state/RNG transcripts are byte-identical; 9 behavior tests pass |
| Exploration UI | Four configurations, 1,286 frozen pixel frames and 36 checkpoints per configuration passed |
| Existing catalog/run-rule/boundary/source checks | 4 scripts, 24 unittest cases passed |
| Related lightweight regressions | 10 scripts, 14 selected unittest cases passed, including the two new lightweight methods |
| Independent scope/provenance review | No actionable findings; old fixture and frozen source hashes verified |
| Build dependencies | All five applications recompiled their consumer and include all three shared headers; top-level dependency locks unchanged |

Horizontal cases cover all six areas, steps 0/4999/5000, generated maps and boss
pity, pickup placement, coin endpoints, item mappings, full bags, final rewards,
cooldown, repel, guaranteed encounters and RNG short circuits. Pure rules also
cover invalid IDs/areas and exhaustive weighted roll intervals.

## Application builds

Existing configurations were reused. IDF targets built the application target;
resource packs and partitioning were not regenerated.

| Target | Application bytes |
| --- | ---: |
| Stick release | 1,422,432 |
| Stick debug | 1,438,288 |
| OSTB | 1,360,352 |
| AMOLED V1 lite | 1,403,712 |
| AMOLED V2 lite | 1,725,280 |

Commands: `pio run -e m5stick-s3 -e m5stick-s3-debug -j 3`; after loading
ESP-IDF, `cmake --build build --target app -- -j 3` for OSTB and
`cmake --build build-lite --target app -- -j 3` in each AMOLED project.

## Preexisting failures and limits

The item-system suite has the same five source-text assertion failures before
and after extraction: extra medicine confirmation, medicine result enums,
legacy save layout, shop catalog layout and evolution-stone flow. Its pickup
distribution check passes. These unrelated assertions were not rewritten.

The AMOLED routes test differs from its legacy golden in all 36 state hashes.
The same mismatch reproduces using this batch's frozen pre-change sources;
frozen and current transcripts match exactly, including RNG bounds and results.
The legacy fixture was not changed. The cause of the older golden mismatch was
not established by this extraction.

Firmware compilation and host checks do not establish physical-device behavior.
No new hardware validation was performed. Prior unrelated failures remain open;
this acceptance does not claim a fully passing repository test suite.

## Evidence and reruns

Artifacts are under `.stick-local/artifacts/explore-shared-rules-20261023/`:
`acceptance-summary.json`, `horizontal-adapter-summary.json`,
`amoled-consumer-result.json`, `ui-parity-result.json`, `build-audit.json`,
`scope-review.json`, `regressions-review.json` and build/test logs.

Run `python3 tools/test_explore_shared_rules.py` to check pure rules, dependency
boundaries and all four actual horizontal configurations. Set
`STICKMON_TEST_SANITIZE=1` for ASan/UBSan. Existing exploration pixel checks use
`python3 tools/test_explore_ui_parity.py`.
