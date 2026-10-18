# Shared battle execution

The user authorized the next architecture batch and device flashing/log tests.

1. When either frontend starts a planned action, shared code shall perform
   status checks, charge transitions and damage calculation once, preserving
   the existing Stick rule and random-call order.
2. When an action reaches its impact point, shared code shall commit self-hit,
   damage, effects, contact/absorb abilities, move history and proficiency once.
   Repeated preparation/commit shall not repeat mutation or consume randomness.
3. Given identical actors, action context and random stream, immediate and
   delayed execution shall produce identical gameplay results and random calls.
   Each frontend shall use the shared execution path for both sides.
4. While showing actions, each frontend shall retain its animation timing and
   ordered recovery/block/charge/move/result/effect logs. Fainting and forced
   endings shall prevent further actions and duplicate rewards.
5. Existing save formats, gameplay tuning, battle-exit save boundaries and
   non-battle flows shall remain compatible. Relevant tests and all affected
   firmware/Web builds shall pass.
6. After validation, the connected device shall receive the matching firmware
   and resources, then exercise battle flows with diagnostic logs. Preserve the
   player's save during controlled scenarios and report actual device limits.
