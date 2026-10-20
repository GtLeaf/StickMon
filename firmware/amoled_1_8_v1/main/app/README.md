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
- `ProgressionFlow`: level-up, evolution and move replacement session state,
  animation timing, gestures and game mutations through existing shared rules.
  One-shot effects request saving, team synchronization, sprite preloading,
  sound, redraw and completion. BattleSession owns the battle growth queue;
  App applies return navigation.

- `BattleSession`: combat phase, current battle slot, shared turn planning and
  action commits, switching, victory/defeat/friendship, and the bounded two-member
  growth queue. Reuses the portable battle/experience/effort/friendship services.
  The model is built from const session/timeline state.
- `BattleTimeline`: attack, HP/XP, food and switch presentation; bounded log FIFO;
  queued audio and first-frame acknowledgement. Each session owns an independent
  FIFO, allocated on its first log and reused, without allocating per frame.

- `ExploreSession`: one route map/pool, step and ice progression, pickups,
  encounters/Boss scheduling, route item effects, pause state and return rewards.
  Reuses portable game rules and emits fixed effects; its state is read-only.
  App retains resource preloading, storage and departure/return presentation.
- `ExplorePresenter`: route interpolation/follower motion, camera/frame clocks,
  ecology/grass/daylight, read-only models and bounded damage. Borrows the
  session map and game state; never owns a duplicate or mutates gameplay.
- `HomeFlow` (first batch): attention, ambient motions, window gaze and the
  main actor's post-feed clearance lifecycle. Owns action state and deadlines;
  borrows actors and the shared Simulation. App supplies room geometry and
  bowl-blocking observations, and retains camera/frame animation and damage.
  Companion survival and exploration door transitions remain in App.
- `PairFlow`: pair eligibility, scheduling, approach/invite/talk/chase/celebration,
  cancellation and visitor arrival talk events. Owns pair state; borrows GameState,
  two actors, Simulation and the shared VisitArrivalFlow. App supplies narrow
  environment/geometry observations and retains sprite-width lookup and presentation.

- `VisitorFlow`: host approach/open/clearance and guest crossing/landing/exit.
  Owns visitor geometry, deadlines and arrival/exit state; borrows GameState,
  actors, Simulation and the shared VisitArrivalFlow also used by PairFlow.
- `VisitAwayFlow`: the local pet's visit departure through the shared
  VisitDepartureFlow. Communication and actual departure notification stay in App.

Visitor/visit departure ordering contract:

1. App supplies room geometry and narrow communication observations. Both flows
   own their lifecycle state through const `state()` and use shared Simulation
   routes/resources. They have no App, hardware, storage or cache dependency.
2. Consume movement effects before `completeMovement()`, then consume phase
   effects. Landing fallback explicitly requests App's existing autonomous goal
   picker and acknowledges it once, preserving random call order.
3. Entry completion releases the guest door before App starts PairFlow welcome
   talk. Exit releases resources before App removes the guest/synchronizes caches.
   The direct movement tail resumes after terminal effects to preserve damage
   and frame ordering. Early exit during host approach releases the host door.
4. App retains frame clocks/camera/damage, debug formatting, communication,
   roster and save operations. Exploration-specific departure/return remains
   in App for the next batch. Effects use fixed storage and allocate no heap.

Pair interaction ordering contract:

1. Sprite-width spacing is read only when an eligible pair starts. Shared
   ActorController and Simulation retain eligibility rules, random selection,
   routes and arbitration. Flow owns parallel approach speeds, arrivals and
   stage/interaction deadlines; App exposes no mutable session state.
2. `update()` advances movement and emits facing/camera/frame/damage effects.
   Consume them before `completeMovement()`, which resolves arrivals/snapping,
   chase replanning and termination. Consume again after completion. Stationary
   invite/celebration phases do not introduce continuous redraw.
3. Completion releases both actors and emits one finished effect. App preserves
   the existing PRNG order: schedule the main decision, schedule the next pair,
   then grant each shared care reward and apply progression before mood/save.
   Cancellation emits no reward. Visitor talk advances the shared arrival flow.
4. Effects are fixed-size and consumable once; debug traces are formatted in
   App. The module has no hardware/storage/cache access and allocates no heap.

Home action ordering contract:

1. App keeps survival/control/pair/door eligibility and cancellation priority.
   Flow tries window gaze, due attention, then a due special action, preserving
   the existing random calls. Window anchors are read only when due, never as
   part of an active action's per-frame movement.
2. `update()` advances a route and returns its step. App consumes facing effects,
   updates camera/frame damage, then calls `completeMovement()` once with the
   fresh bowl-blocking observation. Arrival starts the next leg or stationary
   wait; window arrival records the timestamp and emits one save request.
3. Consume `takeEffects()` after each command/update/completion. Effects request
   animation resets, facing, hearts, next-decision scheduling, room damage and
   saving. Empty effects do no work; stationary waits do not request continuous
   redraw. Flow never calls hardware, storage or caches and exposes const state.
4. Feed completion retains the original bowl-clearance retry until the main
   actor stops blocking the bowl. Cancellation uses this same completion path;
   attention cancellation clears only its owned heart feedback. Actors, shared
   resources, navigation and game rules retain their existing owners.

Exploration ordering contract:

1. Begin prepares the run and any pity-slot save; App consumes effects before
   preloading route sprites/audio, then generates the first map. Consume map
   effects and reset presentation before starting a step on a new map.
2. `beginStep()` issues one move. Presenter interpolates it and returns a completion enum; App calls
   `completeStep()` once: recovery precedes cooldown/item accounting and events.
   Ice continues before resolving events; Boss/pickup precedes wild encounters.
   Consume effects before issuing another move. A pending encounter blocks
   additional moves/battle requests until App acknowledges battle closure.
3. Manual walk-to-interaction and automatic walking remain separate. Pickups
   stop manual walking; automatic walking continues. Manual walking stops one
   point before a Boss. App alone decides whether a completed automatic route
   should navigate home.
4. Pause retains its initial anchor. Resume returns the elapsed duration to
   rebase Presenter's movement/exit/map clocks and rebases the session's pending fall.
   Battle/growth use the same pause, resumed with a fresh clock after save/cache
   work. Door/fade acknowledgement stays in App's existing lifecycle.
5. Return settlement is guarded per run. App still requests the snapshot even
   if only steps changed; failed saves retry through SaveCoordinator without
   replaying the rewards. A new begin resets this guard. No map/GameState copy
   or per-frame heap allocation is introduced by the session.

Presenter damage contract:

- `takeDamage()` consumes the pending union once. Camera/map mismatches restore
  the whole map band; actor/ecology changes request native pixel rectangles.
  Static and paused updates do not produce continuous motion damage.
- App calls `recordDrawn()` at the existing route draw point, and invalidates
  the remembered bounds after presenting another scene. This records a draw;
  only App/board code acknowledges actual presentation/transfer success.
- App consumes motion damage before committing step/exit effects. A new map
  resets presentation before the next move. Fade, toast, scene eligibility,
  resource/cache operations and device clocks remain explicit App inputs.

Battle ordering contract:

1. AMOLED commits shared action damage/effects at action start. Displayed HP
   begins moving 280 ms later over 420 ms. Stick's adapter retains its own impact
   commit point. This refactor changes neither gameplay adapter's commit policy.
2. App consumes gauge effects before taking a fresh clock for attack updates.
   Session receives a narrow monotonic clock for action/switch anchors; no
   platform object enters the module. App calls `markPresented()` from its
   existing `markRendered()` path, and consumes pending cry audio once.
3. A switch retreats for 360 ms, changes only the battle slot, and requests
   sprite preloading. App acknowledges after preloading using a fresh clock;
   only then does the 360 ms entry start. Logs drain before the counterattack.
4. Victory has an explicit settlement guard. Active and reserve growth entries
   are consumed in order; App saves at the growth handoff and before route
   resume. Saving, navigation, sprite synchronization and audio stay in App.
5. Consume `takeEffects()` after operations and each update pass. Empty effects
   produce no redraw. A second consumption cannot repeat an external action.
   The log FIFO is 24 x 64 bytes plus 24 durations (1,584 bytes). Allocation
   failure displays the latest log with a bounded deadline, allowing continuation.
   It does not strand a battle behind an unavailable queue.

Controllers must not access App, hardware, storage or render caches. Keep
gameplay rules in `src/game/`. App owns touch arbitration, navigation, redraw
requests and save boundaries. `state()` returns a const snapshot; operations
and complete transitions belong to the owning controller or flow. Production
callers must not mutate page state through references or test accessors.

Input/update/presentation contract:

1. App arbitrates global capture and overlays, then sends the event to the
   active page. Entering/leaving the extracted pages cancels outstanding
   gestures; an orphan release never becomes a click. Hatch pauses its own
   countdown when navigating away. Growth retains its return context.
2. Update uses the supplied monotonic `uint32_t` clock. Consume effects after
   each operation/update; consuming them twice must not repeat rewards, saves,
   sounds or completion. A completed growth session ignores further advancement.
3. Build read-only view models and draw only the requested damage. Evolution
   touch redraws include its animated area above the normal page header.
4. `markRendered()` acknowledges the caller's presentation path and clears
   damage. Keep existing fade presentation and battle audio ordering; drawing
   a model alone does not acknowledge hardware/IMU/storage success. Board lock,
   wake and brightness paths retain their existing acknowledgement behavior.

Save scheduling stays in App. Ordinary five-minute saves retain their existing
eligibility; successful saves advance the timer. Failed snapshot or encounter
history writes remain pending in the shared `SaveCoordinator` and retry after
its quiet interval when no pointer is held, including exploration/battle scenes.
Shower keeps its initial reward debounce and uses the same failure interval.
Critical hatch commits remain synchronous: save the completed snapshot before
clearing egg progress. Reset, unavailable storage and newer-save write guards
still apply. No save schema or resource format changes are needed.

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
python3 tools/test_amoled_progression.py
python3 tools/test_evolution_sequence.py
```

The first suite drives public App touch/update/render entry points and checks
partial renders against full frames. The second checks controller transitions
and shared-service contracts. Device display, touch and power behavior still
require hardware validation.
