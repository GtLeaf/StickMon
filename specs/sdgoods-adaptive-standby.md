# SDGOODS adaptive standby

Approved: inactivity tiers0..5min/5min..1h/1h..1day/1..3days/3days+ map to
30s/5min/2h/12h/24h. First2tiers retain RAM via light sleep, later tiers deep
sleep. Only physical user wake resets inactivity; silent timer wakes do not.
Darkness multiplies existing day/night satiety decay by0.5, retaining sub-point
and sub-minute remainders across lock/unlock/deep sleep. Existing bowl food is
consumed via shared HomeCare rules at hunger<70; never refill from inventory.
Replay game-minute/real-minute/hunger events chronologically, without replaying
visual AI, random movement or input. RTC elapsed time handles early button wake.

Safe room only for offline/deep path; battle, hatch, linked visits, contact events,
progression and failed saves retain short running standby. Market exits before
lock as today. First long deep wake resumes at home. Load/validation/write failure
must preserve last durable state and never acknowledge unwritten checkpoint.
Cold reset never interprets stale RTC contents as elapsed sleep. Timer boot checks
retained checkpoint then loads validated resources/catalog and save without
initializing LCD/audio/IMU/UI. Key wake reconciles before normal boot. Existing
save schema unchanged; RTC context is versioned and tied to canonical snapshot.
Other platforms keep their care behavior. No V1/OSTB builds, flash, commit/push.

## Verification and failure behavior

The production `StandbyBootFlow` and `StandbyRuntime` execute in native tests
with deterministic board time and injected storage faults. Native engine and
real SaveManager tests exercise fractional clock import/export, progression,
canonical save/load identity and failed-write replay. GPIO/light sleep and
physical RTC drift still require device measurement; successful SDK builds are
not hardware acceptance.

A failed timer write retains its original durable checkpoint and retries dark
in30minutes. A failed key-wake write carries the reconciled RAM candidate into
verified normal startup for its initial save retry; the old RTC anchor is only
cleared after that succeeds. Invalid timer anchors also retry dark, without
loading or overwriting a game snapshot; a physical key can boot normally from
the last durable save. No elapsed time is invented from invalid retention.
