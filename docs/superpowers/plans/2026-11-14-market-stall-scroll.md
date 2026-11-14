# Market Stall Scrolling and Layout Implementation Plan

> **For agentic workers:** Execute approved requirements directly; independent rule, tutor UI and shop UI tasks use dispatching-parallel-agents. Root integrates shared scrolling and one independent final review. No commits/pushes until requested.

**Goal:** Remove market exploration gates and deliver consistent momentum scrolling, outside-panel tutor navigation and compact shop information.
**Architecture:** Keep the existing state/schema and pacing. A shared UI-only MarketListScroll owns gesture capture, recent speed and frame-independent deceleration; each existing stall keeps its scroll position and supplies viewport/bounds. MarketShop independently drops exploration gating while ordinary shop rules remain unchanged.
**Tech Stack:** C++17, existing Canvas565/368x448 stall panels, native ASan/UBSan/App hosts.
**Spec:** `specs/market-stalls/list-interaction.md`. User approved the five-point UI proposal and explicitly cancelled exploration gates. Result Leave button remains; only tutor Back buttons removed. Battle potion stall diagnosis is separate, no fix in this task.

## Global Constraints
- Tutor/shop panel y208,14px corners; retain care panel y168 and current33ms/frame limits unchanged. No refresh increase, no overscroll bounce. Visual inertia stops on press, page exit, cancel/lock and bounds; a brake tap never selects.
- Each market opening has8 positions/16units from the first day; fixed candy1 and box bonus independent. Ordinary shop progression, prices, weights, stock and save rollback stay intact. Preserve already saved current-period stock, no replenishing sold slots on upgrade; new openings initialize full supply.
- Existing save/schema unchanged. No new dependencies or device access. Evidence under .stick-local/artifacts/market-stall-scroll-20261114/; work on feature branch in current mounted checkout, preserving existing assets/evidence.
- Shop rows keep72px; remove stock text, center name/price, sold-out names/prices struck and rowgrey, noninteractive but draggable. Details show icon/name, two description lines, stock(or shared choice allowance), Back/Buy. Remove owned/coin transitions and box quota sentence.
- Tutor removes every Back footer/hit target; outside returns one level, menu outside closes. Primary action becomes wide centered; recall viewport216..440 shows4rows. Results keep Leave; existing performance skip/commit behavior retained.

## Review Focus
- Flick braking, stale release/cancel, drag out/back to origin, timestamps and limits must not select, buy or navigate.
- Inertia really schedules drawing under the existing cadence and fully stops at rest/lock.
- Sold-out/low-money/bag-full selection and transactions remain checked by service and save failure rollback.
- Shared nutrient/reset quotas, shelf position and scroll restoration survive child-page return.
- Fresh saves have full market supply while old current-period sold stock stays sold; ordinary shop remains gated.

## Tasks
- [x] Task1 Rules (independent): modify MarketShopService.h only and focused shop/box rule tests. Write fresh-save all8/16, full-weight box and ordinary-shop unchanged assertions RED, remove market progression checks, GREEN; old same-period stock retained.
- [x] Task2 Shared scroll (root): create src/ui/common/MarketListScroll.h and meaningful native host. API touch(event,Bounds,maxScroll,int&offset,TouchTap&)->bool(consumed); update(delta,maxScroll,int&offset)->bool(pixelChanged); cancel(); moving(). Recent100ms samples,10px drag capture, speed cap1400px/s, exponential friction10/s, stop30px/s. Slow releases do not fling; fast flick stops around0.2–0.45s. RED missing functionality before implementation; test at16/33/66ms, bounds, brake, hold, cancel, stopped redraw.
- [x] Task3 Tutor UI (independent): MarketTutorUi.h plus dedicated/related tests. Remove Back rendering/hits, widen actions, extend recall list. Preserve footer routing for outside navigation and prevent old footer from firing. Root adds shared scroll integration after worker finishes.
- [x] Task4 Shop UI (independent): MarketShopUi.h plus selector/season App tests. No row stock, strike disabled rows; compact description/stock layout and one-line box explanation. Shared category stock labels supplied by root UiStrings. Root adds shared scroll after worker finishes.
- [x] Task5 Integrate (root): replace all3listDrag implementations with shared scroll; update methods integrate inertia, cancelTouch stops; care drag capture unified. App existing changed→dirty path advances inertia while preserving frame cadence. Test real UI flick/stop/brake/outside scroll release on all3stalls and stopped scheduling.
- [x] Task6 Validate/docs: focused rules, native and complete App journeys, related tutor/care, save/architecture/assets/glyph coverage; export representative pages/scroll preview, inspect. Independent review, fix meaningful findings, update current specs and ledger. Report unavailable firmware/device checks.
