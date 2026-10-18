# Exploration daylight

The user approved implementation of the proposed exploration day/night colors,
with longer days and shorter nights. Reuse current assets and game time.

1. While exploring outdoors, the world shall show dawn 04:00-06:00, daylight
   06:00-18:00, dusk 18:00-20:00 and night 20:00-04:00. Thus the lit day,
   including its transitions, lasts sixteen hours and full night eight hours.
2. During dawn/dusk, colors shall change continuously with game minutes,
   including across midnight, without changing the game clock or save schema.
3. Maps, party, wildlife, grass, shadows and pickups shall share world lighting;
   names, buttons, dialogs, toasts and text shall retain their normal colors.
4. Frost Crystal Cave shall retain its authored lighting at every hour. Indoor
   screens and battles remain outside this exploration-only first batch.
5. AMOLED V1/V2 and StickS3 shall use shared time/color rules. Paused AMOLED
   exploration shall retain its last lighting until resumed. Stable colors
   shall not schedule repeated map redraws; changed colors repaint only the map.
6. Partial frames, camera changes and grass/wildlife motion shall match full
   rendering without accumulated tint, stale pixels or modified cached art.
