# Design

Use a pure shared `ExploreDaylight` header for a daily timeline of RGB channel
gains. Linear interpolation joins night -> soft dawn -> neutral day -> warm
sunset -> blue night. Frost Crystal Cave returns identity gains. No RNG,
assets, save fields, offline-time policy or ecology behavior changes.

Canvas565 applies gains through three small RGB565 channel lookup tables,
respecting logical clipping, raster scale and byte order. Apply once after
world/actors/foreground are composed and before overlays/HUD. The static map
cache remains neutral, so partial redraws reconstruct and recolor each damaged
pixel exactly once. Daytime identity bypasses all pixel processing.

AMOLED keeps the last displayed tone as transient route state, initializing it
when generating a map. Clock updates compare the active outdoor tone and mark
the map only when gains change. Pause/menu/exit/fade preserve the prior tone;
resume catches up. StickS3 applies the same rules before its walking HUD.

Validate timeline boundaries, all RGB565 colors, clip/scale/byte-order handling,
AMOLED full/dirty parity with camera/cover, unchanged UI and cave pixels, actual
clock-driven damage and pause behavior. Export real map previews for visual
review. Build all three firmware targets; hardware performance stays unverified.
