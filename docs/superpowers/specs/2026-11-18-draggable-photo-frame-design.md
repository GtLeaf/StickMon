# Draggable Photo Frame

Approved by the user's “执行，然后给我效果渲染图，包括取景框不在中间的场景”. Continue in the existing photo feature branch; preserve its uncommitted work. No commit, push, device access or flash.

- Fixed256×256 square in the368×344 native map, initially(56,44), clamped to x0..112/y0..88. Keep the existing camera controls and black theme. Four white corner marks, dark translucent exterior, no added labels/handles.
- A press inside the square captures the gesture except header controls. Beyond8px movement, translate relative to its original position. No long-press, inertia, snap or world camera pan. Releases over shutter/back never activate those controls. Cancellation/outside-screen releases end the gesture; stale events cannot restart it.
- Keep position for retake/back from preview, reset upon a new camera session. Single frame coordinates drive rendering and clean-world192×192 downsampling. Capture excludes overlays and preserves the whole chosen composition. Album stays at5 images.
- Coalesce MOVE redraws into the existing66ms photo update; redraw map rows0..344 for a moved frame. UP still renders the final state immediately. Album keeps its own80..448 dirty rows. No new pixel buffers or storage format changes.

## Approaches

Use a movable crop within the existing viewport, as proposed and approved. Panning the world instead would change the exploration camera; adding corner handles would add unnecessary controls and imply resizing. Both are outside this change.

## Verification

Actual App tests:256 mask, sub-threshold movement, arbitrary touch offset, every edge, outside press, controls priority, drag through controls, cancellation/stale events, no inertia, coalesced dirty rows, retake/re-entry, clean moved capture and persisted readback. Full existing photo journey including5/full, errors, album, lock and dirty bands/byte orders. Render centered, upper-left and lower-right with matching captured preview. Native tests plus AMOLED V2 build only; no V1/OSTB or unrelated SDK build.
