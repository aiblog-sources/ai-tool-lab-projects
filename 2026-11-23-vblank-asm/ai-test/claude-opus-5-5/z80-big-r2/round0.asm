; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM, starting at the address
; stored at $C000/$C001 (low byte first).
; The handler has already reset the VDP control port latch by reading $BF.
; The loop is fully unrolled with OUTI (16 cycles per byte). That gives
; 768 * 16 = 12288 cycles for the copy, which fits inside NTSC vertical blank.

vblank_update:
        ld      hl,($C000)          ; 16  L = VRAM addr low, H = VRAM addr high
        ld      c,$BF               ;  7  VDP control port
        out     (c),l               ; 12  address bits 0-7
        ld      a,h                 ;  4
        or      $40                 ;  7  code 01 = VRAM write
        out     (c),a               ; 12  address bits 8-13 + command
        ld      hl,$C100            ; 10  source buffer
        ld      c,$BE               ;  7  VDP data port
        DUP     768
        outi                        ; 16 each: (C) <- (HL), HL++, B--
        EDUP
        ret
