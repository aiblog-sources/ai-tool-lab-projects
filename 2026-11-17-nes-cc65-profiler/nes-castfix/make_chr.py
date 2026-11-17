#!/usr/bin/env python3
"""Writes the 8 KB CHR ROM shared by the test ROM and the Claude scenes (MIT).

Pattern table 0 ($0000, used by 8x8 sprites):
  $00 blank          $01 solid colour 1   $02 solid colour 2   $03 solid colour 3
  $04 ball (8x8)     $05 ring             $06 ship             $07 bullet (4x4)
  $08 small dot (2x2) $09 star            $0A box outline      $0B diamond
  $10/$11 8x16 pair: body top / body bottom (solid with eyes)
  $12/$13 8x16 pair: ball top / ball bottom
Every other tile is blank. Pattern table 1 ($1000) is a copy of table 0, so
8x16 sprites with an odd tile number show the same shapes.
"""
import sys

def tile(rows):
    """rows: 8 strings of '0'-'3'. Returns 16 bytes (two bit planes)."""
    lo = bytes(int(''.join('1' if int(c) & 1 else '0' for c in r), 2) for r in rows)
    hi = bytes(int(''.join('1' if int(c) & 2 else '0' for c in r), 2) for r in rows)
    return lo + hi

T = {}
T[0x01] = ['11111111'] * 8
T[0x02] = ['22222222'] * 8
T[0x03] = ['33333333'] * 8
T[0x04] = ['00111100', '01122110', '11222211', '12233221', '12233221', '11222211', '01122110', '00111100']
T[0x05] = ['00111100', '01100110', '11000011', '10000001', '10000001', '11000011', '01100110', '00111100']
T[0x06] = ['00033000', '00033000', '00133100', '01133110', '11122111', '11122111', '11011011', '10000001']
T[0x07] = ['00000000', '00000000', '00333300', '00322300', '00322300', '00333300', '00000000', '00000000']
T[0x08] = ['00000000'] * 3 + ['00033000'] * 2 + ['00000000'] * 3
T[0x09] = ['00030000', '00030000', '00333000', '33333330', '00333000', '00303000', '03000300', '00000000']
T[0x0A] = ['11111111', '10000001', '10000001', '10000001', '10000001', '10000001', '10000001', '11111111']
T[0x0B] = ['00011000', '00122100', '01222210', '12222221', '12222221', '01222210', '00122100', '00011000']
T[0x10] = ['01111110', '11111111', '11311311', '11311311', '11111111', '11111111', '11222211', '11111111']
T[0x11] = ['11111111', '11111111', '11111111', '11111111', '01111110', '01100110', '01100110', '11100111']
T[0x12] = ['00000000', '00011000', '00122100', '01222210', '01233210', '12233221', '12222221', '12222221']
T[0x13] = ['12222221', '12222221', '12222221', '01222210', '01222210', '00122100', '00011000', '00000000']

table = bytearray(4096)
for n, rows in T.items():
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), hex(n)
    table[n * 16:(n + 1) * 16] = tile(rows)
open(sys.argv[1], 'wb').write(bytes(table) * 2)
