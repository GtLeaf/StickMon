# 探索共享参数与拾取规则

更新：2026-10-26。提取已完成；独立[验收记录](acceptance.md)保留随机数对照、原有差异与既存失败。

## 行为要求

The horizontal scene and AMOLED session share area tuning and pickup rules.
Each application retains its own session, presentation, save coordination and
random-call order.

1. When either application queries an area, its map-count distribution,
   encounter probability, cooldown, encounter cap, pickup probability and map
   generation retry seeds shall match the pre-change working tree.
2. When a pickup is selected, its numeric ID, ordered weights, daily-step gate,
   coin bounds and item mapping shall match the pre-change rules. Selection
   shall be a pure operation; each application shall retain its existing random
   calls, short circuits and final-reward fallback.
3. When either application runs an existing exploration journey, state and
   random-call transcripts shall match its own frozen pre-change application.
   Existing differences between the two applications shall remain intact.
4. The shared rules shall not depend on UI, platform, storage or device SDKs.
   This batch shall not change save schema, resources, protocol, scene lifecycle,
   recovery coordination or firmware partitioning.
5. Acceptance shall include independent old-rule checks, actual application
   journeys, relevant regressions and builds for affected firmware targets.
   Preexisting test failures shall remain separately documented.

## 设计与所有权

ExploreAreaCatalog owns map-count bounds and encounter probability.
ExploreRunRules owns shared scalar limits, pure count/gate helpers and
map-generation retry constants. ExplorePickupRules owns ordered
pickup tables, IDs, step eligibility, weighted selection and item mapping.
These header-only definitions require no new source manifest entries.

ExploreScene retains its presentation RouteMap and labels; gameplay fields
reference the shared catalog and pickup tables. ExploreSession uses the same
rules while retaining its own session state and effect dispatch. Both callers
retain their existing GameRandom calls; the shared weighted selector accepts a
roll without advancing random state. Coin rolls and inventory/coin mutations
remain in the original application coordination paths.

Horizontal invalid-area selection still falls back to its first RouteMap.
AMOLED pickup-table and encounter-probability access retain their explicit
last-area clamp. Valid area behavior and the original map-count RNG skip under
horizontal boss pity remain unchanged.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Freeze the existing sources and fixtures; enumerate duplicated rules. (1-5)
- [x] Extract pure parameters, pickup tables, IDs and weighted selection. (1-2,4)
- [x] Migrate both consumers without moving random calls or effects. (1-4)
- [x] Verify independent old-rule and actual-adapter state/RNG equivalence. (1-3,5)
- [x] Run relevant regressions, affected firmware builds and final scope audit. (4-5)
- [x] Record acceptance and preexisting failures. (5)

Evidence and limitations: [acceptance.md](acceptance.md).
