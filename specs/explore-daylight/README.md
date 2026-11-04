# 探索昼夜光照

更新：2026-10-26。共享时间／颜色规则已实现，硬件性能结论仍以原批次边界为准。

## 行为要求

1. While exploring outdoors, the world shall show dawn 04:00-06:00, daylight
   06:00-18:00, dusk 18:00-20:00 and night 20:00-04:00. Thus the lit day,
   including its transitions, lasts sixteen hours and full night eight hours.
2. During dawn/dusk, colors shall change continuously with game minutes,
   including across midnight, without changing the game clock or save schema.
3. Maps, party, wildlife, grass, shadows and pickups shall share world lighting;
   names, buttons, dialogs, toasts and text shall retain their normal colors.
4. Frost Crystal Cave shall retain its authored lighting at every hour. Indoor
   screens and battles remain outside this exploration-only first batch.
5. AMOLED V1/V2 and StickS3 shall use shared time/color rules. Paused AMOLED
   exploration shall retain its last lighting until resumed. Stable colors
   shall not schedule repeated map redraws; changed colors repaint only the map.
6. Partial frames, camera changes and grass/wildlife motion shall match full
   rendering without accumulated tint, stale pixels or modified cached art.

## 设计与所有权

Use a pure shared `ExploreDaylight` header for a daily timeline of RGB channel
gains. Linear interpolation joins night -> soft dawn -> neutral day -> warm
sunset -> blue night. Frost Crystal Cave returns identity gains. No RNG,
assets, save fields, offline-time policy or ecology behavior changes.

Canvas565 applies gains through three small RGB565 channel lookup tables,
respecting logical clipping, raster scale and byte order. Apply once after
world/actors/foreground are composed and before overlays/HUD. The static map
cache remains neutral, so partial redraws reconstruct and recolor each damaged
pixel exactly once. Daytime identity bypasses all pixel processing.

AMOLED ExplorePresenter keeps the last displayed tone as transient route state, initializing it
when generating a map. Clock updates compare the active outdoor tone and mark
the map only when gains change. Pause/menu/exit/fade preserve the prior tone;
resume catches up. StickS3 applies the same rules before its walking HUD.

Validate timeline boundaries, all RGB565 colors, clip/scale/byte-order handling,
AMOLED full/dirty parity with camera/cover, unchanged UI and cave pixels, actual
clock-driven damage and pause behavior. Export real map previews for visual
review. Build all three firmware targets; hardware performance stays unverified.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Shared long-day timeline and cave lighting exception. (1, 2, 4)
- [x] Clipped RGB565 color transform and both exploration renderers. (3, 5, 6)
- [x] AMOLED clock change detection and pause/resume handling. (2, 5)
- [x] Simulation/render tests and visual previews. (1-6)
- [x] Three firmware builds and documentation. (5)


Validation (2026-10-17):

- Shared timeline and RGB565 checks passed: all six areas, daily wrap and
  boundary continuity, every 16-bit color under four tones in both byte orders,
  negative/cropped rectangles, clips and raster scales 1/2/4.
- All 48 native AMOLED tests passed. Added 288 wildlife dirty/full comparisons
  across six real generated maps and four times, exact band reconstruction,
  neutral-cache reuse, unchanged HUD/dialog colors and cave equivalence.
- App tests passed initialization, clock-driven map-only lighting damage,
  pause/resume, stable-night idle and cave behavior. Expanded grass coverage
  to 648 ordinary/high-grass pair frames across day/dusk/night and two cameras;
  partial/full pixels agree and rebound settles without accumulated tint.
- Existing canvas test, 12 architecture, 18 exploration-screen and two render
  demand checks passed. `git diff --check` passed.
- StickS3 release and AMOLED V1/V2 Lite builds passed. Actual generated-map
  comparisons and nighttime grass previews were visually reviewed.
- No firmware flashing or hardware FPS/display validation performed.
