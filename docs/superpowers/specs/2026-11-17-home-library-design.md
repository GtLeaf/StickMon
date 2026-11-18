# Home Library

The user approved the fixed-position library/preview flow and requested execution on2026-11-17, preserving the current black UI and naming both the room entry and library `家居库`.

- Add `家居库` as the third room-menu action on SDGOODS and AMOLED touch. Food/shower and their returns retain existing behavior. StickS3 remains unchanged.
- Library lists owned, known furniture with icons, names and `已摆放`; preserve scroll/filter on preview return. Use shared inertia/tap cancellation. Show category controls only when multiple owned categories exist. With no furniture show `暂无家居`.
- Current production catalogue contains only the Kangaskhan photo frame (stable ID0, existing ownership bit0). Demo beds/rugs are not unlocked or sold. Future catalogue entries use stable IDs and explicit fixed slots; same-slot replacement retains all ownership.
- Selecting an item previews it at its fixed authored wall position, with the entire room image visible. Confirm `摆放`/`更换`; an already placed removable item offers `收起`. Back cancels preview. Saves precede committed feedback; failed saves restore previous state and allow retry.
- Persist placement separately in existing `room.placedFurniture`; preserve unknown furniture bits and other save fields. No schema/version change. Actual home background and day/night use saved owned placement, not a retained preview.
- Reuse room background cache without new framebuffers or resource loads per frame. Add frame drawing to shared presentation, retaining approved gift appearance. No refresh cadence increase or theme recoloring.
- Cancel held touches on back, lock, standby, home and reentry. Rendering must preserve clip/buffer byte order and invalidate cache when placement changes.
- Verification is native and SDK only; no V1/OSTB builds, device access, flash, commit, push or merge.

The user explicitly clarified: retain the original right-wall frame position (logical x193/y54, size21), with no requirement that it is in the default camera on returning home. Do not relocate it near the window or adjust camera behavior.
