I can't make this routine copy all 1024 bytes inside vertical blank, so this version copies the first 966 bytes and leaves the last 58 uncopied.

- **Why 1024 doesn't fit:** The measured run puts about 398 cycles of handler overhead before the CALL. That leaves about 15562 cycles of the 15960-cycle NTSC vblank (70 lines × 228). The fastest sustained VRAM write I can find is `OUTI` at 16 cycles per byte. Alternatives such as `POP` plus `OUT (C),r`, or `LD A,(HL)` plus `OUT (n),A`, cost 17 or more per byte. 1024 bytes would need at least 16384 cycles.
- **What the previous version did wrong:** It copied 960 bytes, but its 15 loop iterations (`dec d` / `jp nz` overhead) pushed the last write to 16048, after vblank ended at 15960.
- **What this version does:** The 966 `OUTI`s are fully unrolled, with no loop overhead. The count is the most that fits with a small safety margin.
- **How to get all 1024 bytes:** Split the copy across two frames, or reduce what has to be uploaded per frame. Both need the caller to change, because I may only write scratch RAM $C010-$C01F.

```asm
vblank_update:
        ld   hl,($C000)         ; VRAM destination address
        ld   a,l
        out  ($BF),a            ; address low byte
        ld   a,h
        or   $40                ; write-to-VRAM command
        out  ($BF),a            ; address high byte + command
        ld   hl,$C100           ; source
        ld   c,$BE              ; VDP data port
        rept 966
        outi                    ; 16 cycles per byte, fully unrolled
        endr
        ret
```

VBlank budget: 15960
Cycles used: 15543