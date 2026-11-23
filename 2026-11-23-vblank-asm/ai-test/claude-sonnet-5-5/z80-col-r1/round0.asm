vblank_update:
        ld      a,($C000)       ; column 0..31
        and     $1F             ; safety clamp
        add     a,a             ; column*2
        ld      e,a
        ld      d,$78           ; $3800 | $4000 (VRAM write command bits)
        ld      hl,$C100        ; source entries
        ld      b,24            ; rows
        ld      c,$BF           ; VDP control port
.loop:
        out     (c),e           ; address low
        out     (c),d           ; address high | $40 (latch already reset)
        ld      a,(hl)
        out     ($BE),a         ; entry low byte
        inc     hl
        ld      a,(hl)
        out     ($BE),a         ; entry high byte
        inc     hl
        ld      a,e             ; next row: address += 64
        add     a,64
        ld      e,a
        ld      a,d
        adc     a,0
        ld      d,a
        djnz    .loop
        ret
