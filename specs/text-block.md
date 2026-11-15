# Shared bitmap TextBlock

Approved scope: automatic wrapping on the existing Canvas565 and font assets;
first adoption is Gengar learning/recall confirmation. Preserve colors,
32px primary copy, transaction behavior, and the accepted training/coin changes.

Use a shared, bounded, owning TextBlock with cached line layout. Support bitmap
16/32px, explicit width, optional height/max lines, line gap, left/center/right
alignment, wrapping, clipping and end ellipsis. Measurement and drawing consume
the same line records. Never shrink the font automatically.

Decode UTF-8 safely, normalize CRLF/CR and tabs, preserve explicit newlines and
blank lines. Prefer word boundaries for ASCII words/numbers. Use a small explicit
table of common CJK/ASCII opening/closing punctuation; backtrack to a legal break.
When a word or punctuation sequence cannot fit, consume characters safely and
report the forced break. This is not a full Unicode segmentation implementation.

Own at most 511 UTF-8 text bytes and cache at most 32 lines; report source or line
capacity overflow. Report full required line count/height separately from visible
lines. Clip fixed-height text; ellipsis is opt-in and uses existing ASCII dots.
Font measurements share Unscii/CJK advance rules with PixelRenderer, including
the half-width gender symbols. Explicit bitmap rendering ignores ambient font
mode, preserves clipping/state, and uses Canvas logical coordinates.

Cache invalidation occurs only on text/layout changes. Color and position do not
invalidate layout; no allocations or timers are introduced. Drawing honors and
restores the incoming clip; dirty-row reconstruction must equal a full render.

Gengar joins the existing sentences without inserting a newline. Learning uses
its existing centered 320px-wide body area; recall uses the existing left aligned
320px body area above the heart-scale hint. Both use fixed 32px text. If content
actually exceeds the reserved body height, allow body scrolling with the existing
inertial scroll helper; never overlap the footer or silently drop confirmation
copy. Outside back and the payment button retain their existing behavior.

Approach: prefer this small shared component over adopting an entire UI runtime
or extending each stall's private helper. Reference contracts:
https://lvgl.io/docs/open/9.5/widgets/label
https://developer.android.com/reference/android/text/StaticLayout.Builder
https://developer.android.com/reference/android/text/Layout

## API and ownership

Implementation: `src/presentation/TextBlock.h`; metrics:
`BitmapTextMetrics.h`; explicit raster adapter: `PixelRenderer::bitmapText`.
Pure text ownership/measurement now lives in `TextLayout.h`; it has no renderer
dependency. TextBlock inherits the same options, line and measurement API.

```cpp
Ui::TextBlock text;
Ui::TextBlock::Options options;
options.width = 320;
options.maxHeight = 96; // 0 means content height
options.font = BitmapText::Font::PX32;
options.align = Ui::TextBlock::Align::CENTER;
text.setOptions(options);
text.setText(message); // owns a normalized copy, including stack-built messages
const auto& measured = text.measure();
text.draw(canvas, x, y, ink);
```

`setText`/`setOptions` return whether the input changed. `measure` is cached;
`needsLayout` reports pending reflow. Position/color are draw arguments, so moving
or recoloring text does not invalidate layout. `line(i)` exposes byte offsets and
pixel width for a retained line (indices below min(lineCount, 32)).

Options also include `maxLines` (0 unlimited), `lineGap` (2 default), `wrap`
(true default), and `overflow` (`CLIP` default or `ELLIPSIS`). Explicit `maxHeight`
clips to the supplied viewport; `draw(..., scrollY)` accepts a caller-controlled
vertical offset. The caller clamps the offset to required height minus viewport
height and owns gestures/inertia. A line can be partially visible at the viewport
edge. Ellipsis uses up to three existing ASCII dots and is not used on confirmation
copy. It does not modify the retained source text.

Measurements report `lineCount`/`height` for retained source text and
`visibleLines`/`visibleHeight` for fully fitting cached lines, plus `overflow`,
source `truncated`, and `forcedBreak`. Capacity and max-line/height constraints
set overflow; a caller needing all content must handle it rather than treating
visible height as the entire paragraph. Font size never changes automatically.

Follow-up approved by the user: comma/full stop must align with the text bottom.
The 32px generator previously centered all glyph bounding boxes. It now places
only `，。、` at CJK ink row 30; all other glyphs, metrics, glyph count, pack size
and other font packs remain unchanged. The updated `zh32.smonfont` must be included
when installing resources. `test_text_block_fonts.py` checks bottom alignment and
the entire pack against the generator.

`CLIP` draws partial lines when the viewport is shorter than a glyph. `ELLIPSIS`
requires at least one fully fitting line to show its ending dots.

Native host coverage: `python3 tools/test_text_block.py`; scoped tutor replay:
`python3 tools/test_market_tutor.py --output <artifact-directory>`.

## Market adoption, second batch

Approved: Kecleon normal/box descriptions, ordinary purchase and box reward main
copy, Gengar learning/training/recall outcome main copy, both static greetings.
Reuse one primary cached TextBlock per stall; short supporting labels keep their
existing fixed font/positions. Preserve fonts/palette, greeting panel geometry,
greeting timers/tap guards, payment/save boundaries, box fades and motion timings.
Retain the authored newline separating training move identity from outcome.
Audit current text against fixed content rectangles (including the longest
species/move names and all item/reward variants); no ellipsis or font shrinking.
NPC typewriter conversations and Chansey are outside this batch.

## NPC typewriter composition

The next approved batch adapts existing MarketDialogue and MarketDialogueUi.
Extract a pure owning TextLayout (no Canvas/font-resource/renderer dependency);
TextBlock keeps its current API by inheriting that layout and adding drawing.
Add DrawRange {firstLine, lineCount, visibleCharacters}, relative to the selected
line window. A negative character count reveals all; zero reveals nothing. Count
complete UTF-8 codepoints in retained lines, including ordinary spaces but not
line separators. Alignment always uses complete line widths, never prefix width.
Consume the same reveal budget regardless of dirty clipping. Drawing must not
change text, cached breaks, selection or playback time.

MarketDialogue owns full-utterance layout and its independent playback clock.
Layout options use its configured width/font and existing glyph+8 line spacing.
Pages select two cached lines; only revealed count changes every 40ms. Keep the
existing first click completing the page and subsequent click advancing page,
utterance or ending conversation. Preserve pause/reentry/clock clamping and all
resource failure fallbacks. visibleText remains a normalized rendered-page
prefix for existing callers; it is no longer input to the renderer.

Keep bounded storage. If an unusually narrow/long utterance exceeds the 32-line
cache, continue from its last consumed source position when reaching the cache
end; never drop remaining content. Current authored dialogues fit one layout.
Whitespace omitted at soft-wrap boundaries is not a displayed character; glyph
and word content must not be skipped or repeated across pages.

No script resources, fonts, dialogue content, runtime dependency or save changes.

### Drawing a fixed page or partial reveal

```cpp
Ui::TextBlock::DrawRange window;
window.firstLine = dialogue.pageFirstLine();
window.lineCount = dialogue.pageLineCount();
window.visibleCharacters = int(dialogue.revealedCharacters());
Ui::TextBlock::drawLayout(canvas, dialogue.textLayout(), x, y, ink, window);
```

Existing `block.draw(canvas,x,y,ink,scrollY)` is unchanged. The new overload
`block.draw(canvas,x,y,ink,window,scrollY)` selects lines and budgets complete
codepoints relative to that window. Zero reveals nothing; a negative budget
reveals all. A selected line retains its complete measured width for alignment.
Reveal counts advance through clipped rows as well, so dirty painting cannot
accidentally disclose later text. Line indices are bounded to cached lines.

MarketDialogue keeps only its raw full utterance, one normalized layout and
playback counters. `visibleText` reconstructs a rendered prefix for compatibility;
it does not trigger layout or feed drawing. The original 256-byte full buffer
and validated catalog bounds remain; long narrow pages can continue in a second
layout chunk. Native checks exercise 192-byte catalog lines at64px widths,
40ms boundaries, UTF-8 prefix buffers, fallback loads, and paused clocks.

## Bag and ordinary-shop details

Approved next batch: only the item description in AMOLED bag (home/battle) and
ordinary-shop buy/sell details. Keep current 32px bitmap font, RGB(126,175,175)
description color, dark background, icons, labels, values and hit rectangles.
The two detail views are static; do not add entrance or text animation.

Use a shared bounded render cache keyed by the copied text and layout options;
it owns no item selection, Canvas pointer, shop state or transaction state.
Description region is x24/y248/320x100, allowing three32px lines with2px gaps,
ending at348 before buttons at354. Validate all current item descriptions fit
without ellipsis/shrinking. Bag toasts must remain above the multiline body.
Lists, other device families and gameplay rules stay outside this batch.

## Skill details and skill-management descriptions

Approved continuation: migrate the shared AMOLED `drawMoveDetailContent` body
used by team skill management and the learning replacement comparison page.
Keep the power/accuracy label, 32px font, RGB(178,194,190) ink, 40px line stride,
existing expansion/fade, scrolling and hit geometry. Retain x24, width320 and
body start at detailBase+64; five lines occupy192px inside the272px panel.
Use one bounded TextBlock render cache with8px line gaps, no shrink/ellipsis.
Audit every current move description against this region. Empty descriptions
must not repeat cached text. Fade/position changes must not change line breaks.
Description painting must intersect both the expanding panel and dirty rows;
header, subsequent rows and toast feedback retain their existing layering.
No gameplay, move data, font assets or other device-family changes.

## Growth messages

Approved continuation: AMOLED level-up dialogue, evolution in progress/complete/
cancelled text and hold/cancelling instruction, and new-move name prompt. Keep
existing copy, 32px font and colors, sprite/effect/clock behavior, counters,
header/button geometry, toast layer and input/save rules. Shared global toast
rendering and other device families are outside this batch.

Level-up: x32/width304/height64, centered, zero line gap; retain y346 for a single
line and y336/368 for the existing semantic two-line form (`name等级` then
`提升到level`). Use TextBlock measurement to select that authored break, and
let the component own wrapping/drawing. Evolution primary text: x8/y270/352x66,
centered/gap2; instruction x8/y388/352x52, centered/gap2. New-move name: existing
x88/y224, width256/height100, left/gap2, ending before the y348 continue button.
Reuse bounded caches per text role; no layout tied to elapsed time/fade/dirty
rows and no automatic font shrink. Audit actual species/levels/move names plus
fallbacks. Explicit32px remains stable even when ambient nativeText is false.
