# Design

Add a platform-independent BattleActionService beside BattleSystem and
BattleTurnController. A fixed-capacity PendingAction records preparation,
outcomes and a committed flag. prepare performs action checks, charge changes
and damage calculation. commit applies HP/effects once and retains results.

Use the existing Stick order: damage, move effects, post-damage abilities,
move history, proficiency. Failed/missed/immune moves retain their existing
history rules; absorption applies through the shared ability helper. This
also fixes missing AMOLED abilities and its premature move-history mutation.

Stick commits at its animation impact; AMOLED commits immediately and animates
the returned HP snapshots. Frontends own logs, sound, animation, fainting,
navigation and saves. They map ordered results without re-running rules.
Keep growth orchestration and general save scheduling as later batches.

Validate deterministic random traces, duplicate calls and edge cases in a
shared host, actual AMOLED application flows and existing regressions. Stick
integration is checked at its call boundary and by firmware builds; no claim
of a full native Stick scene simulation. Use debug-only logs and controlled
device scenarios with save backup/restore before final normal startup.
