#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A fake FM-1 on a virtual CoreMIDI port, speaking the RemoteFM1 link protocol v2 (RemoteFM1/docs/PROTOCOL.md) so
RemoteFM1 hosts (the iOS app on a Mac, the plug-in) link to it as a unit, and playing an ec_run script
(host/ec_run.c: press / lower / upper / turn / master lines, device ms) as the unit's panel events.

  fake_fm1.py                       the unit alone (link it, press nothing)
  fake_fm1.py --script S.txt [--delay 3] [--ms 180000]   the script from --delay s after the session starts
  fake_fm1.py --script S.txt --at-start      the script from the moment the fake starts (no wait for a session)
Needs python-rtmidi. The unplugged real unit must not be present, or the host may pick it instead."""
import argparse, os, re, struct, sys, threading, time
import rtmidi

PORT = 'FM-1 promo'
HELLO, SESSION, LEDS, TILES, PANEL_REQ = 0x01, 0x10, 0x11, 0x12, 0x13
HELLO_R, ACK, FRAME_ACK, PANEL_R = 0x41, 0x46, 0x52, 0x53
BTN = {'OCT-': 0, 'OCT+': 1, 'FX': 2, 'SEL': 3, 'ENV': 4, 'LFO': 5, 'EDIT': 6, 'GLO': 7, 'HOME': 8, 'SAVE': 9,
       'ARP': 10, 'SEQ': 11, 'PLAY': 12, 'REC': 13}
ENC = {'SELECT': 0, 'ALGO': 1, 'PRESETS': 6, 'K1': 2, 'K2': 3, 'K3': 4, 'K4': 5}   # Elasticat's ENC_ID (Felucca's)
NAT = [s in (0, 2, 4, 6, 7, 9, 11) for s in range(12)]
LOWER = [k for k in range(27) if NAT[k % 12]]
UPPER = [k for k in range(27) if not NAT[k % 12]]


def pack7(b):
    out = bytearray()
    for i in range(0, len(b), 7):
        g = b[i:i + 7]
        out.append(sum(((x >> 7) & 1) << k for k, x in enumerate(g)))
        out += bytes(x & 0x7F for x in g)
    return bytes(out)


def unpack7(b):
    out = bytearray(); msb = 0; g = 0
    for x in b:
        if g == 0:
            msb = x; g = 1; continue
        out.append((x & 0x7F) | (((msb >> (g - 1)) & 1) << 7))
        g = 0 if g == 7 else g + 1
    return bytes(out)


class Fake:
    def __init__(self):
        self.out = rtmidi.MidiOut(); self.out.open_virtual_port(PORT)
        self.inp = rtmidi.MidiIn(); self.inp.ignore_types(sysex=False, timing=True, active_sense=True)
        self.inp.open_virtual_port(PORT); self.inp.set_callback(self.on_midi)
        self.lock = threading.Lock()
        self.session = False; self.keys = 0; self.buttons = 0; self.enc = [0] * 7; self.master = 512
        self.frames = 0; self.session_event = threading.Event()
        self.lcd = bytearray(240 * 240 * 2)   # BE RGB565, the shadow

    # ---- the link (device -> host as SysEx)
    def send(self, cmd, body=b''):
        with self.lock:
            self.out.send_message([0xF0, 0x7D, 0x52, 0x46, cmd] + list(pack7(body)) + [0xF7])

    def hello_r(self):
        b = bytearray(67)
        b[0:9] = b'RemoteFM1'; b[16] = 2; b[17:22] = b'promo'
        b[25] = 1 | 4; b[26] = 1; b[27] = 0; b[28] = 0
        struct.pack_into('<H', b, 29, 64); struct.pack_into('<II', b, 31, 0, 0)
        struct.pack_into('<HH', b, 39, 1024, 16384); b[43:51] = b'FM-1_PRO'
        struct.pack_into('<HHHBB', b, 51, 240, 240, 1 | 32 | 64, 2, 1 if self.session else 0)
        b[59:64] = b'promo'
        self.send(HELLO_R, bytes(b))

    def panel_r(self):
        b = struct.pack('<IH', self.keys & 0x7FFFFFF, self.buttons & 0x3FFF)
        b += struct.pack('<7h', *[((e + 32768) % 65536) - 32768 for e in self.enc])
        b += struct.pack('<HHB', self.master, 0, 1 if self.session else 0)
        self.send(PANEL_R, b)

    def on_midi(self, event, _):
        msg, _dt = event
        if len(msg) < 6 or msg[0] != 0xF0 or msg[1:4] != [0x7D, 0x52, 0x46] or msg[-1] != 0xF7:
            return
        cmd = msg[4]; body = unpack7(bytes(msg[5:-1]))
        if cmd == HELLO:
            print('HELLO from the host', flush=True); self.hello_r()
        elif cmd == SESSION:
            start = body[0] if body else 0
            self.session = bool(start)
            self.send(ACK)
            if start:
                self.frames = 0; self.panel_r(); self.session_event.set()
                print('session started', flush=True)
            else:
                print('session ended', flush=True)
        elif cmd == TILES:
            if len(body) >= 4:
                frame, flags, nrects = struct.unpack_from('<HBB', body, 0)
                o = 4
                for _ in range(nrects):
                    if o + 5 > len(body): break
                    x, y, w, hh, enc = body[o:o + 5]; o += 5
                    if enc == 0:
                        for r in range(hh):
                            s = (y + r) * 480 + x * 2
                            self.lcd[s:s + w * 2] = body[o + r * w * 2:o + (r + 1) * w * 2]
                        o += w * hh * 2
                    elif enc == 1:
                        px = body[o:o + 2]; o += 2
                        for r in range(hh):
                            s = (y + r) * 480 + x * 2
                            self.lcd[s:s + w * 2] = px * w
                    elif enc == 2:                          # PAL: n, n colours, rows of indices (byte-aligned, MSB first)
                        n = body[o]; pal = body[o + 1:o + 1 + 2 * n]; o += 1 + 2 * n
                        bpp = 1 if n <= 2 else 2 if n <= 4 else 4
                        rb = (w * bpp + 7) // 8
                        for r in range(hh):
                            row = body[o + r * rb:o + (r + 1) * rb]; s = (y + r) * 480 + x * 2
                            for c in range(w):
                                bit = c * bpp
                                i = (row[bit >> 3] >> (8 - bpp - (bit & 7))) & ((1 << bpp) - 1)
                                self.lcd[s + 2 * c:s + 2 * c + 2] = pal[2 * i:2 * i + 2]
                        o += rb * hh
                    elif enc == 3:                          # RLE: len8 colour16, runs across rows
                        left = w * hh; c = 0
                        while left > 0:
                            ln = body[o] + 1; px = body[o + 1:o + 3]; o += 3
                            for _ in range(ln):
                                r, cc = divmod(c, w); s = (y + r) * 480 + (x + cc) * 2
                                self.lcd[s:s + 2] = px; c += 1
                            left -= ln
                    elif enc == 4:                          # COPY sx sy from the shadow
                        sx, sy = body[o], body[o + 1]; o += 2
                        blk = [bytes(self.lcd[(sy + r) * 480 + sx * 2:(sy + r) * 480 + (sx + w) * 2]) for r in range(hh)]
                        for r in range(hh):
                            s = (y + r) * 480 + x * 2; self.lcd[s:s + w * 2] = blk[r]
                    else:
                        break
                if flags & 1:
                    self.frames += 1
                    self.send(FRAME_ACK, struct.pack('<HBIIB', frame, 0, 800, len(body), 0))
        elif cmd == PANEL_REQ:
            self.panel_r()
        # LEDS and the rest: nothing to answer

    def heartbeat(self):
        while True:
            time.sleep(0.5)
            if self.session: self.panel_r()

    def shot(self, path):
        from PIL import Image
        im = Image.new('RGB', (240, 240))
        px = im.load()
        for y in range(240):
            for x in range(240):
                v = (self.lcd[y * 480 + x * 2] << 8) | self.lcd[y * 480 + x * 2 + 1]
                px[x, y] = (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
        im.save(path)

    def commands(self, path):
        """live tests: lines appended to path are script lines, played now (their @MS ignored); 'shot FILE' saves the LCD"""
        seen = 0
        while True:
            time.sleep(0.2)
            try: lines = open(path).read().splitlines()
            except FileNotFoundError: continue
            for ln in lines[seen:]:
                ln = ln.strip()
                if not ln: continue
                if ln.startswith('shot '):
                    self.shot(ln[5:].strip()); print('shot', ln[5:], flush=True); continue
                try:
                    ev = parse_script_lines([ln])
                except Exception as e:
                    print('bad command', ln, e, flush=True); ev = []
                t0 = time.monotonic() - (ev[0][0] / 1000.0 if ev else 0)
                threading.Thread(target=play, args=(self, ev, t0, 10 ** 9), daemon=True).start()
            seen = len(lines)

    # ---- the panel (device -> host as plain MIDI)
    def key(self, k, down, vel=100):
        with self.lock:
            if down: self.keys |= 1 << k; self.out.send_message([0x9F, 0x53 + k, vel])
            else: self.keys &= ~(1 << k); self.out.send_message([0x8F, 0x53 + k, 0])

    def button(self, b, down):
        with self.lock:
            if down: self.buttons |= 1 << b; self.out.send_message([0x9E, b, 127])
            else: self.buttons &= ~(1 << b); self.out.send_message([0x8E, b, 0])

    def turn(self, e, n):
        while n:
            d = max(-63, min(63, n)); n -= d
            with self.lock:
                self.enc[e] += d; self.out.send_message([0xBE, 16 + e, 64 + d])

    def set_master(self, adc):
        v = (adc << 4) & 0x3FFF
        with self.lock:
            self.master = adc; self.out.send_message([0xBE, 7, v >> 7]); self.out.send_message([0xBE, 0x27, v & 0x7F])


def parse_script(path):
    return parse_script_lines(open(path))


def parse_script_lines(lines):
    ev = []   # (ms, fn)
    for ln in lines:
        ln = ln.split('#')[0].strip()
        if not ln: continue
        m = re.match(r'(press|lower|upper|turn|master)\s+(\S+)', ln)
        if not m: continue
        what, arg = m.groups()
        if what == 'turn':
            role, rest = arg.split('@'); at, n = rest.split(':')
            ev.append((int(at), ('turn', ENC[role], int(n))))
        elif what == 'master':
            v, at = arg.split('@'); ev.append((int(at), ('master', int(v))))
        else:
            name, rest = arg.split('@')
            m2 = re.match(r'(\d+)(?:\+(\d+))?(?::(\d+))?', rest)
            at = int(m2.group(1)); dur = int(m2.group(2) or (60 if what == 'press' else 100)); vel = int(m2.group(3) or 100)
            if what == 'press':
                b = BTN[name]; ev.append((at, ('button', b, 1))); ev.append((at + dur, ('button', b, 0)))
            else:
                k = (LOWER if what == 'lower' else UPPER)[int(name)]
                ev.append((at, ('key', k, 1, vel))); ev.append((at + dur, ('key', k, 0, 0)))
    ev.sort(key=lambda e: e[0])
    return ev


def play(f, ev, t0, limit_ms):
    for at, e in ev:
        if at > limit_ms: break
        now = time.monotonic()
        due = t0 + at / 1000.0
        if due > now: time.sleep(due - now)
        if e[0] == 'turn': f.turn(e[1], e[2])
        elif e[0] == 'master': f.set_master(e[1])
        elif e[0] == 'button': f.button(e[1], e[2])
        else: f.key(e[1], e[2], e[3] or 100)
    if limit_ms < 10 ** 9: print('script done', flush=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--script'); ap.add_argument('--delay', type=float, default=3.0)
    ap.add_argument('--ms', type=int, default=180000); ap.add_argument('--at-start', action='store_true')
    ap.add_argument('--mark', help='a file written (with the monotonic start time) when the script starts')
    ap.add_argument('--commands', help='a file watched for script lines to play at once (live tests), and "shot FILE"')
    a = ap.parse_args()
    f = Fake()
    threading.Thread(target=f.heartbeat, daemon=True).start()
    if a.commands: threading.Thread(target=f.commands, args=(a.commands,), daemon=True).start()
    print('fake FM-1 up as', PORT, flush=True)
    if a.script:
        ev = parse_script(a.script)
        if not a.at_start:
            f.session_event.wait()
        t0 = time.monotonic() + a.delay
        if a.mark: open(a.mark, 'w').write(f'{t0:.6f} {time.time() + a.delay:.6f}\n')
        print(f'script starts in {a.delay} s ({len(ev)} events)', flush=True)
        play(f, ev, t0, a.ms)
    try:
        while True: time.sleep(1)
    except KeyboardInterrupt:
        pass
