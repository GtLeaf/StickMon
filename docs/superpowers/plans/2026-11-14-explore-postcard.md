# Explore Postcard Trial

User authorized implementing the first approved preview with rollback guaranteed.

Baseline: 886c4c4, preserved as backup/explore-selector-before-postcard-20261114. Trial branch: feat/explore-postcard. No flash, push, merge or commit requested.

- [x] Add a compile-time switch (STICKMON_EXPLORE_POSTCARD, default 1); keep original renderer and original input geometry under 0. No save/rules changes.
- [x] Use seven compact, reproducible indexed scene thumbnails, native fonts, existing real preview pool and hidden-species rules. Scene card, static visible species, header Back and full-width Depart; preserve overflow paging and loading.
- [x] Align hits, swipe direction and dirty rows with the new layout. Verify native App interactions, clipping, partial redraw and both switch settings. Export real screenshots.
- [x] Review and document one-command branch rollback; leave uncommitted for user review.
