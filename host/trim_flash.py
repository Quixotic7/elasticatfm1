#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Cuts an Elasticat flash image (1 MiB, mostly erased 0xFF) down to its header sector + the project record, for
tests that boot from a save (rfm1_run --flash-in): the core pads the rest with 0xFF.
0.6.1: the flash holds only the last project's name ("ECLP"); the project is the file <folder>/data/elasticat/
projects/<name>.ecp. Given the folder, the file goes into the record (a 0.6.0-style project record, which a 0.6.1
firmware still loads at power-on), so the test stays one flash file.
   trim_flash.py <in> <out> [<folder>]"""
import os
import struct
import sys

PROJECT, NAME = 0x4A504345, 0x504C4345   # "ECPJ", "ECLP"
d = bytearray(open(sys.argv[1], 'rb').read())
magic, = struct.unpack_from('<I', d, 4096)
if magic == NAME:
    if len(sys.argv) < 4:
        sys.exit('the flash names a project file: give the folder it was saved in')
    n = d[4096 + 6]
    name = d[4096 + 7:4096 + 7 + n].decode()
    rec = open(os.path.join(sys.argv[3], 'data', 'elasticat', 'projects', name + '.ecp'), 'rb').read()
    d[4096:4096 + len(rec)] = rec
    print(f'{name}.ecp put into the record')
    magic, = struct.unpack_from('<I', d, 4096)
if magic != PROJECT:
    sys.exit('no project record')
magic, ver, _, n = struct.unpack_from('<IHHI', d, 4096)
open(sys.argv[2], 'wb').write(d[:4096 + 16 + n])
print(f'{sys.argv[2]}: {4096 + 16 + n} bytes (project record v{ver}, {n} bytes)')
