# SDGOODS adaptive standby implementation plan

> Use superpowers:executing-plans inline, with one final fresh review.

**Goal:** Approved tiered sleep plus exact dark care and bowl consumption.
**Architecture:** Pure shared standby timeline/policy, opt-in native engine care
bridge, SDGOODS retained checkpoint/early boot controller and event-driven wait.
**Tech Stack:** C++17, ESP-IDF5.5.4, native sanitizer hosts.
**Spec:** specs/sdgoods-adaptive-standby.md.

## Global Constraints
Preserve save schema, current branch work and other platform behavior; no V1/OSTB
builds, device/serial access, flashing or automatic commits/pushes. Use exact
30s/5min/2h/12h/24h tiers and0.5 dark satiety multiplier.

## Review Focus
- Early button wake, cross-tier/day/night, tick wrap and repeated short locks.
- Food exhaustion, two members, sleep/ineligible/guest monsters, fractional care.
- Timer boot must stay dark and not reset inactivity; cold reset rejects RTC.
- Save/load failure and stale checkpoint cannot duplicate food or elapsed time.
- Audio notification races, key debounce, safe-state gates and other families.

## Task1: Shared policy and chronological care
- [x] Tests RED for tier boundaries, half rate, split-vs-batch, bowl/day/night.
- [x] Pure policy/timeline reuse CareTicker and HomeCare; exact fractional units.
- [x] Native tests GREEN, legacy care/save tests unchanged.
## Task2: Native engine and retained sleep lifecycle
- [x] Tests RED for care bridge/checkpoint/time/rollback and unsafe scene gates.
- [x] Implement opt-in engine bridge, RTC snapshot-bound context, timer/key boot.
- [x] Native normal/locked/restore and failure cases GREEN.
## Task3: Event waits, hardware integration and verification
- [x] Event policy test/source guards RED for polling/boot order.
- [x] Block idle audio, GPIO notification wake, tickless/light sleep and runtime.
- [x] Native ASan/UBSan journeys, shared regressions, SDGOODS+V2 SDK builds,
      final fresh review, fix important findings RED→GREEN; leave uncommitted.
