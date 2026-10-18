# Room state ownership

This is the next authorized architecture batch from the 2026-10-18 plan.

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
