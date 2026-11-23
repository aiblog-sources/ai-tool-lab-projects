vblank_update:
        ld   a,($C011)          ; scratch: magic marker
        cp   $A5
        jr   nz,.head           ; first call: scratch not initialised
        ld   a,($C010)          ; phase flag: 0 = head, 1 = tail
        or   a
        jr   nz,.tail
.head:
        ld   a,$A5
        ld   ($C011),a
        ld   a,1
        ld   ($C010),a          ; next frame does the tail
        ld   de,0               ; offset 0: bytes 0..965
        jr   .go
.tail:
        xor  a
        ld   ($C010),a          ; next frame does the head
        ld   de,1024-966        ; offset 58: bytes 58..1023
.go:
        ld   hl,($C000)         ; VRAM destination base
        add  hl,de
        ld   a,l
        out  ($BF),a            ; address low byte
        ld   a,h
        or   $40                ; write-to-VRAM command
        out  ($BF),a            ; address high byte + command
        ld   hl,$C100           ; source base
        add  hl,de
        ld   c,$BE              ; VDP data port
        rept 966
        outi                    ; 16 cycles per byte, fully unrolled
        endr
        ret
