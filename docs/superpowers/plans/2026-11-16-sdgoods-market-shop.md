# SDGOODS Kecleon implementation plan

> Execute inline using superpowers:executing-plans. User requested first batch;
> no further approval gate. No automatic commits/pushes or V1/OSTB builds.

**Goal:** Real round Kecleon commerce, advice, star box and coin feedback.
**Architecture:** Native round UI/controller using existing shared rules; small
parameter additions to shared animation painters preserve default AMOLED output.
ExploreScene owns lifecycle and submits requests through MarketTransaction.
**Tech Stack:** C++17, Canvas565, existing fonts/art/persistence and host probes.
**Spec:** specs/market-stalls/sdgoods-shop.md.

## Global constraints
360x360 native circle; Debug entry unchanged; no schema/business-rule changes;
only Kecleon service; scope excludes Gengar/Chansey service UI. Keep66ms cadence.

## Review focus
- Durable snapshot plus failed encounter history never rolls back paid state.
- First shelf persistence fails safely; scene exit/lock/cancel cannot leak a tap
  or repeat a transaction/reward; animated display never mutates inventory.
- Long stock lists, inertial braking, circle edges and button/description widths
  stay legible and hit testing matches visible content.
- Coin anchors survive slow saves, repeated transactions and cleared scene clocks.
- AMOLED defaults retain pixels/timing/byte order/clip; resource bank includes art.

## Task1: Foundation, round service and end-to-end proof
Files: src/core/GameEngine.cpp; src/scenes/ExploreScene.h/.cpp;
new header-only src/ui/round_touch/MarketShopUi.h;
src/ui/common/MarketCoinHud.h,MarketBoxPerformance.h;
new tools/sdgoods_market_host.cpp,tools/test_sdgoods_market.py.

- [x] Write failing snapshot/history transaction and round UI/service tests;
      observe baseline missing-service/API and durability failures.
- [x] Align save return semantics; implement round pages and safe geometry,
      shared animation/HUD optional layouts; no default AMOLED changes.
- [x] Wire facing/open/save/request/feedback/close/update/render/lock lifecycle;
      all commerce mutations use shared rules and atomic transaction.
- [x] Verify rules, actual engine journeys, money/stock/box persistence/faults,
      inertia/gesture ownership, screenshots/clip/swap and real resources.
- [x] Run AMOLED shop/box/coins, save/market regressions and architecture checks;
      build SDGOODS and V2 only; inspect native captures and one final independent
      review. Record evidence, leave source uncommitted.

Verified 2026-11-16: native embedded and staged external-resource ASan/UBSan
journeys, six market rules/UI hosts, AMOLED box and real coin App regressions,
27 save/architecture/family/demand tests (two paused OSTB gates skipped),
SDGOODS Debug and AMOLED V2 builds. Fresh independent review: no blocking
findings. Evidence and actual round previews:
`.stick-local/artifacts/sdgoods-market-shop-20261116/`.

Deferred minor: native test object cache does not key on external mode; use
distinct output directories for embedded and external runs. This task did so.
Branch retained uncommitted; no flash or publishing performed.
