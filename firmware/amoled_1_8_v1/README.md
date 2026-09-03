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
- Open `COMM` from the main menu to host a visit room or search for an available
  room. The visit page supports join confirmation, remote pet sync, a timed
  visit with heartbeat/status exchange, explicit end, timeout and link-failure
  states through the shared `VisitSessionService`.
- Use the lock button beside the menu button to turn off the AMOLED; tap the
  screen to wake it.
- Read up to two formal team members in the bottom-right HUD. Each row uses the
  same shared hunger icon and HP percentage/color rules as M5StickS3; visitor
  monsters are excluded. `HomeHud` owns the shared roster/value rules and
  `HudRenderer` owns the shared hunger-icon rendering.
- Load all generated Pokemon `.smonsp` resource packs used by the M5StickS3
  firmware. A geometric fallback remains visible if a resource image was not
  flashed.
- Load the shared `zh16.smonfont` Chinese font and `ascii16-unscii.smonfont`
  from the SPIFFS resource partition. UTF-8 names, move names, and user-facing
  Chinese prompts use the shared 16x16 renderer; English labels keep the compact
  portrait font. V1 and V2 package the same font files, so new visible Chinese
  text only needs one shared font regeneration.
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
- Open `ROOM / FOOD` to choose one of the seven shared food types and inspect
  stock. The bowl on the home screen places the selected food using the shared
  `HomeCare` rules.
- Open `COMPUTER` for a read-only `STATUS` page and a draggable/inertial
  `STORAGE` list with up to 20 stored monsters. The `ESP-Claw` item starts a
  local Wi-Fi setup hotspot and configuration page on demand. Open `SETTINGS` to cycle
  brightness, volume, game speed, power-save timeout, and voice-call settings;
  each change is persisted immediately.
- Exploration steps resolve shared area encounters, pickups, ordinary regional
  boss pity, and the special encounter schedule. First Snorlax, Latias, Latios,
  roaming Latias/Latios, and the Mew event can reach the same route-end battle
  flow as ordinary bosses. Battles support basic, special, and charge moves,
  switching between two team members, reserve experience, medicine, and flee;
  boss victory updates regional progress and pity state. Level gains open the
  portrait progression flow for level-up, evolution, move learning, and full
  move-slot replacement. Defeat returns home with the fainted state preserved.
- Run the ESP32-S3 at 240 MHz and expand logical pixels through two alternating
  DMA buffers. Each transfer stripe contains 32 logical rows (64 physical
  rows), allowing 2x scaling for the next stripe to overlap the current QSPI
  transfer.

The AMOLED exploration loop now includes capture/friendship prompts, route
blocks and puzzles, battle animation/audio cues, the portrait progression
settlement flow, and the user-facing communication page. Route events update
steps, inventory, coins, battle HP, experience, progression, regional boss pity,
and special-boss progress through the shared rules. Remaining work is
V1/V2/StickS3 hardware interoperability testing and hardware-specific low-power
services.

The ESP-IDF platform now provides NVS A/B saves, SPIFFS resources, PSRAM
allocation, audio codec access, and the real ESP-NOW transport used by the
shared `EspNowLink` protocol. The communication scene starts the shared visit
session flow, while two physical devices still need an end-to-end V1/V2
interoperability check. The current V1 BSP does not expose calibrated battery,
QMI8658 IMU, or PCF85063A RTC services, so those capabilities deliberately
report unavailable instead of returning fake values.
The lock function powers down the panel while keeping the MCU awake at a
reduced polling rate; PMU-backed deep sleep remains a hardware validation task.

The V1 startup sequence resets the display and touch rails through TCA9554
before initializing SH8601 and FT3168. Touch reads are interrupt-driven from
GPIO21; continuously polling FT3168 while it is in monitor mode causes periodic
I2C timeouts.

## Build

```bash
cd /Users/gtleaf/project/esp/StickMon
./tools/build_amoled_variant.sh v1 claw
./tools/build_amoled_variant.sh v1 lite
```

The Claw and Lite builds resolve dependencies into their own build directories,
so building one variant does not rewrite the other variant's dependency graph
or commit machine-specific ESP-Claw paths. Their output directories are
`build-claw` and `build-lite`.

## ESP-Claw remote chat

This target includes a trimmed ESP-Claw runtime for WeChat and Telegram remote chat with a Coze agent. The
ESP-Claw source is kept outside this repository and is referenced by CMake. The
default path is `../espclaw/esp-claw-master` relative to the StickMon project;
set `STICKMON_ESPCLAW_ROOT` when it is stored elsewhere.

Before boot, write these binary blobs to the device NVS partition. NVS keys are
limited to 15 characters:

- namespace `wifi`: `ssid`, `password`
- namespace `claw`: `coze_token`, `coze_bot_id`, `coze_base_url`
- namespace `claw`: optional `wechat_token`, `wechat_base_url`,
  `wechat_cdn_url`, `wechat_acct_id`, `telegram_token`

`coze_base_url` defaults to `https://api.coze.cn`, and `coze_bot_id` defaults to
`7679649015453597711`. The access token is sent as a Bearer token. The current
device backend uses a stable anonymous `user_id` (`stickmon-device`); employee-
specific identities and Coze tool-call continuation are reserved for the next
iteration.

The WeChat and Telegram capabilities are enabled. The recommended setup path is
the device portal. The `微信登录` section obtains an iLink QR code, polls the
login state, and saves the returned token and account ID into NVS. The Telegram
field accepts the BotFather-issued bot token; no Chat ID is needed. The device
learns the Chat ID from the first inbound Telegram message and replies in that
chat. Both tokens are secrets and are never returned by the configuration API.

The LLM can read live state with `stickmon_get_context` and
`stickmon_get_inventory`, display a message with `stickmon_say`, start an
unlocked expedition with `stickmon_start_expedition`, recall the pet with
`stickmon_return_home`, open an ESP-NOW visit room with
`stickmon_invite_friend`, feed the pet with `stickmon_eat`, or buy one ordinary
 food item with `stickmon_buy_food`. An expedition started by an idle autonomous
 Agent request enables the normal route auto-walk and reuses the built-in battle
 AI until a player touches the device; a player touch immediately stops that
 automatic route and returns control to the player. Explicit WeChat commands
 only perform the requested state transition and leave the normal route screen
 under player control. The expedition route engine always uses its normal
 duration and encounter rules. Opening an invitation room does not claim that a
 friend has already joined.

The runtime does not copy or modify the ESP-Claw checkout. It compiles only the
core, lightweight memory, event router, WeChat, and skill-manager pieces;
CLI, Lua, MCP, system UI, scheduler, and unrelated IM capabilities remain off.

### Device setup portal

On the device, open `电脑 -> ESP-Claw`. The firmware starts a WPA2 hotspot
named `StickMon-XXXX` and opens a setup screen with a Wi-Fi pairing QR code
(scanning it joins the hotspot directly), the portal address, the hotspot
name, and its randomly generated password. Connect a phone or computer to
that hotspot — scanning the QR code does this on most phones — then open the
displayed address.
The portal is organized into modules: `Wi-Fi 配置`, `Coze 配置`, and
`IM 渠道`. The IM module manages channel tabs: use `+ 新建渠道` to add
微信 (WeChat QR login), QQ (App ID + App Secret), 飞书 / Feishu (App ID +
App Secret), or Telegram (Bot Token). Deleting a channel tab and then
pressing `保存配置` also wipes that channel's stored credentials from NVS.
The enabled channel list is stored as `im_channels` in the `claw` namespace;
`fillClawConfig` enables exactly the esp-claw caps that are both compiled in
(`CONFIG_APP_CLAW_CAP_IM_*`) and listed there.
For WeChat, use `生成微信二维码`, scan it with WeChat, wait for the success
state, and press `保存微信登录`. To test Telegram, open the bot in Telegram
and send it a normal text message; after the device receives it, the Agent's
reply is sent back to the same chat.
The portal page also shows a 设备状态 section with Wi-Fi / phone / ESP-Claw /
WeChat badges and a live log view (`GET /api/status`, `GET /api/log?since=`).
Both the badges and the log are fed by the shared 64-entry `ClawStatusLog`
ring buffer in `src/brain/`; long messages are split into continuation lines
at UTF-8 boundaries instead of being truncated.
On the device itself, the setup screen has two header tabs: `连接` (the
pairing QR page) and `日志` (a scrollable view of the same log plus a Wi-Fi /
WeChat summary line). When a phone joins the hotspot, the device switches to
the log tab automatically.
Secret fields left blank keep their existing values; App ID fields can be
cleared by submitting them empty.
Configuration is written to the `wifi` and `claw` namespaces one key at a time;
game saves are not touched.
Restart the device after saving so the normal STA connection and ESP-Claw
remote chat can start with the new values. The setup server is only started
after selecting the menu item, and returning from the setup screen stops the
HTTP server and hotspot. If ESP-Claw is already using STA, that connection is
kept alive while the setup hotspot is stopped.

Generate the NVS image with the repository helper after loading ESP-IDF. Keep
the output outside the repository because it contains credentials:

```bash
source /Users/gtleaf/.espressif/v5.5.4/esp-idf/export.sh
python3 /Users/gtleaf/project/esp/StickMon/tools/generate_claw_nvs.py \
  --ssid "$WIFI_SSID" \
  --password "$WIFI_PASSWORD" \
  --coze-token "$COZE_ACCESS_TOKEN" \
  --output /tmp/stickmon-claw-nvs.bin
```

For pre-issued channel credentials, add `--wechat-token "$WECHAT_ILINK_TOKEN"`
and/or `--telegram-token "$TELEGRAM_BOT_TOKEN"` before `--output`; the portal
QR flow is preferred for WeChat.

The NVS image replaces the whole NVS partition, which also contains the
StickMon game save. Prefer flashing it before the first boot on a new device.
For a device that already has progress, back up the partition first and note
that this helper cannot merge credentials into an existing NVS image yet;
flashing the generated image resets the game save. Do this again after any
full-chip erase:

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX \
  read_flash 0x9000 0x6000 /tmp/stickmon-nvs-backup.bin

python -m esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX \
  write_flash 0x9000 /tmp/stickmon-claw-nvs.bin
```

## Flash

```bash
idf.py -B build-claw -p /dev/cu.usbmodemXXXX flash monitor
```

For a repeatable build and flash flow, use the checked-in helper:

```bash
./flash.sh --port /dev/cu.usbmodemXXXX --variant claw
```

The `claw` variant enables the WeChat capability. Use `--variant lite` only
when flashing the build without ESP-Claw.

After an interrupted flash or when the device still contains an older
application/resource combination, erase once before flashing:

```bash
./flash.sh --port /dev/cu.usbmodemXXXX --variant claw --erase
```

Keep `bootloader.bin`, `partition-table.bin`, the application, `resources.bin`,
and `flash_args` from the same `build-claw/` or `build-lite/` directory. Do not
combine files from different variant directories; an old bootloader can still print its
startup banner while launching a newer application, which makes the failure
look like a display problem.

The `flash` target writes both the application and the resource image
(`active.json`, the dev manifest, all generated `.smonsp` sprite packs,
`standard.smonroom`,
`maps.smonfx`, `ui.smonfx`, `zh16.smonfont`, `ascii16-unscii.smonfont`, all
generated `.smonaudio` files, and all generated `.smoncry` files). The
16 MB partition table reserves two 3 MB OTA slots and a stable 4 MB resource
partition so later resource expansion does not require another layout change.

The first on-device check should confirm display orientation, pixel colors,
full-screen alignment, touch orientation, menu dragging, lock/wake behavior,
real route tiles, camera following, route pause/resume, and route exit.
