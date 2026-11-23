; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM, starting at the
; address stored (little-endian) at $C000/$C001.
; Called from the VBlank handler; the VDP control latch is already reset.
; Uses unrolled OUTI (16 cycles/byte) so the whole transfer
; (~12,400 cycles) completes inside the NTSC vertical blank.

vblank_update:
        ; Set VDP write address: low byte, then high byte | $40 (VRAM write)
        ld      hl,($C000)
        ld      a,l
        out     ($BF),a
        ld      a,h
        or      $40
        out     ($BF),a

        ld      hl,$C100        ; source
        ld      c,$BE           ; VDP data port
        ld      d,6             ; 6 blocks x 128 bytes = 768 bytes
.block:
        DUP 128
        outi
        EDUP
        dec     d
        jp      nz,.block
        ret
