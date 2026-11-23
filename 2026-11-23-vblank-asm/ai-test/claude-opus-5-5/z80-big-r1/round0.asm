; vblank_update
; Copies 768 bytes from RAM $C100-$C3FF to VRAM, starting at the address
; stored at $C000/$C001 (low byte first, range $0000-$3500).
; Called from the VBlank handler. The control port latch is already reset.

vblank_update:
        ld      hl,($C000)      ; 16  VRAM destination address
        ld      a,l             ; 4
        out     ($BF),a         ; 11  address low byte
        ld      a,h             ; 4
        or      $40             ; 7   command bits 01 = VRAM write
        out     ($BF),a         ; 11  address high byte + command

        ld      hl,$C100        ; 10  source
        ld      c,$BE           ; 7   VDP data port
        ld      d,24            ; 7   24 blocks x 32 bytes = 768 bytes

.loop:
        DUP 32
        outi                    ; 16 each (B is changed but not used)
        EDUP
        dec     d               ; 4
        jp      nz,.loop        ; 10

        ret
