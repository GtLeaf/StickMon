# Cry ADPCM compression (2026-11-15)

User approved executing the 16 kHz IMA ADPCM option after comparing DEFLATE-9 (237 B saved) and ADPCM (~164 KiB saved). Keep all 64 species, duration, gain and existing interaction triggers. Re-encode the existing normalized U8 signal from the original WAV pipeline with the shared 1024-byte IMA blocks (2041 samples/block); do not lower sample rate or trim cries.

SMCR v2 retains the 32-byte header. sampleCount and payloadRawBytes are the exact decoded U8 sample count. payloadCompressedBytes is the IMA payload length, payloadCrc32 checks the encoded payload, flags=4 means IMA ADPCM mono, reserved=0. Block padding must never play. SMCR v1 raw-DEFLATE/PCM-U8 remains readable. Validate version, flags, species, rate, lengths, CRC and every block header before submitting PCM to the cry channel. Reuse the existing audio decoder rather than adding a second codec implementation.

Alternatives: DEFLATE-9 has negligible savings; reducing sample rate changes bandwidth. ADPCM was chosen to preserve 16 kHz with a moderate lossy size reduction. Listening on hardware remains necessary; do not claim hardware sound acceptance from host checks.

## Implementation plan

Execute inline using writing-plans, executing-plans and TDD. The existing cloud checkout is isolated; preserve user changes, do not create another worktree, push, publish or flash. Commit only when explicitly requested by the user.

- [x] Add runtime tests: v2 known PCM vector, multi-block tail length, legacy v1 PCM, CRC failure, invalid headers/indices, truncated/oversized payload, mute/replay/stop and all shipped cries. Observe v2 playback failure first.
- [x] Extract existing IMA decoder to a shared header; add v2 loading with strict validation and CRC, preserving v1 loading. Run new runtime and existing music tests.
- [x] Add generator regression for new encoding, regenerate only cries/manifest, verify all 64 files and savings. Document the v2 format.
- [x] Build V2 Lite Debug and SDGOODS Debug only; package new artifacts, inspect resource free space and run affected audio/resource/architecture checks.
- [x] Fresh independent code review, fix material findings and report validation and limits.

Review focus: bad block index/reserved byte, final-block padding, CRC over the whole encoded payload, oversized count allocation, retained legacy behavior and unchanged music decoding.

## Validation

- 64 cry packs: 690195 -> 522240 bytes (167955 B saved, 24.3%). Source durations, rate, species and normalization unchanged; independent review verified regenerated packs against the old decoded PCM.
- V2 SPIFFS offline `SPIFFS_info` accounting: free 43172 -> 212597 bytes (~207.6 KiB). Partition layout unchanged.
- V2 Lite Debug application: 2376144 bytes; SDGOODS Debug also built and its development package passed official inspection. V1 was not built.
- Targeted regression: 44 tests, 42 passed and 2 optional OSTB checks skipped. Cry runtime tests also pass ASan/UBSan. Independent code review reported no material findings.
- Evidence: `.stick-local/artifacts/cry-adpcm-20261115/`; development firmware bundles: `.stick-local/firmware/cry-adpcm-debug-20261115/`.
- SMCR v2 needs the new player: update application and resources together. The new player can read v1 packs; old firmware cannot read v2 packs. No flashing or hardware listening/latency acceptance was performed. No remote push was performed.
