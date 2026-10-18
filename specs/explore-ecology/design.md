# Design

`ExploreEcology::Simulation` owns a fixed actor array and a surface mask,
derived once when a map is generated. Pool membership, selected route and map
seed determine the species and habitat positions. Actor-local random streams
keep animation decisions independent of gameplay randomness and other actors.

`AmoledApp` advances simulation only for an active, unlocked route, publishes
poses through `ExploreRouteViewModel`, and invalidates old/new actor bounds.
Inactive updates rebase time so menus and battles do not cause a catch-up jump.
The existing route actor depth ordering gains wildlife entries; cached walking
frames, shadows and vegetation restoration are reused. Grass and forest maps
use vegetation habitats, the frost cave uses clear floor/ice, and the waterfall
valley uses clear rock floor. Only open horizontal water tiles permit swimmers;
shore edges, waterfall strips, bridges, rocks and holes are excluded. Water-only
fish require water. Amphibious species prefer water but can inhabit land. Short
walks stay on the actor's original surface. Failed species placement considers
remaining pool members so unavailable habitat does not waste the population cap.

Unknown rare species use grass rustles, water ripples, ice dust or rock dust.
Visible swimmers use a ripple in place of a land shadow. All effects fit the
actor's dirty bounds. Missing sprite resources skip the actor.

Ecology is transient map state. Camera movement and battles preserve it; a new
map creates a new population. No persistent fields or new asset packs are added.
StickS3 can later consume the same simulation; all six areas use the shared
AMOLED renderer. The road actor ground alignment remains independent of wild
actors' habitat coordinates.

First behavior batch (user-approved 2026-10-17): each actor owns a transient
activity/elapsed-time sequence with separate attention and episode cooldowns.
A map-wide owner serializes conspicuous episodes; hidden-in-cover waiting
releases the owner, then waits its turn before peeking. Offscreen actors finish
existing sequences at reduced cadence but start no new conspicuous episodes.
Known ground actors use idle/walking/sleeping frames from `PetAnimationProfile`;
unknown rare actors remain habitat hints through every transition.

Approach/retreat/flight take a validated cardinal leg and retrace it back to
the episode origin. Peeking uses a smaller safe outward-and-back leg. Movement samples the leg, checks compatible surface/home
radius and reserves the origin against ordinary wanderers. Grass cover is
derived once from dense/tall grass and supported decorative objects. Hiding
uses crouching frames in grass and actual occluder contours behind trees/rocks;
peeking moves to a safe edge and back. Bodies never sink into the floor.
Body lift/sway are separate from ground depth. Shadows stay grounded while
flight/jump bodies lift. Breathing, a small pixel Z, ripples and splash droplets
use procedural drawing, so no asset pack or save schema changes are needed.

Cover and locomotion refinement (approved 2026-10-17): classify decorative
occluders across map layers using their complete object's shared ground row.
Restore their existing opaque pixels per actor in all areas. Dense grass uses
its own leaf pixels, excluding sparse-ground colors from the foreground mask,
so restoring cover cannot paint an opaque rectangular ground tile over a body.
The grass-feedback refinement below deforms map art and this mask together.
Safe habitat distinguishes cover from walls/holes and reserves ground behind
decorations without opening any player routes. Hiding uses crouching artwork
in grass and the actual contour behind solid objects, with a safe lateral peek.

Locomotion is independent of temperament. Except for the wide-ranging gulls
described below, known flapping species continuously circle locally between
larger validated flights, with a separate wing clock;
returns stay airborne. Their existing frames do not provide grounded landing
poses, so the former generic takeoff/landing pause is removed for them. Pidgey
family remains ground-only, and the Lati pair retains its separate floating
movement. Unknown species remain environmental hints. No assets/save changes.

Observation pacing refinement (2026-10-17): spawn two actors normally, with
an occasional third only when the combined home positions span a separated
route region. Prefer candidates with validated walking exits before habitat
flavor scores. Initial quiet waits last 0.6-2.0 seconds; later rests last
1.4-3.0 seconds. Episode cooldowns and the single conspicuous owner remain.
Ordinary walking tries all four directions and shorter legs; blocked actors
turn visibly using existing directional idle art. Failed player reactions
also turn rather than silently resetting the rest timer. Native observation
tests compare actual output pixels, so idle frame counters cannot masquerade
as visible activity when a species has only one idle frame.

Wide flight (approved 2026-10-17): Wingull/Pelipper use a separate cached flight
surface/region mask. A bounded breadth-first search chooses reachable goals
roughly 3-6 tiles away, penalizes recently visited goals, and plans only when
the previous route ends. A small fixed-step steering follower rounds waypoint
corners at constant forward speed. Flight continues offscreen and does not
claim the conspicuous-episode owner. Isolated habitat retains a local loop.

After 25-45 active seconds, a gull can route to a reachable open border and
continue beyond the sprite envelope. An explicit absent pose freezes drawing
while a 12-25 second timer runs. Reentry starts wholly outside another open
edge in the same connected region and retains the same population slot.
Regions without a reachable open border keep roaming. Ground and flight
passability remain separate; no map generator, encounter, save or asset changes.
Airborne sprites use small ground shadows, no swimming ripples or grass crop;
tall tree contours still occlude them by depth. Old/new dirty bounds include
departures, absence and reentry.

Adult/young pairs (approved 2026-10-17): an explicit first-batch table supports
Breloom/Shroomish and Marill/Azurill only. Selection precedes independent spawn,
requires either active entry to be visible, uses a 50% eligibility roll, and
consumes exactly two slots. An explicitly hidden pool member blocks that
family, even if its partner is visible. A missing member is supplied only by
the explicit family table, never added to the encounter pool or history.
One transient group controller replaces independent episodes for these actors.
No evolution guesses, extra assets, encounter changes or persistent data.
Route entry preloads and pins partners of active pool species alongside the
area encounter table and boss, so an out-of-pool companion needs no sprite
decode during walking. The shared pair table also supplies this preload list.

Cache shared cardinal walking edges validated for both members against surface
and prop footprints. Remove dead-end branches, then reject connected pockets
smaller than eight cells. The initial adult anchor is near the selected route.
At each destination, run a bounded BFS preferring targets 4-7 steps away;
penalize the eight recent stopping places and their neighbours. Exclude the
young's current cell from the new path to avoid reversing into it. Plan only
at stops, not on each frame. Every new trail starts with the exact adjacent
young/adult nodes; the trail is open, never wrapped back to its start.

The adult advances on the trail; the young follows the same real traversed
positions after a short delay. Brief glances open the gap; the adult waits and
faces the young before continuing. At arrival or after 15-25 active seconds
they stop at adjacent trail nodes and face each other, then continue toward
another destination. If there is insufficient shared walking room, use ordinary
independent spawning. A narrow connected habitat can constrain the route, but
open terrain allows the pair to move between different observation places.
Player reactions close the gap along the safe trail and time out with a cooldown.
Group updates use one shared cadence/clock, freezing through pause. Existing
poses, cached natural-size sprites and depth/dirty bounds handle presentation.


## Ordinary and high-grass feedback

`ExploreGrass::Simulation` in shared game code tracks five grounded contacts
(two party members and three ecology slots) and one bounded impulse per map
cell. Six world pixels of accumulated movement start a 720 ms, eight-phase
part/rebound sequence, with a 270 ms retrigger cooldown. Segment sampling avoids
missing crossed cells; relocations over two tiles reset contact history. The
high top (391) shares its clock with the base (399) immediately below. Map seed
or area changes reset the transient simulation; none of this enters saves or
uses gameplay RNG. Inactive updates rebase time and preserve animation age.

AMOLED V1/V2 reuse the existing 390/399/391 art, decoded once. Horizontal pixel
sampling parts the blades around the contact and rebounds before settling;
high top/base displacement joins at their shared edge. Floor colors are read
from the existing sparse grass tiles. Foreground rendering skips floor pixels
and uses the same displaced artwork as the map pass, retaining leaf gaps.
Ordinary cover starts ten world pixels below its tile top; high grass uses its
natural two-tile height. Cover stops at the grass ground-depth boundary.

The static world cache stays unchanged. Active grass cells are recomposed in
layer order before actors, clearing old transparent leaf positions. The actor
pass restores moving leaves by ground depth. Each changed cell damages only
its screen rectangle, plus the high top above it; camera transforms and clips
are shared with the map. Idle effects request no redraw. Grounded visible
wildlife and party members supply actual shadow-center anchors; airborne,
water-only, hidden and absent actors do not trample grass.
