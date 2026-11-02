#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes the E0 test loop: two bars of a synthesized drum loop at 120 BPM (4.0 s), 44.1 kHz 16-bit stereo WAV.
Kick on every beat, snare on 2 and 4, eighth hats panned a little, a bass note per bar. Deterministic (fixed seed),
no third-party modules.   make_testloop.py <out.wav>"""
import math
import os
import random
import struct
import sys

FS, BPM, BARS = 44100, 120, 2
beat = int(FS * 60 / BPM)
n = beat * 4 * BARS
rnd = random.Random(7)
noise = [rnd.uniform(-1, 1) for _ in range(n)]
out = bytearray()
for i in range(n):
    b, tb = divmod(i, beat)
    t = tb / FS
    kick = 0.9 * math.sin(2 * math.pi * (45 * t + 60 * (1 - math.exp(-t * 30)) / 30)) * math.exp(-t * 8)
    snare = 0.0
    if b % 4 in (1, 3):
        snare = 0.45 * noise[i] * math.exp(-t * 20) + 0.25 * math.sin(2 * math.pi * 190 * t) * math.exp(-t * 25)
    te = (i % (beat // 2)) / FS
    hat = 0.16 * noise[(i * 7) % n] * math.exp(-te * 90)
    pan = 0.3 if (i // (beat // 2)) % 2 else -0.3
    bar_t = (i % (beat * 4)) / FS
    bass = 0.25 * math.sin(2 * math.pi * (55 if i < beat * 4 else 49) * bar_t) * math.exp(-bar_t * 1.2)
    mono = kick + snare + bass
    l = 0.7 * (mono + hat * (1 - pan))
    r = 0.7 * (mono + hat * (1 + pan))
    out += struct.pack('<hh', max(-32768, min(32767, int(l * 32767))), max(-32768, min(32767, int(r * 32767))))
path = sys.argv[1]
os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
with open(path, 'wb') as f:
    f.write(b'RIFF' + struct.pack('<I', 36 + len(out)) + b'WAVE')
    f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 2, FS, FS * 4, 4, 16))
    f.write(b'data' + struct.pack('<I', len(out)) + out)
print(f'{path}: {n} frames, {n / FS:.2f} s at {BPM} BPM')
