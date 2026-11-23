```asm
vblank_update:
        ld      a,($C000)
        and     $1F                 ; keep column in 0..31
        add     a,a                 ; column*2
        ld      e,a
        ld      d,$38               ; DE = $3800 + column*2
        ld      hl,$C100
        ld      b,24
.row:
        ld      a,e
        out     ($BF),a             ; VRAM address low byte
        ld      a,d
        or      $40                 ; code 01 = VRAM write
        out     ($BF),a             ; VRAM address high byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a             ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a             ; entry high byte
        ld      a,e                 ; DE += 64 (next row)
        add     a,64
        ld      e,a
        ld      a,d
        adc     a,0
        ld      d,a
        djnz    .row
        ret
```

The routine rewrites the VRAM address for each row, since consecutive rows are 64 bytes apart. It masks the column to 0–31 for safety, and the highest address it writes is $3DBF, which is inside the name table.