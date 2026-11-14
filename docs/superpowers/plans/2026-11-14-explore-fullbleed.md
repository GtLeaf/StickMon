# Full-width exploration selector implementation plan

User chose translucent circular arrows after approving the layout preview. Execute inline; no commit/push/flash. Baseline84e50b4, branch feat/explore-fullbleed. Existing legacy compile switch remains available.

Goal: match full-width preview: 368×380 scene behind the public navigation, top black gradient, light name badge, native1× grouped sprites; market status only进行中/X天后举行 and shared出发 button. Remove both indicators. Circle diameter44, centers32/336,y176; preserve original public header positions and Back hit area.

Architecture: reuse bounded layout and generated row-RLE art, add a small clock for discrete3s paging/manual5s pause. Keep current frame limits and gameplay/save rules. Navigation paints above the backdrop without clearing its gradient. Add举 glyph without changing existing glyph bitmaps.

Review focus: loading and single-page pools stay idle; pointer-down, cancellation and display pause never trigger accidental paging/departure; clocks wrap safely; glyph and native sprite sizes remain valid; full and one-row/byte-swapped rendering agree.

- [x] RED: native layout boundaries + clock cases (3s, manual5s, off/single-page/loading/pointer, rollover).
- [x] Implement shared full-width geometry, clock/input integration, art and translucent circular rendering. Add only required font glyphs.
- [x] Update real App checks for new pixels and automatic paging; run new/legacy host flows, partial rendering, architecture and render-demand checks. Inspect actual screenshots; independent review.
- [x] Update spec and verification evidence; keep code uncommitted and preserve baseline/legacy rollback.

Ruling: use the established clean checkout on a separate feature branch, preserving preview artifacts and the user's existing workflow. Scope and execution are already authorized by the accepted previews and final style choice.
