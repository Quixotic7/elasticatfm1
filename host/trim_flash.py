#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Cuts an Elasticat flash image (1 MiB, mostly erased 0xFF) down to its header sector + the project record, for
tests that boot from a save (rfm1_run --flash-in): the core pads the rest with 0xFF.   trim_flash.py <in> <out>"""
import struct
import sys

d = open(sys.argv[1], 'rb').read()
magic, ver, _, n = struct.unpack_from('<IHHI', d, 4096)
if magic != 0x4A504345:
    sys.exit('no project record')
open(sys.argv[2], 'wb').write(d[:4096 + 16 + n])
print(f'{sys.argv[2]}: {4096 + 16 + n} bytes (project record v{ver}, {n} bytes)')
