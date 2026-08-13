# StickMon AMOLED 1.8 V1

Minimal ESP-IDF firmware for the original Waveshare
ESP32-S3-Touch-AMOLED-1.8 board:

- Display: SH8601, 368x448 QSPI AMOLED
- Touch: FT3168 over I2C
- Target framework: ESP-IDF 5.5.x

The current milestone initializes the display and FT3168 touch controller,
mounts a SPIFFS resource partition, and runs the first portrait interaction
layer:

- Tap the pet to trigger the petting response.
- Tap the bowl to place the selected food from the real room inventory.
- Use the bottom-left menu button to open the main menu. The adjacent lock
  button turns off the panel.
- Drag the menu vertically and tap the top-left back button to return.
- Share `AppSceneFlow` with the M5StickS3 firmware for semantic scene state,
  menu return state, item order, destination, and icon mapping. The AMOLED
  firmware no longer maintains a private view enum or numbered menu list.
- Match the release M5StickS3 main menu order (`EXPLORE`, `TEAM`, `ROOM`,
  `BAG`, `SHOP`, `COMPUTER`, `SETTINGS`, `BACK`) and render the same 40x40
  `MenuAssets` icons in the portrait scrolling list.
- Open `EXPLORE` from the main menu to use the portrait area selector. The
  list supports drag/inertia scrolling, shared sequential unlock rules,
  locked rows, selection, back navigation, and the top-right menu button.
- Tap an already selected unlocked area to enter its generated route. Tap the
  map to start or pause automatic walking; the portrait camera follows the pet
  across the shared 16x12 tile world. The top-right menu pauses route time,
  back opens a stay/exit confirmation, and tapping at the route end returns to
  the area selector.
- Use the route's portrait exploration menu for `TEAM`, `BAG`, `END`, and
  `BACK`. Its semantic order, destinations, and `MenuAssets` icon indices are
  shared with the M5StickS3 firmware. Header back and `BACK` resume from the
  exact paused position; `END` returns to the area selector. `TEAM` and `BAG`
  open their shared portrait subviews and return to the still-paused
  exploration menu.
- Open `TEAM` from the main or exploration menu to view up to two member cards
  with the shared sprite, level, HP, hunger, and major-status values. Tapping a
  formal reserve member opens a confirmation to move it to the front. The
  shared `TeamRoster` mutation is also used by M5StickS3; AMOLED refreshes its
  home behavior/sprite state and saves immediately after a leader change.
- Open `BAG` for a drag/inertia inventory list using shared item icons, names,
  counts, and descriptions. Recovery, status-cure, and revive items use shared
  `ItemInventory` rules and save immediately. Items whose dependent flows are
  not migrated remain visible but are never consumed.
- Open `SHOP` for StickS3-aligned `DAILY`, `EXPLORE`, and `SELL` categories.
  Product unlocks, prices, stack limits, candy daily limits, coin mutations,
  and owned counts come from shared `ShopService`/`ItemInventory` rules.
  The category screen keeps StickS3's category/icon split, while each product
  screen uses a draggable icon rail beside the selected item's details and
  action button. Scrolling snaps to the nearest shared 36x36 item icon.
  Purchases and sales require touch confirmation and redraw only rows 28..224.
- Open `ROOM`, then `SHOWER`, to use the portrait bath flow. Choose one of the
  three soaps from the real shared inventory, drag the soap across the pet,
  then drag the brush across it. Progress counts only pointer travel whose
  previous and current points are both inside the pet body region; holding a
  tool still does not advance the stage. `RINSE` runs a short automatic wash.
  Soap consumption, care-experience caps, stage experience, mood rewards, and
  unfinished-bath exit confirmation follow the shared StickS3 rules. The
  AMOLED interaction does not use the IMU.
- Use the lock button beside the menu button to turn off the AMOLED; tap the
  screen to wake it.
- Read up to two formal team members in the bottom-right HUD. Each row uses the
  same shared hunger icon and HP percentage/color rules as M5StickS3; visitor
  monsters are excluded. `HomeHud` owns the shared roster/value rules and
  `HudRenderer` owns the shared hunger-icon rendering.
- Load Bulbasaur from the same `.smonsp` resource pack used by the M5StickS3
  firmware. A geometric fallback remains visible if the resource image was not
  flashed.
- Load and save the shared `GameState` record in ESP-IDF NVS. Level, HP, mood,
  satiety, food stock, and bowl state survive restarts and use the same
  `SaveManager` format as the M5StickS3 firmware.
- Use the shared `HomeCare` rules for petting and placing food in the bowl.
- Advance the shared game clock and `CareTicker` rules while the firmware is
  running, including hunger decay, mood drift, home HP recovery, and daily
  care counter reset.
- Reuse `MonsterMind` for lightweight autonomous movement. When the pet is
  hungry and food is available, it walks to the bowl and consumes food using
  the same bite, taste, satiety, and mood rules as the M5StickS3 firmware.
- Reuse the shared `240x161` `standard.smonroom` world. The portrait home view
  renders a `184x148` camera viewport that follows the pet while retaining the
  original walk polygon, bowl anchor, and world-coordinate interactions.
- Use the shared `RoomMovementArea` rules for the home pet. Its footprint is
  derived from the current sprite dimensions; initial placement, wandering,
  food approach, and every movement segment stay inside the room polygon,
  including concave boundaries.
- Reuse `ExploreMapGenerator`, `ExploreAreaCatalog`, and
  `ExploreRouteGeometry` for route generation, tuning, and world coordinates.
  Route tiles come from the same `maps.smonfx` pack as the M5StickS3 firmware,
  with geometric fallback drawing when the map pack is unavailable.
- Redraw and transfer only invalidated row bands. Clock, room, status, and menu
  content updates remain independent; moving routes redraw rows 24..224 at a
  90 ms cadence, while paused routes do not refresh continuously. Page
  transitions and wake-up request a full frame.
- Keep the bath screen demand-driven as well. Tool movement redraws rows
  28..224, rinse redraws the bath content at an 80 ms cadence for about 1.8
  seconds, and an idle bath screen does not continuously refresh.
- Run the ESP32-S3 at 240 MHz and expand logical pixels through two alternating
  DMA buffers. Each transfer stripe contains 32 logical rows (64 physical
  rows), allowing 2x scaling for the next stripe to overlap the current QSPI
  transfer.

Explore pickups, encounters, battles, route blocks, rewards, results, audio,
and the full game engine are not connected yet. Route walking is local
presentation only: it does not change exploration progress or grant rewards.
Bath care experience updates level and HP, but the portrait level-up,
evolution, and move-learning presentation is still pending. `ROOM / FOOD`,
`COMPUTER`, and `SETTINGS` currently show a migration placeholder.
The lock function currently powers down the panel while keeping the MCU awake
at a reduced polling rate. PMU-backed low-power sleep will be added after its
wake sources are validated on V1 and V2 hardware.

The V1 startup sequence resets the display and touch rails through TCA9554
before initializing SH8601 and FT3168. Touch reads are interrupt-driven from
GPIO21; continuously polling FT3168 while it is in monitor mode causes periodic
I2C timeouts.

## Build

```bash
source /path/to/esp-idf/export.sh
idf.py -B build-v1 build
```

## Flash

```bash
idf.py -B build-v1 -p /dev/cu.usbmodemXXXX flash monitor
```

The `flash` target writes both the application and the resource image
(`active.json`, the dev manifest, `001.smonsp`, `standard.smonroom`,
`maps.smonfx`, and `ui.smonfx`). The
16 MB partition table reserves two 3 MB OTA slots and a stable 4 MB resource
partition so later resource expansion does not require another layout change.

The first on-device check should confirm display orientation, pixel colors,
full-screen alignment, touch orientation, menu dragging, lock/wake behavior,
real route tiles, camera following, route pause/resume, and route exit.
