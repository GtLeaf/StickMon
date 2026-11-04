# 探索队伍服务

更新：2026-10-26。已核对三个生产调用方接入，验收仍有三项未关闭。

## 行为要求

1. When either exploration consumer queries health, leadership or followers,
   it shall preserve its existing results, including empty teams, fainting and
   AMOLED's extra companion species-ID check.
2. When a completed step is a recovery step, the shared service shall apply the
   same bounded HP recovery to eligible team members without reviving them or
   clearing statuses. It shall not touch storage or other state.
3. When an application settles adventure bond or clears battle statuses, shared
   mutations shall preserve existing member eligibility, capped changes and
   result values. Horizontal local visiting companions remain eligible only
   under the existing active/exploring condition; AMOLED excludes visitors.
4. Each application shall retain its original settlement latch, operation
   timing, save urgency, effects, item reset and scene lifecycle. The service
   shall not depend on UI, clocks, storage, RNG or device SDKs. No save schema,
   resources, protocol or firmware configuration shall change.
5. Acceptance shall compare actual consumers to this batch's frozen sources,
   exercise HP/bond/status/save/latch boundaries, preserve existing fixtures,
   run relevant checks and build affected firmware.

## 设计与所有权

The header-only Game::ExplorePartyService follows TeamRoster and reuses
ExploreRunRules for recovery timing and amount, BondSystem for gains and caps,
and TeamRoster for bounded member iteration. The service owns health/leader/
follower queries, step recovery, adventure bond mutation and status cleanup.
Methods return whether state changed or the actually granted bond increment.
The visitor option is explicit and defaults to exclusion.

ExploreScene keeps its existing adapters and deferred recovery save. GameEngine
keeps the grantAdventureBond and clearExploreBattleStatuses entry points,
immediate bond saves, SOON status saves and local-visit eligibility. ExploreSession
keeps its public/private entry points, companion species check, statusChanged
effect, return latch, item reset and unconditional return-save effect.

AMOLED's old combined status/bond loop becomes two service calls in the same
synchronous settlement method. Bond does not read statuses and there is no
callback between the calls. Application return controllers remain independent.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Inspect differences and freeze current source/configuration/resources. (1-5)
- [x] Extract party service and migrate three consumers without moving effects. (1-4)
- [ ] Verify independent state mutations and actual frozen/current adapters. (1-5)
- [ ] Run relevant regressions, pixel checks and five application builds. (4-5)
- [ ] Audit scope, document acceptance and retain preexisting failures. (5)

原设计计划使用 `.stick-local/artifacts/explore-party-service-20261023/`
保存 559 份源码／配置／测试及 159 份资源哈希；当前指定目录不可用，也未找到本规范的
验收报告。测试脚本已存在，但不能据此关闭上述三项；结果和既存失败需补录。
