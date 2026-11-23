```asm
vblank_update:
        ld   a,(0xC000)         ; column 0..31
        and  31                 ; safety clamp
        add  a,a                ; column*2
        ld   l,a
        ld   h,0x78             ; $3800 | $4000 (VRAM write command)
        ld   de,0xC100          ; source entries
        ld   bc,64              ; row stride in VRAM
.row:
        ld   a,l
        out  (0xBF),a           ; address low
        ld   a,h
        out  (0xBF),a           ; address high + write command
        ld   a,(de)
        out  (0xBE),a           ; entry low byte
        inc  de
        ld   a,(de)
        out  (0xBE),a           ; entry high byte
        inc  de
        add  hl,bc              ; next row (+64)
        ld   a,e
        cp   0x30               ; DE reached $C130 -> 24 entries done
        jr   nz,.row
        ret
```

VBlank budget: 15960
Cycles used: 2720