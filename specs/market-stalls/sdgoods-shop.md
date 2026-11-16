# SDGOODS market foundation and Kecleon

2026-11-16. User approved first batch: common transaction/lifecycle foundation,
round Kecleon purchasing/advice/box/coin feedback. Preserve current Debug-only
market entry; Gengar and Chansey remain ordinary dialogue in this batch.

- Reuse MarketShop/MarketSupply/MarketSchedule, ItemInventory, SaveCodec and
  MarketTransaction. No new gameplay rules, ItemIds, save schema or random calls.
- GameEngine saveNow reports game snapshot durability independently of encounter
  history. Coordinator still retries either failed persistence component.
- Register Kecleon as a service only on ROUND_TOUCH. Wait for facing readiness,
  persist the shelf before opening it, consume the opening gesture, stop walking.
  All active panel input belongs to the panel, including drag/cancel/close.
- Native360x360 circle. Paper/border/ink reuse market colors. Sheet top144,
  with content centered inside the circle. Brand and coins near y52. Shelf
  viewport x60/y152/240x152,64px rows, inertia via MarketListScroll. Main labels
 20px smooth, descriptive text16px wrapping. Buttons x84/188,y288,88x40; single
  action x80/y288/200x40. No player-facing technical captions.
- Home two icon rows: purchase/advice; advice three categories with pending tick.
 8slots/16units, concrete random nutrient/berry, stock and sold-out rules unchanged.
  Detail shows item name,x1,price,wrapped description,stock; no held/coin comparison.
  Outside panel returns one level; home outside closes; result returns shelf;
  advice result returns home; failed initialization only permits leaving/retry.
- Same paid-before-animation semantics. Box arrival600ms, click wait, star burst
 1250ms; reward already saved, no animation payout. Reuse MarketBoxPerformance
  with an opt-in stage layout (default AMOLED coordinates unchanged).
- Reuse MarketCoinMotion and parameterized HUD positioning: right288,y52,20px
  balance and24px signed tags in two lanes above the sheet. Only committed
  balance changes animate; slow saves cannot consume animation time.
- Common stock/period, full bag, insufficient coins, stale quote, total/partial
  save failures and advice changes retain existing rules. No replay/restart
  rewards, rerolls or repeated charge. End conversation once panel closes;
  lock/return/exit clear presentation and gestures, retain durable purchases.
- Preserve current66ms market cadence and static UI scheduling. No V1/OSTB
  builds, flashing, commits/pushes or changes to other market services.

Verification: actual round GameEngine/ExploreScene input journeys plus pure UI,
64 shipped resources, failure injection, two currencies/frame orders/clips,
legacy AMOLED shop/box/coins regression, SDGOODS Debug and V2 builds.
