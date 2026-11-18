# SDGOODS Photography and Album

User approved full migration and actual rendered previews on2026-11-18. Continue from a183f91 on feat/explore-photo-album, leave new work uncommitted. No device access, flash, push or merge. No V1/OSTB builds.

## Behavior

- Native360×360 round exploration gains a camera icon below the existing top map/menu controls. Keep original menu, HP arcs, bag and world camera.
-224px square crop starts at(68,68), drags after8px, remains fully within the circle centered179.5/179.5 radius180. No world pan/inertia; header controls retain priority. Retake preserves position; new session centers. Camera and preview stay awake automatically.
- Pause route advancement, encounters and their clocks, while wildlife/grass keep updating in viewfinder. Freeze in preview. Close/save resumes the exact route and camera without regenerating. Power-key lock discards unsaved draft and follows existing exploration lock behavior.
- Shutter takes clean world pixels, no HP/menu/toast/frame overlays, saves192×192 RGB565 with48×48 thumbnail and area/game-day. Preview offers retake/save; failed save retains draft; success returns to route with “照片已保存”.
- Computer adds相册 and count/5, two-column inertia-scrolled thumbnails, swipeable photo detail with location/day, confirm delete, correct empty/full/corrupt/unavailable states. Normal album idle policy. Reuse black style and existing fonts; ensure round font coverage.
- Existing userdata LittleFS holds independent scoped photos. No partition change; mount non-destructively, initialize only proven all-FF. Accurate blob missing/read-failure distinction, storage free-space check, temporary publish, old-photo protection, monotonically increasing IDs. Game reset retains archive like AMOLED.

## Architecture and Approaches

Reuse shared PhotoLibrary and file codec; extract AMOLED PhotoSession into presentation-layer PhotoFlow with geometry-driven crop/control/album bounds, keeping an AMOLED compatibility alias and existing APIs. RoundPhotoUi paints the round layout. Scene adapters coordinate route capture/resume and computer navigation. This avoids duplicating storage and gesture logic while keeping screen families' drawing separate. Copying AMOLED whole pages would impose rectangular dimensions; storing scene recipes would alter old photos when assets change.

## Geometry Contract

PhotoFlow::Geometry{width,height,mapHeight,frameSize,circular}; default368/448/344/256/false; round360/360/360/224/true. Retain defaultFRAME_SIZE/MAP_WIDTH/MAP_HEIGHT for AMOLED compatibility and expose runtime frameSize()/geometry(). All rectangles have x/y/width/height and contains(). Round control bounds: back84/24/44/44; shutter148/302/64/44; retake88/288/88/44; save184/288/88/44; trash232/24/44/44; delete dialog cancel76/208/100/44 and delete184/208/100/44. Round album bounds60/80/240/224, thumbs104px at70 or186, y84+row*116-offset,12px bottom padding. Public getters backBounds/shutterBounds/retakeBounds/saveBounds/trashBounds/cancelBounds/deleteBounds/albumBounds/thumbBounds. Preview image216px at72/68; detail image200px at80/76, metadata centered below. Crop rendered from common coordinates.

## Validation

Shared flow tests and full AMOLED regression after extraction; native actual SDGOODS camera -> crop -> preview/retake/save/reload -> computer album -> swipe/delete/full/fail/cancel/lock, clean pixels and frozen route vs active wildlife. Round crop corners always visible; dirty clip and swapped RGB565. Mock ESP LittleFS lifecycle/NVS scenarios plus real-file library tests. SDGOODS and AMOLED V2 SDK builds; actual rendered screenshots. Hardware timing remains unverified until user flashes.
