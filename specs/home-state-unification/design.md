# Design

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
