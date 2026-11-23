```asm
vblank_update:
        ld   a,(0xC000)         ; VRAM address low
        out  (0xBF),a
        ld   a,(0xC001)         ; VRAM address high ($00-$35)
        or   0x40               ; set write-VRAM command bits
        out  (0xBF),a
        ld   hl,0xC100
        ld   bc,0x00BE          ; B=0 (256 bytes per page), C=data port
        ld   d,3                ; 3 pages = 768 bytes
.page:
.loop:
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
        jp   nz,.loop           ; OUTI sets Z when B hits 0 (after 256 bytes)
        dec  d
        jp   nz,.page
        ret
```

VBlank budget: 15960
Cycles used: 12909