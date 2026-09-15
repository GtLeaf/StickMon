# AMOLED renderer modules

`sources.txt` is the shared renderer source list for both firmware boards and
the native host renderer. Paths are relative to the V1 `main` directory.
Add each new renderer `.cpp` here once. Firmware CMake watches this file and
reconfigures when it changes; the host tests read the same list.

`RoomScreens.cpp` and `SettingsScreens.cpp` compile independently. Their headers
own the page render and hit-test APIs. `HomeScreen.h` includes these headers to
preserve existing callers. New page implementations should include their own
header, required models and shared helpers, without including `HomeScreen.h`.

`ExploreMapRenderer.cpp` handles exploration world cache construction, viewport copying,
tile fallback rendering, animated tiles and ESP Debug performance statistics.
Only `drawExploreRouteMapLayer` and the map viewport bounds are public. The
route screen keeps responsibility for actors, pickups, the HUD and overlays.

`AmoledApp` owns a `RenderCaches` instance and passes the required `PixelCache565`
explicitly to battle, explore-selector and route renderers. Renderers no longer
keep global pixel buffers. Content keys include asset/seed identity, area variant,
dimensions and framebuffer byte order. `begin` invalidates content and reuses a
same-sized buffer; `commit` marks a complete image ready. Failed allocation leaves
no valid content and the renderer uses its normal fallback. Background asset-load
failures are not committed, allowing a later draw to retry the asset.

Before rendering, `retainForScene` releases a background when leaving its scene.
The exploration world survives menus, battles and progression dialogs, and is
released on the home or area-selection screen. `begin` on the application releases
all old caches. Destruction also releases them, through the allocator that created
each buffer. The platform allocator must outlive the application (or a host test's
local `RenderCaches` instance).

`UiCommon` owns the helpers used across compilation units. Page-only helpers
belong in the page's anonymous namespace. Models remain in `models/ScreenModels.h`.
The remaining `.inc` files still compile within `HomeScreen.cpp`; they are not
entries in the source list.

Each top-level renderer uses `UiCommon::PageClip` for the duration of its call.
`setRect` intersects a local viewport with the requested dirty rows on both the
page canvas and PixelRenderer's asset canvas. `reset` returns to the requested
rows, not an unrestricted canvas. Destruction clears both canvases, including
early returns. PageClip is not a nested clip stack and does not preserve an
external caller's clip: page rendering owns clipping for the call.

Use `drawPageHeader` for ordinary secondary pages. It owns the 76-pixel black
header, 52-pixel circular back button, 80-pixel back hit area, right-aligned title
and optional trailing value. Page content starts at `UiMetrics::CONTENT_TOP`.
`drawHeader` and `drawHeaderText` remain for scene HUDs and composite headers.
The home room, main menu, battle, exploration, progression, touch calibration and
ESP-Claw tab header keep their independent layouts. `drawToast` runs under the
page clip like other overlays.

Validate changes with `tools/test_amoled_*.py` and both boards' `lite` and
`lite debug` builds. Host Debug rendering does not compile ESP-only code, so
it does not replace a firmware Debug build.

## Behavior test migration

`tools/test_amoled_native_render.py` builds one host binary per test class using
this module's source manifest. Each behavior case runs in a separate process and
appears as its own Python test, so failure output identifies the affected contract.
`tools/amoled_ui_behavior_host.cpp` calls public render/hit-test APIs; it never reads
renderer source text or private layout constants. Expected coordinates describe
the current native 368x448 UI contract and intentionally do not reuse UiMetrics.

The first migration batch replaces source-pattern checks for:

- Explore departure/back button geometry, plus locked-selection pixels.
- Shop detail button geometry, plus pressed feedback and static progress frames.
- Settings slider thickness, endpoint/clamping behavior, pressed knobs and absence
  of changing numeric labels.
- Team popup separators and status-page navigation geometry/indicators.
- Main-menu columns, cell gaps, scrolling, empty final cells and header/scrollbar
  absence, exercised with both release and Debug menu counts.

Cache ownership/invalidation/scene retention and PageClip intersection/cleanup
checks also have individually selectable cases. The full/partial-frame test still
covers real pages, overlays and cache-allocation fallback rendering.

Run `python3 -m pytest -q tools/test_amoled_*.py` and repeat with
`AMOLED_RENDER_DEBUG=1`. To run a single contract, use pytest's `-k`, for example
`-k settings_slider`. The host binary accepts `DATA --case settings-sliders`;
unknown case names fail instead of silently passing. Existing `DATA OUTPUT_DIR`
PPM export is preserved.

This is an incremental migration. Most app touch sequencing, startup brightness,
board-specific submission, build wiring and remaining resource/text layout
checks still use source assertions until equivalent executable coverage exists.
Do not remove those checks just because the renderer cases pass. The host now links AmoledApp for expedition-transition coverage; other application
flows still need equivalent behavioral coverage. ESP touch headers supply only
opaque type declarations in the host build; no physical display/touch driver is
simulated or verified.

## Expedition scene transitions

`SceneFade` keeps opacity fixed for a rendered frame. A fade advances only after
`markRendered` acknowledges a successful full-screen submission. The black
endpoint must be submitted before the application switches scenes; the new scene
then submits its initial black frame before fade-in timing begins. Long loads and
slow transfers cannot consume the entire fade or skip all intermediate shades.

Departure closes menus to the home room, plays the door animation, fades fully
out, loads the route, then fades in. Return closes route menus, fades the route
out, settles the expedition once and goes directly home. The home room fades in
before the pet walks through the door. No return phase enters the area selector.
Route movement and transition-cancelling touches are suspended during fades.

`tools/amoled_expedition_host.cpp` drives real app update/render/touch calls with a
controlled desktop clock. It covers manual completion, the route-menu End action,
automatic completion, slow frames, missing/partial submission acknowledgements,
full-frame black pixels, repeated return requests and visible walking after
fade-in. It also reproduces the device loop where rendering/transfer takes time
but the next update shares the previous acknowledgement timestamp: repeated
initial-frame submissions must not reset the fade start time. Run with `python3 -m pytest -q tools/test_amoled_native_render.py -k expedition`.
