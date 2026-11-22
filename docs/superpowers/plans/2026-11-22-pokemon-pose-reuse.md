# Pokemon pose reuse

Approved scope: automatically reuse exact, translated and horizontally mirrored
artwork for the 85 source-timed species. Preserve logical SpriteKind identifiers,
full virtual canvas, ground and shadow metadata, scaling behavior and original
timelines. Existing legacy/battle/icon records keep their pixels and geometry.

## Resource contract

Version 5 retains the 28-byte v3 header and appends two little-endian uint16
values: high RLE word count and pose count. The logical frame count remains in
the original header. The payload contains logical steps, poses, uint16 RLE
words, then uint16 palette words. CRC and raw-DEFLATE semantics remain intact.

- Step, 16 bytes, `<HHBBBBhhBBBB>`: kind, pose index, virtual width/height,
  ground padding, flags (bit zero is pose mirror), image x/y within the virtual
  canvas, shadow x/y/radii.
- Pose, 16 bytes, `<BBBBIII>`: image width/height, format, palette size,
  RLE offset/length, palette offset.
- Old v2/v3/v4 readers remain available. Unsupported flags, missing poses,
  out-of-canvas images, invalid palette/RLE references, CRC/size failures and
  truncated headers must fail without publishing a partial cache entry.

## Runtime contract

SpriteFrame keeps its virtual geometry. Trailing image dimensions, image
offsets and an image mirror flag point directly into compact pose pixels.
Legacy frames use zero image dimensions and retain the existing drawing path.
Shared drawing and visible-bounds helpers resolve transforms. There is no
per-frame heap allocation or reconstruction of a full bitmap.

## Steps and acceptance

1. Snapshot current packs and add failing generator/decoder regressions.
2. Implement v5 writer, Python decoder, runtime loader and shared drawing.
3. Compare all pilot frames (381, 94, 362) with old resources, including outer
   flips, fractional scales, nearest/legacy sampling, tint/silhouette and clip.
4. Regenerate all 85 eligible species and verify every logical frame reconstructs
   its original pixels, canvas, ground and shadow. Measure actual byte savings.
5. Run native V2/SDGOODS journeys, malformed-resource checks and authorized SDK
   builds. Obtain an independent review and fix material findings.

All evidence belongs under `.stick-local/artifacts/pose-reuse-20261122/`.
V1 and OSTB firmware builds and hardware flashing are outside this work.
