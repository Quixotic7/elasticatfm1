# SPDX-License-Identifier: GPL-3.0-only
"""The 0.6.0 mockups' checks (tools/mockups/README.md): the owner's 120x120 grid (no 1-pixel details: every 2x2 block of
the 240x240 LCD, so every 4x4 block of a 480x480 PNG, is one colour) and the 16-colour palette (only the theme's own
colours, at most 16).

  python3 check_png.py <dir>              every mockup PNG in <dir> (palette.json there); exit 1 on any failure
  python3 check_png.py --survey <root>    the firmware's own screenshots under <root> (240 or 480 square): which break
                                          the grid rule, and how many blocks (a report; exit 0)
"""
import json
import os
import struct
import sys
import zlib


def read_png(path):
    """-> (w, h, rows of (r, g, b) tuples). 8-bit RGB / RGBA, non-interlaced: what the harness and Chromium write."""
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    at, idat, w = 8, b"", 0
    while at < len(data):
        n, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and ctype in (2, 6) and inter == 0, f"{path}: unsupported PNG"
            bpp = 3 if ctype == 2 else 4
        elif kind == b"IDAT":
            idat += body
        at += 12 + n
    raw, stride, rows, prev = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp)
    for y in range(h):
        ft, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if ft == 1: line[i] = (line[i] + a) & 255
            elif ft == 2: line[i] = (line[i] + b) & 255
            elif ft == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * bpp:x * bpp + 3]) for x in range(w)])
        prev = line
    return w, h, rows


def corner(x, y, x0, y0, w, h, r):
    """in one of the r x r corner squares (the panel's LCD has rounded, anti-aliased corners: RemoteFM1's bezel)"""
    return r and (x < x0 + r or x >= x0 + w - r) and (y < y0 + r or y >= y0 + h - r)


def bad_blocks(rows, x0, y0, w, h, n, r=0):
    """the n x n blocks (from x0, y0) that are not one colour"""
    bad = 0
    for by in range(y0, y0 + h, n):
        for bx in range(x0, x0 + w, n):
            if corner(bx, by, x0, y0, w, h, r) or corner(bx + n - 1, by + n - 1, x0, y0, w, h, r):
                continue
            c = rows[by][bx]
            if any(rows[by + j][bx + i] != c for j in range(n) for i in range(n)):
                bad += 1
    return bad


def q565(hexs):
    """a theme colour as the LCD shows it: RGB565, expanded as host/ec_png.c does"""
    v = int(hexs[1:], 16)
    r, g, b = v >> 16 & 255, v >> 8 & 255, v & 255
    return ((r >> 3) * 255 // 31, (g >> 2) * 255 // 63, (b >> 3) * 255 // 31)


def colours(rows, x0, y0, w, h, r=0):
    return {rows[y][x] for y in range(y0, y0 + h) for x in range(x0, x0 + w) if not corner(x, y, x0, y0, w, h, r)}


def check_dir(d):
    pal = json.load(open(os.path.join(d, "palette.json")))
    themes = {k: {q565(c) for c in v} for k, v in pal["themes"].items()}
    names = list(pal["themes"])
    fails = 0
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".png"):
            continue
        w, h, rows = read_png(os.path.join(d, fn))
        msgs = []
        if fn.endswith("_panel.png"):
            # the FM-1 panel is RemoteFM1's drawing (anti-aliased by design); the rules apply to its LCD, whose box the
            # renderer recorded beside it
            box = json.load(open(os.path.join(d, fn[:-4] + ".lcd.json")))
            r = -(-box["r"] // 4) * 4                       # the corner radius, whole blocks
            regions = [(box["x"], box["y"], box["s"], box["s"], box["s"] // 120, "CREAM", r)]
        elif fn == "_themes.png":
            cw = w // len(names)
            regions = [(i * cw, 0, cw, h, 4, n, 0) for i, n in enumerate(names)]
        else:
            regions = [(0, 0, w, h, 4, "CREAM", 0)]
        for x0, y0, rw, rh, n, theme, r in regions:
            b = bad_blocks(rows, x0, y0, rw, rh, n, r)
            if b:
                msgs.append(f"{b} {n}x{n} blocks not one colour ({theme})")
            cs = colours(rows, x0, y0, rw, rh, r)
            extra = cs - themes[theme]
            if len(cs) > 16 or extra:
                msgs.append(f"{len(cs)} colours, {len(extra)} outside {theme}'s 16")
        print(("FAIL " if msgs else "ok   ") + fn + (": " + "; ".join(msgs) if msgs else f" {w}x{h}"))
        fails += bool(msgs)
    for fn in sorted(os.listdir(d)):                # GIFs: exempt from the block check, but only palette colours
        if not fn.endswith(".gif"):
            continue
        try:
            from PIL import Image, ImageSequence
        except ImportError:
            print("skip " + fn + ": no Pillow")
            continue
        im = Image.open(os.path.join(d, fn))
        cs = set()
        for fr in ImageSequence.Iterator(im):
            cs |= {c for _, c in fr.convert("RGB").getcolors(1 << 16)}
        extra = cs - themes["CREAM"]
        bad = len(cs) > 16 or extra
        print(("FAIL " if bad else "ok   ") + fn + (f": {len(cs)} colours, {len(extra)} outside CREAM's 16" if bad else f" {im.n_frames} frames"))
        fails += bool(bad)
    print(f"{fails} failing" if fails else "all ok")
    return 1 if fails else 0


def survey(root):
    hits = []
    for dp, _, fs in os.walk(root):
        if "ux-v2-mockups" in dp:
            continue
        for fn in sorted(fs):
            if not fn.endswith(".png"):
                continue
            p = os.path.join(dp, fn)
            try:
                w, h, rows = read_png(p)
            except Exception:
                continue
            if w != h or w not in (240, 480):
                continue
            b = bad_blocks(rows, 0, 0, w, h, w // 120)
            hits.append((b, os.path.relpath(p, root)))
    for b, p in sorted(hits, key=lambda t: t[1]):
        print(f"{'BREAKS' if b else 'ok    '} {p}" + (f": {b} of 14400 grid pixels not uniform" if b else ""))
    print(f"{sum(1 for b, _ in hits if b)} of {len(hits)} firmware screenshots break the 120x120 grid rule")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--survey":
        sys.exit(survey(sys.argv[2]))
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(check_dir(sys.argv[1]))
