# Design

Add ShowerController and HatchController beside existing page controllers.
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
