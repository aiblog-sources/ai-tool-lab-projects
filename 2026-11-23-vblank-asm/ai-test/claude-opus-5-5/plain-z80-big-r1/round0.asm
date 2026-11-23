; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM starting at the
; address stored (little-endian) at $C000/$C001.
; Must run inside VBlank: uses unrolled OUTI (16 cycles/byte),
; total ~12.5k cycles, which fits in the NTSC vertical blank.

vblank_update:
        ; --- set VRAM write address ---
        ld      hl,($C000)          ; L = low byte, H = high byte
        ld      a,l
        out     ($BF),a             ; address low byte
        ld      a,h
        or      $40                 ; command: VRAM write
        out     ($BF),a             ; address high byte + command

        ; --- copy 768 bytes: 6 x 128 unrolled OUTI ---
        ld      hl,$C100            ; source
        ld      c,$BE               ; VDP data port
        ld      d,6                 ; 6 blocks of 128 bytes
.loop:
        REPT 128
        outi
        ENDR
        dec     d
        jp      nz,.loop
        ret
