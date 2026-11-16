# SDGOODS market entry and return-before-lock

User approved fourth batch: market scene card and lock/return parity. Preserve
uncommitted first3batches, current whole-market Debug gate and Chansey Debug gate.
No V1/OSTB target builds/hosts, device/serial/flash, commit/push or schema/rules.

Only the market destination changes: native360 cream-apricot page, centered
232x176 card at64,98 using the same user-chosen actual-map screenshot160x122
as AMOLED. Scene name at80,114, closed countdown beneath it; no open-status,
flavor captions or creature preview on market card. Shared source art bytes
move into assets, compatibility include retains micro pixels. No new image
or source map edits. Preserve native public navigation bounds/header alignment;
keep scene arrows/indicator/depart hit geometry, warm colors for market only.
Closed card still shows art and disables departure; open button says出发.
Frame carries open/countdown; real clock/day changes invalidate a parked card.
Render preserves clipping/byte order; no preview cache allocated for market.

Physical SDGOODS short lock queues a request rather than immediately darkening.
Power dispatcher asks engine/scene to prepare: close active stall and reuse
normal market fade/door return, wait for home/arrival/fade completion, save,
then suspend display/audio/CPU. Nonmarket lock behavior otherwise unchanged.
Departure/return already underway completes safely before locking; departure
may briefly enter market then immediately return. Another short press cancels
a pending request; unlock remains immediate and queues wake/invalidation.
Saved purchases/quotas survive; no replay, callbacks or held touches leak.
Save failure keeps screen awake and retries no faster than1s. Shutdown keeps
existing save-before-power-off behavior. Host evidence is not device acceptance.

Verify open/closed native screenshots, clock refresh/depart disable, touch
bounds and other-scene unchanged pixels; shared art default output unchanged.
Verify real power dispatcher+engine walk/fade return from each paid service,
physical/API request, busy departure/return, retry/cancel, wake and held touch;
both Debug/release gates, all3prior journeys, native ASan/UBSan clip/swap,
platform power tests, save/architecture, SDGOODS+V2 builds and fresh review.

Automatic idle remains the existing dim-only behavior; this batch does not add
a new automatic screen-lock policy. Physical/API lock requests are covered.
