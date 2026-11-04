# AMOLED 房间权威状态

更新：2026-10-26。并入 AMOLED 重构主题；状态统一已实施，后续房间动作／互动／访客拆分见同目录。

## 行为要求

1. While the AMOLED room is active, each actor shall have one authoritative
   position, target, route, gameplay task, mind and decision clock in
   `Home::Actor`; synchronization shall not copy a second leader pose into it.
2. When an actor turns or finishes walking, animation shall pause/resume the
   existing task without duplicating or prematurely advancing gameplay.
3. When food, sleep, faint recovery, pair activity or a visitor preempts an
   actor, existing shared policy/resource ownership shall determine its task.
4. When loading a save or returning from exploration, the room shall retain
   its existing position conversion, healthy-only doorway behavior and save
   boundaries. GameState and the save schema shall remain compatible.
5. After the migration, room care, visitor, exploration, lock/wake and page
   regression checks and all affected firmware/Web builds shall pass.

Animation frames, facing, camera, hearts, stopping playback and scripted
activity phases remain presentation state. This batch does not merge battle,
growth or save scheduling flows or change hardware brightness.

## 设计与所有权

Use the existing `Home::Simulation` and its actors as the runtime authority.
Remove leader pose/mind/decision duplicates from AmoledApp. Initialize the
actor before loading its runtime pose, and keep roster synchronization limited
to identity, geometry and runtime attachment.

Keep PetMotion as a read-only presentation view computed from actor task and
motion. Use Simulation beginTurn/finishTurn and actor resumeTask for turning;
retain only a local stopping-animation flag. Sleep is Task::SLEEPING, faint
rest is Actor::faintRestActive. Clearing faint rest must also stop the FAINTED
task before normal decisions resume.

Turn completion resumes the actor's own saved intent, including IDLE, WANDER
and YIELDING whose priorities are below TURNING. Only finishTurn bypasses that
priority comparison after checking the current task; ordinary transitions and
interrupted turns keep their existing guards. A yield requested during turning
changes resumeTask without skipping the turn pause.

Both ordinary routes use Runtime::advanceRoute and a shared ActorController
speed calculation. Scripted room/pair/door activities retain their current
phase and timing; doorway crossing may intentionally go outside walk bounds.
No global per-frame path planning or new dynamic allocation is introduced.
Leader yield movement keeps the existing wandering speed; companion yield
keeps its existing default route speed. Stationary room-action phases explicitly
set STILL after transitioning, since ROOM_ACTION can also own walking routes.

Save view-state remains a serialization adapter: actor coordinates are foot
coordinates, persisted positions subtract sprite ground offset. Rendering
reads the existing simulation snapshot plus presentation state.

Validate with real App event chains and partial/full rendering comparisons,
shared task/resource tests, existing visitor/exploration/lock tests, save
compatibility tests and builds for all affected targets. Compilation does not
stand in for device validation.

## 实施与验收记录

以下记录对应原实施批次，本次文档整理未重新执行功能测试或实机验收。

- [x] Audit duplicate state and capture the current working implementation.
- [x] Centralize ordinary route speed without changing values. (R1, R3)
- [x] Move leader pose, mind, geometry and decision clocks into Actor. (R1)
- [x] Derive presentation from task; unify turn and rest transitions. (R2, R3)
- [x] Preserve load, save, visitor and exploration lifecycle behavior. (R4)
- [x] Extend real App regressions and migrate old field assertions. (R5)
- [x] Run affected checks/builds and document device-validation limits. (R5)

原批次的真实 App 流程、构建及未刷机边界见
[房间状态统一执行记录](../../doc/代码结构评估与优化方案-2026-10-18.md#房间状态统一执行记录)。
