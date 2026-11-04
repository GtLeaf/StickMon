# 共享战斗执行

更新：2026-10-26。共享行动服务已接入横屏与 AMOLED；战斗会话拆分另见[BattleSession](../amoled-app-refactor/battle.md)。

## 行为要求

1. When either frontend starts a planned action, shared code shall perform
   status checks, charge transitions and damage calculation once, preserving
   the existing Stick rule and random-call order.
2. When an action reaches its impact point, shared code shall commit self-hit,
   damage, effects, contact/absorb abilities, move history and proficiency once.
   Repeated preparation/commit shall not repeat mutation or consume randomness.
3. Given identical actors, action context and random stream, immediate and
   delayed execution shall produce identical gameplay results and random calls.
   Each frontend shall use the shared execution path for both sides.
4. While showing actions, each frontend shall retain its animation timing and
   ordered recovery/block/charge/move/result/effect logs. Fainting and forced
   endings shall prevent further actions and duplicate rewards.
5. Existing save formats, gameplay tuning, battle-exit save boundaries and
   non-battle flows shall remain compatible. Relevant tests and all affected
   firmware/Web builds shall pass.
6. Hardware validation shall back up and restore the existing save, use bounded
   controlled scenarios, and record display/audio/radio limits independently.

## 设计与所有权

BattleActionService is platform-independent and sits beside BattleSystem and
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
AMOLED growth orchestration now uses ProgressionFlow; save retries use the
existing SaveCoordinator. Their ownership remains separate from action rules.

Validate deterministic random traces, duplicate calls and edge cases in a
shared host, actual AMOLED application flows and existing regressions. Stick
integration is checked at its call boundary and by firmware builds; no claim
of a full native Stick scene simulation. Use debug-only logs and controlled
device scenarios with save backup/restore before final normal startup.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Audit both paths, capture sources and run existing battle/native baseline. (R1-R5)
- [x] Add shared prepare/commit service and register the shared source. (R1-R3)
- [x] Integrate Stick and AMOLED action, effects and ending adapters. (R1-R4)
- [x] Add deterministic, duplicate-submit and real application regressions. (R2-R5)
- [x] Build affected firmware and both Web targets on final sources. (R5)
- [x] Flash connected hardware, capture battle logs and verify save restoration. (R6)
- [x] Record results and remaining limits. (R5-R6)

2026-10-18 的 V2 受控战斗、存档恢复及硬件限制，见
[原批次验证记录](../../doc/代码结构评估与优化方案-2026-10-18.md#战斗执行共享与实机验证记录)。
