# Computer expansion card

Approved by the user after native SDGoods and Waveshare previews, including the 13000 maximum price.

- Computer starts at 20 stored companions. Nine upgrades add 20 each, maximum 200.
- Prices: 1000, 2500, 4000, 5500, 7000, 8500, 10000, 11500, 13000.
- Kecleon always offers one expansion card per market period until maximum capacity. This is an additional fixed offer, outside the existing eight-row supply plan; advice never changes it.
- Purchase applies immediately, with no bag stack. The current period becomes sold out; reopening/rebooting cannot replenish it. Maximum capacity removes the offer.
- Both interfaces use the approved minimal detail: card icon/name, before-and-after capacity, purchase price. Success: card icon, 扩容成功, before-and-after capacity (new number green), 离开. Existing market coin animation applies after durable payment.
- Capacity, spending and quota commit together and roll back on save failure. Old saves preserve all companions/progress and start with zero upgrades.
- Computer counts display used/unlocked capacity. All acquisition paths respect unlocked capacity; iterating/browsing existing contacts respects physical maximum.
- No automatic Waveshare V1 or OSTB builds, no device flashing, no automatic commit/push.

Approaches: use a persistent upgrade count and an independent purchased-period marker; retain the original eight seasonal slots and append the expansion row dynamically. This avoids rewriting seasonal inventory or procurement quotas. Increase the fixed runtime companion array to 200 with explicit legacy-layout migration; use bounded versioned serialization and heap snapshots to protect device stacks.


Persistence uses the existing userdata volume after the first expansion; the24KiB NVS partition keeps ordinary20-slot saves and a small archive marker. Activation publishes a complete snapshot before the marker. Marked archives require at least one CRC-valid file; failed scans/mounts do not restore stale NVS state. New saves replace the alternate of the newest valid file, then mirror it. Reset first clears NVS (the logical reset), then removes invisible orphan files without touching photos. OTA health validation uses this same read-only snapshot scan. No partitions change.
