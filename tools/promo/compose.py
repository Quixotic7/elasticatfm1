#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Composes the Elasticat promo: the harness's screen frames + panel log + script (knob turns) -> 1920x1080 frames
piped into ffmpeg with the run's WAV.  compose.py <dir> <out.mp4>  (dir holds frames3/, promo3.log, promo.txt,
promo3.wav)."""
import math, os, re, subprocess, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

D = sys.argv[1]; OUT = sys.argv[2]
FPS = 20; W, H = 1920, 1080
BG = (18, 18, 14); INK = (240, 236, 220); MUTE = (140, 136, 120); ACC = (255, 106, 31); BLUE = (63, 140, 255)
PANEL = (34, 33, 28); LINE = (60, 58, 50)
F = '/System/Library/Fonts/Supplemental/DIN Condensed Bold.ttf'
F2 = '/System/Library/Fonts/HelveticaNeue.ttc'
FM = '/System/Library/Fonts/Menlo.ttc'
def font(p, s):
    return ImageFont.truetype(p, s)
f_title = font(F, 150); f_cap = font(F, 64); f_sub = font(F2, 30); f_lab = font(FM, 15); f_small = font(F2, 22)

# ---- the panel log
lines = open(f'{D}/promo3.log').read().split('\n')
btn_id = [int(x) for x in lines[0].split()[1:]]          # FM1P_B_* order -> matrix id
LOG = {}
for ln in lines[1:]:
    if not ln.strip(): continue
    a = ln.split(); ms = int(a[0]); keys = int(a[1], 16); btns = int(a[2], 16); green = int(a[3])
    lv = [int(x) for x in a[4:45]]
    LOG[ms] = (keys, btns, green, lv)
# keys: 27 note keys from F; naturals = white
NAT = [s in (0, 2, 4, 6, 7, 9, 11) for s in range(12)]
LOWER = [k for k in range(27) if NAT[k % 12]]; UPPER = [k for k in range(27) if not NAT[k % 12]]
BTN_NAMES = ['FX', 'FILTER', 'AMP', 'MOD', 'MIX', 'SHIFT', 'LOOP', 'FILE', 'XFADE', 'PATTERN', 'PLAY', 'REC', 'F1', 'F2']
BTN_PRINT = ['FX', 'SEL', 'ENV', 'LFO', 'EDIT', 'GLO', 'HOME', 'SAVE', 'ARP', 'SEQ', 'PLAY', 'REC', 'OCT-', 'OCT+']

# ---- knob turns from the script
ROLES = ['SELECT', 'ALGO', 'PRESETS', 'K1', 'K2', 'K3', 'K4']
turns = []   # (ms, role index, n)
for ln in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'promo.txt')):
    m = re.match(r'\s*turn (\w+)@(\d+):(-?\d+)', ln)
    if m: turns.append((int(m.group(2)), ROLES.index(m.group(1)), int(m.group(3))))
turns.sort()

# ---- captions: (from_s, to_s, title, sub)
CAPS = [
    (0, 5.4, 'ELASTICAT FM-1', 'a loop mangler for the M-VAVE FM-1 panel'),
    (5.6, 10.2, 'BROWSE', 'your own folder of samples: they preview as you scroll'),
    (10.4, 22.2, '16 LOOP KEYS', 'hold one, hold two, stutter, let go: the playhead comes back'),
    (22.5, 27.8, 'SIX TRACKS', '128 sample slots, each track with its own machine and warp'),
    (28, 39.8, 'FILTER', 'per track, with envelopes, LFOs: every value locks on a step'),
    (40, 54.8, 'KEYBOARD', 'play any loop chromatically'),
    (55, 58.6, '13 WARP MODES', 'tape, vari, domino, grain, OLA, pitch, harm, wavetable, freeze, formant, gstr, chop'),
    (58.8, 74.8, 'GRAIN', 'density, size, spread, rate: the grains drawn where they read'),
    (75, 91.8, 'FX', 'two inserts per track, 18 machines, two send buses, a master insert'),
    (92, 99.8, 'ECHO ON THE ACID', 'hold a key for the type, turn for the mix'),
    (100, 117.8, 'MIX', 'tap to select, hold to mute, double-tap to solo, and the CAT'),
    (118, 134.8, 'CROSSFADER', 'two scenes of every value, a fader with glides on the keys'),
    (135, 149.8, 'TRIG', '128 steps, p-locks, conditions, chance, ratchets, 64 patterns'),
    (150, 159.8, 'KEYBOARD ON THE BREAK', 'and live recording of everything you play'),
    (160, 171.8, 'FREE AND OPEN SOURCE', 'a Fantasy CFW: runs in RemoteFM1 on the web, iOS, Android and in the FM1VST plug-in'),
]
OUTRO = 172.4

def rr(d, box, r, fill, outline=None, width=1):
    d.rounded_rectangle(box, r, fill=fill, outline=outline, width=width)

def led_col(v, held, base=(70, 66, 56)):
    if held: return (255, 180, 120)
    t = v / 255.0
    return tuple(int(b + (a - b) * t) for a, b in zip((255, 150, 60), base))

def draw_panel(d, img, ms):
    keys, btns, green, lv = LOG.get(ms, (0, 0, 0, [0] * 41))
    X0, Y0 = 930, 120
    rr(d, (X0 - 30, Y0 - 30, X0 + 930, Y0 + 760), 28, PANEL, LINE, 2)
    # knobs
    def knob(cx, cy, name, role, r=30):
        turned = None; angle = 0
        for t, ri, n in turns:
            if ri == role and t <= ms:
                angle += n
                if ms - t < 350: turned = (ms - t) / 350
        angle = max(-140, min(140, angle * 3))
        if turned is not None:
            g = int(255 * (1 - turned))
            d.ellipse((cx - r - 10, cy - r - 10, cx + r + 10, cy + r + 10), outline=(255, 120, 40, g), width=4)
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(52, 50, 44), outline=(90, 86, 74), width=2)
        a = math.radians(angle - 90)
        d.line((cx + math.cos(a) * r * 0.35, cy + math.sin(a) * r * 0.35, cx + math.cos(a) * r * 0.9, cy + math.sin(a) * r * 0.9),
               fill=ACC if turned is not None else INK, width=4)
        d.text((cx, cy + r + 6), name, font=f_lab, fill=MUTE, anchor='mt')
    knob(X0 + 40, Y0 + 40, 'MASTER', -1); knob(X0 + 130, Y0 + 40, 'SEL', 0)
    knob(X0 + 40, Y0 + 150, 'PRESETS', 2); knob(X0 + 130, Y0 + 150, 'ALGO', 1)
    for i in range(4):
        knob(X0 + 560 + i * 100, Y0 + 40, f'K{i + 1}', 3 + i)
    # buttons 2 x 6 + F1 F2
    def button(x, y, w, h, label, printed, bidx):
        mid = btn_id[bidx]
        held = (btns >> mid) & 1
        v = lv[mid]
        col = (255, 150, 60) if held else led_col(v, False, (48, 46, 40))
        rr(d, (x, y, x + w, y + h), 7, col, (100, 96, 84), 1)
        tc = (20, 18, 14) if held or v > 120 else INK
        d.text((x + w / 2, y + h / 2 - 7), label, font=f_small, fill=tc, anchor='mm')
        d.text((x + w / 2, y + h / 2 + 14), printed, font=f_lab, fill=tc if held or v > 120 else MUTE, anchor='mm')
    order = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
    for n, b in enumerate(order):
        r, c = divmod(n, 6)
        button(X0 + 300 + c * 104, Y0 + 120 + r * 70, 96, 60, BTN_NAMES[b], BTN_PRINT[b], b)
    button(X0 + 20, Y0 + 230, 80, 50, 'F1', 'OCT-', 12); button(X0 + 110, Y0 + 230, 80, 50, 'F2', 'OCT+', 13)
    # black keys
    for i, k in enumerate(UPPER):
        x = X0 + 20 + i * 82
        held = (keys >> k) & 1
        rr(d, (x, Y0 + 330, x + 70, Y0 + 440), 6, led_col(lv[14 + k], held, (40, 38, 32)), (90, 86, 74), 1)
        d.text((x + 35, Y0 + 452), f'B{i + 1}', font=f_lab, fill=MUTE, anchor='mt')
    # white keys
    for i, k in enumerate(LOWER):
        x = X0 + 20 + i * 56
        held = (keys >> k) & 1
        rr(d, (x, Y0 + 490, x + 50, Y0 + 690), 6, led_col(lv[14 + k], held, (58, 56, 50)), (100, 96, 84), 1)
        d.text((x + 25, Y0 + 700), f'W{i + 1}', font=f_lab, fill=MUTE, anchor='mt')
    # PLAY's green
    if green:
        mid = btn_id[10]; n = order.index(10); r, c = divmod(n, 6)
        d.ellipse((X0 + 300 + c * 104 + 84, Y0 + 120 + r * 70 + 4, X0 + 300 + c * 104 + 92, Y0 + 120 + r * 70 + 12), fill=(80, 255, 120))

def caption(d, t):
    for a, b, title, sub in CAPS:
        if a <= t <= b:
            fade = min(1, (t - a) / 0.3, (b - t) / 0.3 + 0.01)
            col = tuple(int(c * fade + BG[i] * (1 - fade)) for i, c in enumerate(INK))
            mcol = tuple(int(c * fade + BG[i] * (1 - fade)) for i, c in enumerate(MUTE))
            d.text((96, 975), title, font=f_cap, fill=col, anchor='ls')
            d.text((96, 1022), sub, font=f_sub, fill=mcol, anchor='ls')
            return

def frame(i):
    t = i / FPS; ms = i * 50
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img, 'RGBA')
    if t >= OUTRO:
        k = min(1, (t - OUTRO) / 0.6)
        col = tuple(int(c * k + BG[j] * (1 - k)) for j, c in enumerate(INK))
        mcol = tuple(int(c * k + BG[j] * (1 - k)) for j, c in enumerate(MUTE))
        acol = tuple(int(c * k + BG[j] * (1 - k)) for j, c in enumerate(ACC))
        d.text((W / 2, 380), 'ELASTICAT FM-1', font=f_title, fill=col, anchor='mm')
        d.text((W / 2, 490), 'the norns loop mangler, rebuilt for the FM-1 panel', font=f_sub, fill=mcol, anchor='mm')
        d.text((W / 2, 600), 'quixotic7.github.io/elasticatfm1', font=f_cap, fill=acol, anchor='mm')
        d.text((W / 2, 670), 'in the RemoteFM1 catalog  ·  GPL-3.0  ·  github.com/Quixotic7/elasticatfm1', font=f_sub, fill=mcol, anchor='mm')
        return img
    scr = Image.open(f'{D}/frames3/f{i:05d}.png').convert('RGB').resize((720, 720), Image.NEAREST)
    rr(d, (80, 134, 832, 886), 22, (8, 8, 6), LINE, 2)
    img.paste(scr, (96, 150))
    draw_panel(d, img, ms)
    caption(d, t)
    d.text((1830, 1022), 'quixotic7.github.io/elasticatfm1', font=f_small, fill=MUTE, anchor='rs')
    return img

n = len([f for f in os.listdir(f'{D}/frames3') if f.endswith('.png')])
ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}', '-r', str(FPS), '-i', '-',
                       '-i', f'{D}/promo3.wav', '-af', 'loudnorm=I=-14:TP=-1.5:LRA=11', '-c:v', 'libx264', '-preset', 'medium', '-crf', '20',
                       '-pix_fmt', 'yuv420p', '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', OUT], stdin=subprocess.PIPE)
for i in range(n):
    ff.stdin.write(frame(i).tobytes())
    if i % 400 == 0: print(i, flush=True)
ff.stdin.close(); ff.wait(); print('done', n, 'frames')
