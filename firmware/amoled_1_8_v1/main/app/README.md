# AMOLED page controllers

V1 and V2 use the same controllers. Native tests and the Web simulator compile
the same sources through `sources.txt`.

- `SettingsController`: settings hit testing, gestures, confirmation state and
  the settings view model. App applies hardware changes and saves.
- `TeamController`: roster/status/move page state, swipe/detail animation,
  selection transitions and view models. App calls shared gameplay services.
- `InventoryController`: bag/shop state, inertia, item selection, confirmation
  availability and view models. App routes targets and performs transactions.
- `ShowerController`: wash session and tool gestures, shared foam simulation,
  water/hearts/reactions and the shower view model. One-shot effects report
  stage rewards, completion recovery, sound and redraw needs; App applies
  shared gameplay services, progression navigation and deferred save retries.
- `HatchController`: countdown, egg gestures, interaction counters, heart/shake
  feedback, IMU scheduling/detection and the hatch view model. App performs
  hardware calls, saves progress and commits the completed game before clearing
  egg progress. The pending starter stays in App across failed save attempts.

Controllers must not access App, hardware, storage or render caches. Keep
gameplay rules in `src/game/`. App owns touch arbitration, navigation, redraw
requests and save boundaries. Mutable `state()` is a transitional surface for
the existing touch and feedback flows; prefer controller methods for complete
page transitions.

Consume shower effects with `takeEffects()` after each operation. Reward
feedback returns through `rewardFeedback()` without reissuing the stage. For
hatch, acknowledge successful IMU and progress-save operations explicitly;
failed operations must remain eligible for retry. Menu navigation pauses the
hatch clock, and leaving/completing hatch disables its IMU request.

Add application sources to `app/sources.txt`, renderer sources to
`ui/sources.txt`, and portable shared sources to
`firmware/common/amoled_shared_sources.txt`. Board implementations and
ESP-IDF-only code stay in their own targets. Do not duplicate these lists in
native or Web build tools.

Behavior checks:

```sh
python3 tools/test_amoled_page_controllers.py
python3 tools/test_team_inventory_controllers.py
AMOLED_RENDER_DEBUG=1 python3 tools/test_amoled_native_render.py
python3 tools/test_amoled_source.py
python3 tools/test_architecture_boundaries.py
```

The first suite drives public App touch/update/render entry points and checks
partial renders against full frames. The second checks controller transitions
and shared-service contracts. Device display, touch and power behavior still
require hardware validation.
