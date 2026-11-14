#!/usr/bin/env python3
"""Render Emerald's original Verdanturf MIDI with the existing Essentials bank.

Requires mido, fluidsynth and ffmpeg. Never regenerates the other audio packs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

import generate_audio_assets as audio

SOURCE = audio.ROOT / audio.REPOSITORY_MUSIC_SOURCES['bgm_market']
SAMPLE_RATE = 16000


def prepare_midi(source, destination):
    import mido
    midi = mido.MidiFile(source)
    seconds = 0.0
    tempo = 500000
    markers = {}
    for msg in mido.merge_tracks(midi.tracks):
        seconds += mido.tick2second(msg.time, midi.ticks_per_beat, tempo)
        if msg.type == 'set_tempo':
            tempo = msg.tempo
        if msg.type == 'marker':
            markers[msg.text] = round(seconds * SAMPLE_RATE)
    if not (0 <= markers.get('[', -1) < markers.get(']', -1)):
        raise ValueError('The original MIDI must contain its intro and loop markers')
    for track in midi.tracks:
        for msg in track:
            # GBA uses all ten parts melodically, including MIDI channel ten.
            if not msg.is_meta and msg.channel == 9:
                msg.channel = 10
            if msg.type == 'program_change':
                # These slots select PSG square voices in Emerald's bank.
                msg.program = {4: 80, 5: 81}.get(msg.program, msg.program)
    midi.save(destination)
    return markers['['], markers[']']


def generate(soundfont, output=None):
    soundfont = Path(soundfont)
    if not soundfont.is_file():
        raise ValueError('Provide the original Essentials soundfont.sf2')
    provenance = json.loads(SOURCE.with_suffix('.json').read_text())
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != provenance['sha256']:
        raise ValueError('Verdanturf MIDI differs from the reviewed source')
    output = Path(output) if output else audio.AUDIO_OUT / 'bgm_market.smonaudio'
    with tempfile.TemporaryDirectory(prefix='market-audio-') as temporary:
        temporary = Path(temporary)
        midi = temporary / 'verdanturf-gm.mid'
        loops = prepare_midi(SOURCE, midi)
        rendered = temporary / 'verdanturf.wav'
        audio.render_source(midi, rendered, SAMPLE_RATE, soundfont)
        samples, rate = audio.read_pcm16(rendered)
        samples, start = audio.prepare_music(samples, rate, loops)
        samples = audio.normalize(samples, audio.MUSIC_TARGET_DBFS, soft_limit=True)
        packed = audio.make_audio_pack(samples, rate, True, start)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(packed)
    print(f'bgm_market: {len(samples)/rate:.3f}s, loop={start/rate:.3f}s, {len(packed)} bytes -> {output}')
    return packed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--soundfont', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    generate(args.soundfont, args.output)
