; vblank_update
; Copies 64 bytes at $C100-$C13F into name table row (C000) at $3800 + row*64.
; Runs inside vertical blank, so VDP data writes can go at full speed (OUTI back-to-back).
; The control port latch was already reset by the handler's read of $BF.

vblank_update:
        ld   a,($C000)          ; 13  row 0..23
        ld   b,a                ;  4
        rrca                    ;  4  bits 0-1 -> bits 6-7
        rrca                    ;  4
        and  $C0                ;  7  low byte of row*64
        out  ($BF),a            ; 11  address low byte
        ld   a,b                ;  4
        rrca                    ;  4
        rrca                    ;  4  bits 2-4 -> bits 0-2
        and  $07                ;  7  high byte of row*64 (0..5)
        or   $78                ;  7  $38 base | $40 VRAM-write command
        out  ($BF),a            ; 11  address high byte + write command
        ld   hl,$C100           ; 10
        ld   c,$BE              ;  7  VDP data port
        REPT 64
        outi                    ; 16 each
        ENDR
        ret
