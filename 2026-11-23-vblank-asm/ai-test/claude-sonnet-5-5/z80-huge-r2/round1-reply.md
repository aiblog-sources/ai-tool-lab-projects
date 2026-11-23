```asm
vblank_update:
        ld      a,($C010)               ; flip a phase bit kept in scratch RAM
        xor     1
        ld      ($C010),a
        ld      hl,($C000)              ; VRAM destination
        ld      de,$C100                ; source
        rrca                            ; carry = phase bit
        jr      nc,.go
        ld      bc,64                   ; odd frames: copy bytes 64..1023
        add     hl,bc
        ld      de,$C140
.go:
        ld      a,l
        out     ($BF),a                 ; address low
        ld      a,h
        or      $40                     ; VRAM write command
        out     ($BF),a                 ; address high
        ex      de,hl                   ; HL = source
        ld      c,$BE                   ; VDP data port
        REPT    960
        outi
        ENDR
        ret
```

The 1024-byte copy cannot fit in one NTSC vblank: 1024 × 16 cycles (OUTI) = 16384, which is more than the roughly 15960 available. The previous answer's 960 `outi`s were right for the time limit, but they always left the last 64 bytes uncopied. This version copies 960 bytes per frame and alternates the window between bytes 0–959 and bytes 64–1023. Two consecutive frames therefore cover all 1024 bytes, as long as the source and destination stay unchanged. After only one frame, 64 bytes at one end of the destination can still be stale.

VBlank budget: 15960 (70 lines × 228 cycles)
Cycles used: 15526 (worst path)