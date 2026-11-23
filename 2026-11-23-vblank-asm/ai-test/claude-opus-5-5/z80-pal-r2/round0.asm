; vblank_update
; Copies 32 bytes from RAM $C100-$C11F to CRAM entries 0-31.
; The caller has already read port $BF, so the VDP control-port latch is reset.

vblank_update:
        ld   hl,$C100          ; 10  source palette in RAM
        xor  a                 ;  4  CRAM address low byte = $00
        out  ($BF),a           ; 11
        ld   a,$C0             ;  7  high byte: code 3 (CRAM write), address 0
        out  ($BF),a           ; 11
        ld   c,$BE             ;  7  VDP data port
        ; 32 unrolled OUTI. During vblank the VDP takes back-to-back writes.
        outi                   ; 16 each
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        ret                    ; 10
