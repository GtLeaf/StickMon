# Progress

- [x] Verify full factory backup; keep credentials and binary images outside Git.
- [x] Create branch and independent ESP-IDF target.
- [x] Integrate extracted panel init, PSRAM framebuffer and shared DMA strips.
- [x] Confirm physical A=40 and B=39.
- [x] Native room, hatch, battle layout and full-screen backgrounds.
- [x] Provide no-IMU bath input and truthful unavailable battery display.
- [x] Separate game NVS and preserve shared save format/protection.
- [x] Native host checks: actual hatch/home/menu/map/battle/bath, lock input, power debounce and save readback.
- [x] Verify DMA ownership, source stride, dirty rectangles, errors and timeouts.
- [x] Build OSTB, M5StickS3, AMOLED V1 lite and V2 lite.
- [x] Flash game and resources; confirm user sees game; PSRAM/resources/30.6ms transfer in logs.
- [x] Recover factory GPIO1 active-high middle-key mapping; verify physical lock/unlock.
- [x] Verify hardware long-press power-off and long-press power-on.
- [x] Verify a debounced power press persists before hardware cutoff, then reload after power-on.
- [x] Verify native room coordinates round-trip and restore before movement resumes.
- [x] Confirm the final held-key pause and position restoration on hardware; user feedback passed and save/startup logs match at (132.00,123.00).
- [x] Defer panel display-on and backlight until the first full-frame transfer completes; keep saved brightness changes gated during initialization.
- [x] Confirm a cold power-on shows no startup garbage after the display sequencing fix; user verified black screen followed by normal game output.
- [x] Reuse AMOLED shadow registration for main/visitor room sprites, keeping pixel offsets outside room projection; verify actual shadow pixels for both directions and slither offsets.
- [x] Confirm corrected room shadow contact on the physical OSTB device; user reports the shadow is aligned with the feet.
- [x] Historical: validate the 2x native-art camera presentation; superseded by the centered Stick-size presentation below.
- [x] Restore Stick 1x room/actor/food/egg/shadow sizes; center the full 240x161 room at (28,39) with black margins and a fixed camera.
- [x] Clip world layers to the room and put clock/one-or-two-monster status panels in the top margin, with hints/notifications below.
- [x] Verify exact centered room pixels, black margins, fixed background during movement, real 1x main/visitor sprite bounds, mirrored/pose shadow registration, margin HUD placement and unchanged snapshot restoration.
- [x] Inspect hatch, home, bed, food, pair and two-monster HUD captures; build OSTB, StickS3 and AMOLED V1 lite.
- [x] Flash the centered room app only; verify its hash, successful startup and existing hatched snapshot readback without replacing resources or NVS.
- [ ] Confirm the centered room, Stick-size monsters and margin UI on the physical device.
- [ ] Software deep-sleep fallback and its wake path remain unverified; normal power-off uses the hardware controller.
- [ ] Verify speaker sound, battery calibration and radio against a second device.

## Screen touch extension

- [x] Add optional portable pointer events and release-only clicks with drag/cancel handling. (Requirements 11, 13)
- [x] Share one I2C bus between audio and the board touch reader; provide five-point setup and separate calibration storage. (Requirement 12)
- [x] Add main-monster, bowl, egg and menu hit targets, modal isolation and visible navigation controls while preserving A/B actions. (Requirements 11, 13, 14)
- [x] Verify calibration math, touch behavior, real scene rendering, lock isolation and OSTB/StickS3/AMOLED V1 lite builds. (Requirements 10-14)
- [x] Flash only the app and confirm this device's touch controller response (0x15, ID 0xb8). (Requirements 11, 12)
- [x] Diagnose the first rejected center check; add median sampling, visible retry guidance and center retries, and verify against recorded raw samples. (Requirement 12)
- [x] Complete five-point calibration on the physical screen; center error 2.60px and saved=1. (Requirement 12)
- [x] Confirm physical taps on the monster, bowl, menu and navigation controls; the user reports all normal. (Requirements 11, 13, 14)

## Adaptive direct-touch menus

- [x] Extract the shared adaptive main-menu renderer and preserve AMOLED geometry. (Requirement 15)
- [x] Add OSTB direct grid/detail menus for the main menu, bag and shop, with pointer feedback, drag cancellation and physical-key access. (Requirements 15, 16)
- [x] Verify geometry, interactions, rendering demand, gameplay actions and cross-target builds; inspect exported pages. (Requirements 10, 15, 16)
- [x] Flash only the app and verify calibration/resources/saves reload. (Requirements 4, 12)
- [x] Confirm the new main/bag/shop grids and detail navigation on the physical device; user reports display, clicks and scrolling all normal. (Requirements 15, 16)

## First submenu batch

- [x] Adapt room/food/computer entry to AMOLED headers and cards with scroll/press/back and A/B access. (Requirement 17)
- [x] Adapt settings sliders/list, help navigation and isolated reset confirmation. (Requirement 18)
- [x] Verify interactions, idle rendering, exported pages and affected firmware builds. (Requirements 10, 17, 18)
- [x] Flash only the app and verify startup, calibration and existing hatched snapshot reload. (Requirements 4, 12)
- [x] Confirm room/food/settings/computer-entry display and interaction on the physical device; user reports all normal. Reset acceptance covers viewing/Cancel, not clearing data. (Requirements 17, 18)

## Rounded-screen layout (remaining page groups deferred)

- [x] Record the corner-safe layout rules and provisional parameters in `safe-area.md`; documentation only, as requested on 2026-10-21.
- [ ] After the remaining migration features stabilize, verify the physical boundary, apply the shared safe-area rules across pages and complete device acceptance in one batch.

## Team list touch adaptation

- [x] Adapt member cards and modal actions to AMOLED interaction while preserving legacy status/moves, A/B and exploration return. (Requirement 19)
- [x] Verify popup isolation/cancellation, member operations, legacy subpage return, idle rendering, exported pages and firmware builds. (Requirements 10, 19)
- [x] Flash app only, verify calibration/save reload and request physical team-list feedback. (Requirements 4, 12, 19)
- [x] Confirm physical member-card/popup navigation and unchanged status/moves return; user reports all normal. Device reorder/Leave was not requested. (Requirement 19)

## Team subtree safe navigation (initial redesign superseded)

- [x] Implement opt-in shared safe geometry, team cards/popup and status/moves circular Back headers. (Requirement 20)
- [x] Add five-page status paging/content dragging, independent move list/detail dragging, safe footer, egg and modal/toast layouts. (Requirement 20)
- [x] Verify outside-safe pixel/hit bounds, paging/scrolling, physical reveal, modal cancellation, real forget/recall actions and static scheduling; inspect rounded-mask previews. (Requirements 10, 20)
- [x] Build OSTB, StickS3 and AMOLED V1/V2 lite; run render-demand/architecture checks. (Requirements 10, 20)
- [x] Flash only the app and verify calibration/save/startup. (Requirements 4, 12, 20)
- [x] Record user rejection of the status/moves redesign and replacement by requirement 21; requirement-20 visual acceptance is not claimed.

## Restore original status/moves with header only

- [x] Restore original status field/stat composition and split moves list/details/compact popup; remove both bottom navigation strips and their touch targets. (Requirement 21)
- [x] Keep shared corner-safe origin/header and enable swipe/title paging, overflow dragging and direct move-row actions through existing handlers. (Requirement 21)
- [x] Verify original body pixel identity, five-page navigation/overflow, no hidden bottom actions, touch/modal cancellation and actual forget/recall effects; inspect masked previews. (Requirements 10, 21)
- [x] Build OSTB/StickS3/AMOLED V1 lite/V2 lite and pass render-demand/architecture checks. (Requirements 10, 21)
- [x] App-only flash and verify original calibration/save/startup. (Requirements 4, 12, 21)
- [x] Record user rejection of the global 28px body inset; replace its horizontal constraint with requirement 22. (Requirement 21)

## Header clearance without body inset

- [x] Separate safe header bounds from the full-width body; restore normal team-card/popup/toast margins and edge hit tests. (Requirement 22)
- [x] Verify full-width original pixels, both card edges, edge move selection/status swipes, modal isolation and no hidden bottom controls. (Requirements 10, 22)
- [x] Inspect exported previews and build OSTB/StickS3/AMOLED V1 lite/V2 lite. (Requirements 10, 22)
- [x] App-only flash and verify existing calibration/save/startup. (Requirements 4, 12, 22)
- [ ] Confirm full-width body and inset top navigation on hardware. (Requirement 22)

## Reduce top navigation margin to 4px

- [x] Move shared header content and body start up 12px; keep full-width bodies and synchronized touch targets. (Requirement 23)
- [x] Verify native scene regression/exports and affected firmware builds. (Requirements 10, 23)
- [x] App-only flash and verify calibration/save/startup. (Requirements 4, 12, 23)
- [ ] Confirm the 4px top margin on hardware. (Requirement 23)

## Status indicator and home battery

- [x] Retain horizontal status paging and move the page dots to a bottom overlay without reserving content height. (Requirement 24)
- [x] Recover this device's ADC setup/curve and share the original AMOLED battery drawing in the home clock panel. (Requirement 25)
- [x] Verify native scene/ADC curve/HUD/overlay checks, AMOLED battery regression, affected firmware builds and previews. (Requirements 10, 24, 25)
- [x] App-only flash, verify real battery samples and existing calibration/save/startup. (Requirements 4, 12, 25)
- [ ] Confirm bottom indicator/swipes and home battery display on hardware. (Requirements 24, 25)
