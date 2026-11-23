```asm
vblank_update:
        ld      a,($C000)           ; column 0..31
        and     $1F                 ; safety clamp
        add     a,a                 ; column*2
        ld      e,a                 ; DE = $3800 + column*2 (row 0)
        ld      d,$38
        ld      hl,$C100            ; source entries
        ld      b,24                ; 24 rows
        ld      c,$BF               ; VDP control port
.loop:
        out     (c),e               ; address low byte
        ld      a,d
        or      $40                 ; VRAM write command
        out     (c),a               ; address high byte + command
        ld      a,(hl)
        inc     hl
        out     ($BE),a             ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a             ; entry high byte
        ld      a,e                 ; next row: address += 64
        add     a,64
        ld      e,a
        ld      a,d
        adc     a,0
        ld      d,a
        djnz    .loop
        ret
```

VBlank budget: 15960
Cycles used: 3057