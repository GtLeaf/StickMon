# Tasks

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
Logs: tools/out/bath-hatch-controllers-20261018/.
