/*
 * smsvdp test ROM: VDP data port bursts with hand-counted gaps, each started at
 * a fixed scanline (polling the V counter), every frame, interrupts off.
 *
 * line  40: OTIR, 32 bytes to VRAM ............ gap 21 (OUTI 16 + repeat 5)
 * line  60: OUTI unrolled, 32 bytes .......... gap 16
 * line  80: OUT (n),A unrolled, 32 bytes ..... gap 11
 * line 100: OUTI + 2 NOP unrolled, 32 bytes .. gap 24
 * line 120: OUTI + JP NZ loop, 32 bytes ...... gap 26 (16 + 10)
 * line 140: OUTI + JR NZ loop, 32 bytes ...... gap 28 (16 + 12)
 * line 150: OUTI + NOP + JP NZ loop, 32 bytes  gap 30
 * line 160: OTIR, 16 bytes to CRAM ........... gap 21
 * line 170: display off, OTIR 32 bytes, display on (not active display)
 * line 200: OTIR, 64 bytes in VBlank (not active display)
 *
 * In each burst the first write follows the previous burst by many lines, so
 * per frame the expected counts of active-display writes are:
 *   gap < 23: 31 + 31 + 31 + 15        = 108
 *   gap < 26: 108 + 31 (gap 24)        = 139
 *   gap < 29: 139 + 31 (26) + 31 (28)  = 201
 * and 32*7 + 16 = 240 active-display writes in total.
 * MIT license.
 */
#include "SMSlib.h"

void frame_bursts(void) __naked {
  __asm
    di
    ; ---- line 40: OTIR to VRAM $2000
    ld b, #40
    call wait_line
    ld a, #0x00
    out (#0xbf), a
    ld a, #0x60          ; $2000 | write
    out (#0xbf), a
    ld hl, #0x0100
    ld bc, #0x20be       ; B = 32 bytes, C = data port
    otir

    ; ---- line 60: OUTI unrolled
    ld b, #60
    call wait_line
    ld hl, #0x0100
    ld bc, #0x20be
    .rept 32
    outi
    .endm

    ; ---- line 80: OUT (n),A unrolled
    ld b, #80
    call wait_line
    ld a, #0x55
    .rept 32
    out (#0xbe), a
    .endm

    ; ---- line 100: OUTI + NOP + NOP unrolled
    ld b, #100
    call wait_line
    ld hl, #0x0100
    ld bc, #0x20be
    .rept 32
    outi
    nop
    nop
    .endm

    ; ---- line 120: OUTI + JP NZ
    ld b, #120
    call wait_line
    ld hl, #0x0100
    ld bc, #0x20be
  00120$:
    outi
    jp nz, 00120$

    ; ---- line 140: OUTI + JR NZ
    ld b, #140
    call wait_line
    ld hl, #0x0100
    ld bc, #0x20be
  00140$:
    outi
    jr nz, 00140$

    ; ---- line 150: OUTI + NOP + JP NZ
    ld b, #150
    call wait_line
    ld hl, #0x0100
    ld bc, #0x20be
  00150$:
    outi
    nop
    jp nz, 00150$

    ; ---- line 160: OTIR, 16 bytes to CRAM 0
    ld b, #160
    call wait_line
    ld a, #0x00
    out (#0xbf), a
    ld a, #0xc0
    out (#0xbf), a
    ld hl, #palette
    ld bc, #0x10be
    otir

    ; ---- line 170: display off, OTIR to VRAM, display on
    ld b, #170
    call wait_line
    ld a, #0xa0          ; reg 1: display off, frame IRQ bit kept
    out (#0xbf), a
    ld a, #0x81
    out (#0xbf), a
    ld a, #0x00
    out (#0xbf), a
    ld a, #0x60
    out (#0xbf), a
    ld hl, #0x0100
    ld bc, #0x20be
    otir
    ld a, #0xe0          ; reg 1: display on
    out (#0xbf), a
    ld a, #0x81
    out (#0xbf), a

    ; ---- line 200 (VBlank): OTIR, 64 bytes
    ld b, #200
    call wait_line
    ld a, #0x00
    out (#0xbf), a
    ld a, #0x60
    out (#0xbf), a
    ld hl, #0x0100
    ld bc, #0x40be
    otir
    ret

  wait_line:             ; wait until the V counter equals B
    in a, (#0x7e)
    cp b
    jr nz, wait_line
    ret

  palette:
    .db 0x00, 0x3f, 0x30, 0x0c, 0x03, 0x3c, 0x33, 0x0f
    .db 0x15, 0x2a, 0x10, 0x04, 0x01, 0x14, 0x11, 0x05
  __endasm;
}

void main(void) {
  SMS_displayOn();
  for (;;) frame_bursts();
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(1, 0, "aitoollabs", "smsvdp test ROM", "Hand-counted VDP write gaps");
