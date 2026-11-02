#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes the E1 demo's RemoteFM1 folder: <root>/audio/ with four synthesized loops at 120 BPM (44.1 kHz 16-bit
stereo WAV, deterministic, no third-party modules):
  a_drums_bpm120.wav   two bars of kick / snare / hats
  b_bass_bpm120.wav    two bars of a saw bass line
  c_chords_bpm120.wav  two bars of a four-note pad, a chord per half bar
  d_perc16_bpm120.wav  one bar of sixteen different percussion hits (one per 16th: slice material)
   make_demo_folder.py <root>"""
import math
import os
import random
import struct
import sys

FS, BPM = 44100, 120
BEAT = FS * 60 // BPM
rnd = random.Random(11)


def write(path, frames):
    out = bytearray()
    for l, r in frames:
        out += struct.pack('<hh', max(-32768, min(32767, int(l * 32767))), max(-32768, min(32767, int(r * 32767))))
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(out)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 2, FS, FS * 4, 4, 16))
        f.write(b'data' + struct.pack('<I', len(out)) + out)
    print(f'{path}: {len(frames)} frames')


def drums():
    n = BEAT * 8
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    fr = []
    for i in range(n):
        b, tb = divmod(i, BEAT)
        t = tb / FS
        kick = 0.9 * math.sin(2 * math.pi * (45 * t + 60 * (1 - math.exp(-t * 30)) / 30)) * math.exp(-t * 8)
        snare = 0.0
        if b % 4 in (1, 3):
            snare = 0.45 * noise[i] * math.exp(-t * 20) + 0.25 * math.sin(2 * math.pi * 190 * t) * math.exp(-t * 25)
        te = (i % (BEAT // 2)) / FS
        hat = 0.16 * noise[(i * 7) % n] * math.exp(-te * 90)
        pan = 0.3 if (i // (BEAT // 2)) % 2 else -0.3
        m = kick + snare
        fr.append((0.7 * (m + hat * (1 - pan)), 0.7 * (m + hat * (1 + pan))))
    return fr


def bass():
    n = BEAT * 8
    notes = [36, 36, 48, 36, 39, 36, 43, 41, 36, 36, 48, 36, 34, 34, 46, 38]   # one per eighth
    fr, ph = [], 0.0
    eighth = BEAT // 2
    for i in range(n):
        k, te = divmod(i, eighth)
        f = 440 * 2 ** ((notes[k % 16] - 69) / 12)
        ph = (ph + f / FS) % 1.0
        saw = 2 * ph - 1
        env = math.exp(-te / FS * 6) * min(1.0, te / 40)
        v = 0.45 * saw * env
        fr.append((v, v))
    # a one-pole lowpass for warmth
    y = 0.0
    out = []
    for l, _ in fr:
        y += 0.18 * (l - y)
        out.append((y * 1.6, y * 1.6))
    return out


def chords():
    n = BEAT * 8
    prog = [[57, 60, 64, 67], [53, 57, 60, 64], [55, 59, 62, 65], [52, 55, 59, 64]]
    half = BEAT * 2
    fr = []
    for i in range(n):
        c, tc = divmod(i, half)
        t = i / FS
        env = min(1.0, tc / (0.05 * FS)) * (0.6 + 0.4 * math.exp(-tc / FS * 2))
        l = r = 0.0
        for j, m in enumerate(prog[c % 4]):
            f = 440 * 2 ** ((m - 69) / 12)
            l += math.sin(2 * math.pi * f * 1.002 * t + j)
            r += math.sin(2 * math.pi * f * 0.998 * t + 2 * j)
        fr.append((0.11 * l * env, 0.11 * r * env))
    return fr


def perc16():
    sixteenth = BEAT // 4
    n = sixteenth * 16
    fr = []
    for i in range(n):
        k, ti = divmod(i, sixteenth)
        t = ti / FS
        f = 80 * 1.35 ** k
        tone = math.sin(2 * math.pi * f * t * (1 + 0.5 * math.exp(-t * 40))) * math.exp(-t * (14 + k * 2))
        nz = rnd.uniform(-1, 1) * math.exp(-t * (30 + 5 * k)) * (k % 3) / 2
        v = 0.6 * tone + 0.35 * nz
        pan = ((k % 5) - 2) * 0.15
        fr.append((v * (1 - pan), v * (1 + pan)))
    return fr


root = sys.argv[1]
audio = os.path.join(root, 'audio')
os.makedirs(audio, exist_ok=True)
write(os.path.join(audio, 'a_drums_bpm120.wav'), drums())
write(os.path.join(audio, 'b_bass_bpm120.wav'), bass())
write(os.path.join(audio, 'c_chords_bpm120.wav'), chords())
write(os.path.join(audio, 'd_perc16_bpm120.wav'), perc16())
