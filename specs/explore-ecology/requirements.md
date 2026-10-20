# Exploration Ecology

The user approved the grass-path prototype, then requested its extension to
all exploration maps. Continue the same behavior and rendering across all six
areas on AMOLED V1/V2.

1. When entering any exploration map, usually spawn two ambient actors from the
   expedition's existing active pool near the selected route, with the explicit
   adult/young companion exception in requirement 15. Use safe grass,
   cave floor, ice or open water appropriate to the species. Limited habitat
   or a smaller pool may produce fewer actors. A third actor requires a
   separated part of the route; never spawn more than three. Water-only fish
   shall never spawn on land; actors shall not walk across incompatible surfaces, walls,
   bridges, waterfalls, holes or map boundaries.
2. While exploring, actors shall rest, turn, take short walks and briefly
   face a passing player. Their map positions shall survive camera movement.
3. When a rare species is unknown, show grass movement, ripples or ground dust
   appropriate to its habitat without revealing its identity. Ecology shall
   not mark species encountered or initiate battles.
4. While the route is paused, in battle, behind a menu, locked or complete,
   ecology shall freeze and resume without advancing through the pause.
5. Rendering shall honor ground depth, vegetation and partial redraws, use
   cached sprites, and update offscreen actors at a reduced cadence.
6. Ecology shall use its own deterministic randomness, leave map generation,
   encounter rolls, rewards and save schemas unchanged, and share logic in
   `src/game`. AMOLED V1/V2 share this first presentation integration.
7. The first behavior batch shall use existing sprite packs for sleep/wake,
   curious approach/observe/return, shy retreat/hide/peek/emerge, grass burrowing,
   winged short flights and aquatic jumps/splashes. Species select appropriate
   temperaments; movement remains within the same safe habitat and home radius.
8. Episodes shall include quiet pauses, stagger their starts and allow at most
   one conspicuous sequence at a time. Each actor waits 15-30 seconds after an
   episode before another. Hidden rare species only show habitat hints.
9. Every phase shall freeze during pause/battle/menu states and resume in place.
   Jumps and landings shall finish before a new player reaction. Rendering shall
   clear old/new body, shadow, cover and effect bounds during all transitions.
10. All six areas shall restore real grass, shrub, tree and rock contours by
    ground depth. Open ground and the front of an object shall leave actors
    visible. Hiding shall require actual nearby cover; solid cover shall never
    sink/crop an actor into the floor. Peeking shall move toward a safe edge.
11. Locomotion shall follow the existing sprite artwork. Pidgey, Pidgeotto and
    Pidgeot stay grounded. Zubat/Golbat/Crobat, Butterfree and Wingull/Pelipper
    continuously flap and move while airborne, including turns and returns.
    Wing timing shall survive activity changes and freeze during pause. Actors
    with no long safe leg shall circle within their safe cell instead of hovering.
12. When the player stops to observe, visible ground actors shall begin a
    perceptible turn/step within about two seconds. Quiet looks and short walks
    shall run independently of another actor's conspicuous episode and its
    15-30 second cooldown. Failed walks shall try safe alternatives or turn in
    place, instead of restarting an unchanged long idle. In open/isolated test
    habitats, visible actors shall not keep identical rendered pixels for more
    than five seconds. Spawn scoring shall prefer habitat with room to move.
13. Known Wingull/Pelipper shall continuously roam their connected flyable
    map region, choosing distant, varied destinations without returning to a
    fixed home. Flying may cross roads, horizontal water and low decorations;
    cliffs, cave walls and trunks remain blocked. Turns retain forward motion
    and wing timing. Other species retain their existing movement profiles.
14. After a period of roaming, a gull may leave through a reachable open map
    edge. It shall disappear only after its entire sprite clears the map, wait
    12-25 active seconds, then fly in from an open edge in the same region.
    Away actors retain their population slot. Camera position shall not stop
    roaming or change map coordinates. All new phases freeze during pause;
    unknown species remain habitat hints until revealed.

15. When any member is visible in the active preview pool, the explicit
    adult/young families may spawn with one overall 50% eligibility roll.
    Families are Breloom/Shroomish, Azumarill/Marill/Azurill, Furret/Sentret,
    Mightyena/Poochyena, Quagsire/Wooper, Camerupt/Numel, Pidgeotto/Pidgey,
    Gardevoir/Ralts and Pikachu/Pichu. Multiple eligible families shall be
    sampled uniformly without replacement until safe placement is found.
    The explicit family table may supply missing members even when they are
    absent from the pool. When selected, the Marill family chooses one member
    with 30% probability, two with 45%, or all three with 25%. Each single
    has 10% probability and each pair 15%. A solo uses independent behavior;
    pairs/triples follow in evolution order. At most one selected family uses
    its exact chosen slot count, with no additional independent actors. A family
    with an explicitly hidden pool member is ineligible; selection shall never
    reveal that member. The active encounter pool and history remain unchanged.
16. Families shall share a trail validated for every member, with delayed
    followers, occasional looks, adult waiting and brief reunions at destinations
    or after 15-25 active seconds. A nearby player makes the adult face them and the young
    close the gap. Routes may vary at reunions without teleporting or crossing
    obstacles; unsuitable habitat falls back to ordinary independent actors.
    When all three appear, Azumarill leads Marill and Azurill in that order.
    Each follower follows the preceding member, waits for the younger one, and retains a safe gap
    through turns, gathering and player reactions. Replanning preserves all
    three anchors. Any visible member requests the foreground update cadence.
    Composition is fixed at map entry. If the chosen group lacks a safe trail,
    try other compositions of that size, then fewer members, down to one safe
    actor. Hidden-member exclusion applies before composition selection.
    Existing sprite size, ground anchors, cover, pause and redraw rules apply.
17. Groups shall walk toward varied destinations, preferring targets 4-7 safe
    steps away and avoiding recent stopping places. They shall continue from
    their current anchors without wrapping a short fixed loop or reversing
    into the follower. Tiny pockets and dead-end-only habitats shall use
    independent actors. In connected test habitats a pair shall visit at least
    20 cells and span five tiles or more over 120 active seconds.

18. Moving grounded party and known wildlife actors shall part ordinary dense
    grass (390) and high grass (399/391), followed by a short rebound at the
    touched map cell. Effects shall finish after departure and stop retriggering
    while standing. Sparse ground grass shall remain unchanged. Flying,
    floating, jumping and hidden actors shall not create a trampling trail.
19. High-grass body cover shall use the same moving leaf contour as the map
    animation, remain anchored to world tiles and retain static cover after
    settling. Ordinary grass shall cover only its low leaf region. Adult/young
    contacts shall trigger independently. Pause shall freeze effects; local
    dirty redraws shall match full frames and cease once grass settles.

Acceptance: deterministic simulation and habitat tests, native full/partial
render comparisons, pause and encounter-state checks, and affected firmware
builds. Hardware FPS is recorded as unverified until tested on a device.
