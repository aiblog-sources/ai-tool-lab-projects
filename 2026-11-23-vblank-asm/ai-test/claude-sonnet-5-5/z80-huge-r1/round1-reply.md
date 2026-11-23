I could not make this copy all 1024 bytes, because it can't fit in one NTSC VBlank. This version copies 960 bytes and still leaves the last 64 bytes uncopied, so those tests will keep failing on that tail.

The fastest VDP data write on the Z80 is `OUTI` at 16 cycles per byte. I also checked `POP` + `OUT (C),r` and `LD A,(HL)` + `OUT (n),A`. Both cost at least 17 cycles per byte. So 1024 bytes need at least 16384 cycles, while NTSC VBlank is 70 lines × 228 = 15960 cycles. That is already short before counting the handler's own overhead. The only way to copy everything would be to write outside VBlank, which the rules forbid.

The routine now copies 960 bytes, in 15 unrolled blocks of 64 `OUTI`. That leaves about 300 cycles of VBlank for the handler's register saves and interrupt latency. The previous version copied 928 bytes and left about 600 cycles unused. Copying the last 64 bytes would need 1024 cycles more than the budget allows.

```asm
; vblank_update: copy the first 960 bytes of the buffer at $C100 to VRAM
; starting at the address held in ($C000). The last 64 bytes are NOT copied:
; 1024 bytes * 16 cycles (OUTI, the fastest possible VDP data write)
; = 16384 cycles, which exceeds the 15960-cycle NTSC VBlank.

vblank_update:
    ld hl,($C000)           ; VRAM destination address
    ld a,l
    out ($BF),a             ; address low byte
    ld a,h
    or $40                  ; VRAM write command
    out ($BF),a             ; address high byte + command
    ld hl,$C100             ; source
    ld c,$BE                ; VDP data port
    ld d,15                 ; 15 blocks * 64 bytes = 960 bytes
.vblank_update_blk:
    REPT 64
    outi
    ENDR
    dec d
    jp nz,.vblank_update_blk
    ret
```

VBlank budget: 15960
Cycles used: 15650