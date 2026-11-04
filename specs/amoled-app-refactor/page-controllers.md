# AMOLED 洗澡与孵化控制器

更新：2026-10-26。并入 AMOLED 重构主题；实现已完成，原批次实机边界保留。

## 行为要求

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

## 设计与所有权

ShowerController and HatchController are registered beside the other page controllers.
Each owns page state and provides bounded update/input/model operations.
Effects describe redraw regions, requested application actions, shower stages
and completion scores. App consumes these effects and performs hardware,
persistence, experience/progression and navigation work using existing services.

Hatch IMU scheduling and shake detection belong to the page controller;
App performs acceleration reads and enabled-state changes and reports results.
The pending completed GameState remains in App so a failed commit retries the
same rolled starter before progress is cleared. Shower save scheduling and
reward application remain in App. Cosmetic feedback returns to the controller.

Keep existing thin App entry points where they coordinate these effects, but
remove the duplicate page fields and simulation/gesture bodies. Rendering uses
controller-generated models; dirty-region requests are still executed by App.
Use fixed-capacity existing foam/water arrays and no new per-frame allocation.

Register new production files only through app/sources.txt. Validate with real
App touch/update/render chains and full/partial pixel comparisons, shared bath
and hatch services, save/architecture checks, V1/V2 builds and both Web targets.
Device validation remains separate from native checks and compilation.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Capture the current working tree and define controller boundaries. (R5)
- [x] Extract shower state, gestures, simulation and model; retain application
  reward, progression and save coordination. (R1, R2)
- [x] Extract hatch state, gestures, timing, shake/IMU decisions and model;
  retain application commit and lifecycle coordination. (R3, R4)
- [x] Register both controllers in the shared application manifest. (R5)
- [x] Migrate and extend behavior regressions without weakening assertions. (R6)
- [x] Run relevant checks/builds and record remaining device validation. (R6)

Validation: final native Debug 56/56; compatibility 144; page/source 35;
services/Web 36; shared/save/architecture/scheduling 22. Groups overlap.
AMOLED V1/V2 lite and lite debug, plus both Web targets built successfully.
No device was flashed. Hardware checks remain: shower dragging and progression
return, egg click/shake feedback and IMU disable after hatch completion.
Logs: .stick-local/artifacts/bath-hatch-controllers-20261018/.
