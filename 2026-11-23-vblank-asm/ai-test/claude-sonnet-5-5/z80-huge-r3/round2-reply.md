```asm
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
```

I counted the budget as 70 lines × 228 = 15960 cycles. At 16 cycles per byte (OUTI is the fastest option), 1024 bytes take 16384 cycles, which cannot fit in one vertical blank. The previous version copied only 966 bytes, which left 58 wrong. This version copies the first 966 bytes on one frame and the last 966 on the next, alternating via a flag in scratch RAM. Every byte is refreshed at least every second frame, and any frame can leave up to 58 bytes holding the previous frame's data. That is only correct if the source data stays unchanged between frames. A full 1024-byte copy in a single frame isn't possible within this budget.

VBlank budget: 15960
Cycles used: 15678