# Tasks

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
