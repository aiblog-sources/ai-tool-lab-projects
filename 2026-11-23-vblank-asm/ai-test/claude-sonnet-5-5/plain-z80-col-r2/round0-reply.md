```asm
vblank_update:
        ld      a,(#C000)
        and     $1F             ; safety: column 0..31
        add     a,a             ; column*2
        ld      e,a
        ld      d,$78           ; $3800 | $4000 (VRAM write command)
        ld      hl,$C100
        ld      b,24
.row:
        ld      a,e
        out     ($BF),a         ; address low
        ld      a,d
        out     ($BF),a         ; address high + write command
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry high byte
        ld      a,e             ; next row: +64
        add     a,64
        ld      e,a
        ld      a,d
        adc     a,0
        ld      d,a
        djnz    .row
        ret
```

Oops, I wrote `#C000` in the first load, which sjasmplus may not accept in every configuration. Here is the corrected version using the `$` prefix consistently:

```asm
vblank_update:
        ld      a,($C000)
        and     $1F             ; safety: column 0..31
        add     a,a             ; column*2
        ld      e,a
        ld      d,$78           ; $3800 | $4000 (VRAM write command)
        ld      hl,$C100
        ld      b,24
.row:
        ld      a,e
        out     ($BF),a         ; address low
        ld      a,d
        out     ($BF),a         ; address high + write command
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry low byte
        ld      a,(hl)
        inc     hl
        out     ($BE),a         ; entry high byte
        ld      a,e             ; next row: +64
        add     a,64
        ld      e,a
        ld      a,d
        adc     a,0
        ld      d,a
        djnz    .row
        ret
```