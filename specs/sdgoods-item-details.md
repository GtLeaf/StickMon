# SDGOODS item detail text migration

Continue approved migration with home/battle bag and ordinary buy/sell details.
Use existing TextBlock with punctuation-aware wrapping, native20px labels and16px body; preserve
black/dark palette, item icons, rules, price/owned semantics and all earlier work.
Name rectangle144px wide with up to2lines; metadata below. Description at
68,200,224x86,16px font/4px line gap; all shipped descriptions fit without loss.
Buttons y292,h40,20px labels; bag x88/188,w84, shop same native safe bounds.
The shop retains its original 152px name width (8px wider than bag); this small
documentation/layout difference is deferred, with all names checked at144px.
Preserve header/tabs/coins and current usable/read-only/feeding/actions.

Approach: migrate only round presentation through one small ItemDetailView helper,
retain shared compact/stick legacy wrapping. A global text-helper replacement
would affect unrelated families; copying AMOLED368x448 geometry would clip on360.
No redesign of grids/settings, no new rules/schema/resources, no V1/OSTB builds,
no33ms scheduling, no commit/push/merge/flash. Existing feature branch retained.

Verify all item descriptions/names, home/battle/buy/sell native drawing, clipped
reconstruction/swapped bytes, readonly/feed/usable input, selection and scrolling
context, actual bag use and shop buy/sell. Capture before/after. Staged-bank
ASan/UBSan, shared text/save/architecture regressions, SDGOODS and V2 SDK builds;
one fresh read-only final review. Hardware appearance/feel remain untested.
