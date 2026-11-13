# Periodic Market Implementation Plan

> **For agentic workers:** Execute task-by-task; independent calendar and artwork tasks use dispatching-parallel-agents, with one final independent review.

**Goal:** Deliver the approved8 shelf entries/16 units, periodic opening,17 usable special items and Gengar's once-per-save heart-scale welcome gift.
**Architecture:** Keep all existing state bytes frozen and append a version9 extension for new inventory/season/gift fields. Shared pure calendar and market-item metadata drive all device services. The AMOLED shelf supplies two six-type selectors and preserves the existing rounded layout and scroll gestures.
**Tech Stack:** C++17, Canvas565, SaveCodec schema8, existing SGFX/SMFN/content pipelines, native/sanitizer hosts.
**Spec:** User approved8 entries: discounts3+3,candy1,nutrients2 shared,EV berries3 shared,special supply2,rare1,box1. Heart scale rotates with stones; initial Gengar greeting grants1 once per save. Calendar confirmed3–4 game-day spacing, open1day.

## Constraints and Decisions
- Alternating opening days0,3,7,10,..., one game day each; actual opening-day key persists. First day is open. Closed entry explains next opening, no new purchases/lessons/care when closed; settled performances finish normally.
- Keep current feature branch and all prior uncommitted changes; no commit/push/device access. Evidence under .stick-local/artifacts/market-season-20261113/.
- New ItemIds appended after HEART_SCALE: six nutrients (HP_UP,PROTEIN,IRON,CALCIUM,ZINC,CARBOS), six EV berries (POMEG,KELPSY,QUALOT,HONDEW,GREPA,TAMATO_BERRY), MAX_REVIVE,LAVA_COOKIE,ORAN_BERRY,SITRUS_BERRY,LUM_BERRY. Ordinary shop does not sell these; its candy purchase entry moves to the market.
- Nutrients+10EV, cap252/stat and510 total; EV berries-10/+2 affection only when EV>0; no held items/PP. Recovery berries10/30HP; cookie/lum cure major status; MaxRevive fullHP. Validate before consumption; no healing exploit on EV HP-stat recomputation.
- Initial prices: nutrients600,EV berries200,MaxRevive600,cookie120,oran20,sitrus60,lum100; heart600,stone900,candy1800,box300 retain prices. Unlock nutrients/reset3,MaxRevive4,cookie/lum2,oran0,sitrus1.
- Eight slots: DISCOUNT_A0,DISCOUNT_B1,CANDY2,NUTRIENT3,RESET_BERRY4,SPECIAL5,RARE6,BOX7. Stocks3,3,1,2,3,2,1,1. Complete16 available after unlock; locked categories stay visibly unavailable, not filled with unrelated goods.
- Box candy is bonus and does not consume fixed candy stock or normal-shop daily counter; direct market candy uses its period stock. Only one box per period; preserve the existing draw weights and whole-reward space validation.
- Gengar gift flag persisted with granted scale before success dialogue; failed save/full stack leaves gift pending, no duplicate on reload/cancel/lock.
- Preserve all raw state offsets through2480 bytes; append extension and add rawv8/schema1..7 migration. Preserve old inventory, quotas and existing bought box on upgrade when period matches.

## Review Focus
- Closed/open boundaries, stale confirmations and clock advancing during an active visit.
- Shared selector quotas and candy/box order; save failure never rerolls or double-grants.
- Stat caps, HP recomputation, invalid-target/no-effect consumption, battle-only restrictions.
- Rawv8/schema7 migration and gift idempotency with full inventory/save failure.
- Old asset IDs, decoded pixels and glyphs preserved; external content catalog includes every new item.

## Tasks
- [x] Calendar: pure MarketSchedule API and boundary tests (independent worker).
- [x] Types/items: append state/IDs, metadata, inventory and consumption; RED→GREEN native tests; move ordinary candy purchases to the market while preserving other daily supplies.
- [x] Resources: authentic Emerald icons,48px frames, fonts, runtime routing and full-generator support (independent worker).
- [x] Seasons/save:8-slot rules, shared selection quotas, bonus candy, schema8/rawv8 migration and sanitization; tests for counts, locked/stale/full/refused purchases.
- [x] UI/App: selection subpage, eight-row clipped shelf, opening status, first greeting gift, atomic save/rollback and all-family entry gating. Export full journeys.
- [x] Validate: item/market/save/calendar/catalog/related stall/architecture suites, ASan/UBSan, visual check, independent review and documentation. Report missing firmware/toolchain/device checks.

## Completion evidence

All authorized implementation tasks completed. See .stick-local/artifacts/market-season-20261113/verification.md for actual host results and limitations. Additional integration fixed full bag routes on Stick/round/AMOLED and mapped new sale items through ShopFlow. Independent review found closed-day conversation and partial-save outcome issues; both fixed with actual App and injected-store regression coverage. Firmware/toolchain and physical touch/performance checks remain unavailable in this environment. No commit, push or device access.
