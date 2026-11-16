# Home behavior references

`amoled_home_sequence_fields.json` stores typed Home/Pair/Visitor reference
states. Each case lists column names once; each frame row contains seed,
scenario, tick index, then the corresponding column values. Failures expand
these columns into names and report seed/scenario/frame/field.

Integer state, relative deadlines, animation frames, rewards and final PRNG
values compare exactly. Finite float fields use absolute tolerance 0.001
(no relative tolerance). The fixture retains first/last frames, both sides
of phase/task/motion/route transitions, and samples every 20 ticks (one second).
Every tick additionally compares normal and wrapping-clock runs, including
the renderer's existing partial/full checks and runtime invariants.

## Provenance

This reference was extracted from the pre-cleanup diagnostic `current-*`
traces, after investigation established the floating-point portability issue
and the previously committed navigation behavior change. Source hashes and
navigation revision `11d6b77` are recorded in the JSON. The original
`amoled_home_flow_baseline.json`, `amoled_pair_flow_baseline.json` and
`amoled_visitor_flow_baseline.json` are retained as historical raw-hash records;
the native suite no longer asserts their platform-dependent hashes.

Two explicit clock corrections apply to the captured data:

- Pair setup's initial unused deadline is relative to simulated time:
  `base + 3600000`, rather than the desktop clock's `0 + 3600000`.
- A Visitor deadline is inactive when visitor motion is `NONE`. Its typed
  representation is zero; active deadlines subtract the simulated base,
  including a valid deadline timestamp of zero at uint32 wrap.

No routes, phase events, rewards, total frame counts or PRNG expectations
were changed when applying these two clock corrections.

## Updating

Reference updates require a reviewed behavior change. Capture typed records
from an independently frozen application using
`STICKMON_SEQUENCE_TRACE=1 <native-binary> <data> --case <sequence-case>`.
Use `parse_frames` and `select_reference_frames` from `tools/amoled_sequence.py`
to select reference states; retain exact summaries from `parse_summary` and
record source provenance. Inspect the named differences against the existing
reference before replacing it. Never regenerate a fixture merely to clear
a failing test, or drop normal/wrap and invariant checks during an update.

## Test commands

```sh
PYTHONPATH=tools python3 -m unittest test_amoled_native_render test_amoled_native_harness test_amoled_sequence
AMOLED_RENDER_DEBUG=1 PYTHONPATH=tools python3 -m unittest test_amoled_native_render test_amoled_native_harness test_amoled_sequence
python3 tools/test_amoled_headers.py
```

The firmware and CLI suites share one native application build per mode and
process. Header compilation checks link no application. Release explicitly
skips Debug-only behavior instead of reporting an unexecuted case as passed.
