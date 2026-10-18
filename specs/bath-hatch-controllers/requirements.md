# Bath and hatch page ownership

This implements the next authorized page-controller batch from the
2026-10-18 architecture plan.

1. While the AMOLED shower page is active, ShowerController shall own its
   gesture/session state, tool movement, foam/water/reaction animation and
   view model, preserving existing timing, scoring and partial rendering.
2. When a shower stage or completion is earned, the application shall apply
   shared BathService/ExperienceService rewards once, preserve progression
   navigation and save/retry boundaries, and return feedback to the controller.
3. While the hatch page is active, HatchController shall own countdown,
   interactions, shake feedback/detection, gesture handling, IMU scheduling
   decisions and its view model, preserving the existing egg-first flow.
4. When hatch progress or completion must be saved, the application shall
   retain existing persistence intervals, failure retries, the same rolled
   starter across retries and commit-before-clear ordering. Leaving hatch
   shall disable its IMU request as before.
5. Controllers shall not access AmoledApp, platform services, storage or
   render caches. V1, V2, native and Web shall compile the same controllers.
6. Existing page behavior and save compatibility shall pass regression checks;
   affected firmware and Web targets shall build. No GameState/schema or
   gameplay balance changes are part of this batch.
