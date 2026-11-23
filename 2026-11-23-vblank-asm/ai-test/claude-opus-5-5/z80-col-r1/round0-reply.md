```asm
; vblank_update
; Copies the 24 name table entries at $C100-$C12F down the column given in ($C000).
; Entry i goes to VRAM $3800 + i*64 + column*2.
; Destroys: AF, B, DE, HL.

vblank_update:
        ld      a,($C000)       ; column 0..31
        and     31              ; keep it in range
        add     a,a             ; column*2
        ld      e,a             ; DE = VRAM write command: $3800 + column*2, with bit 14 set
        ld      d,$78           ; $38 | $40 (write to VRAM)
        ld      hl,$C100        ; source entries
        ld      b,24            ; 24 rows

.loop:
        ld      a,e
        out     ($BF),a         ; address low byte
        ld      a,d
        out     ($BF),a         ; address high byte | $40
        ld      a,(hl)
        out     ($BE),a         ; entry low byte
        inc     hl
        ld      a,(hl)
        out     ($BE),a         ; entry high byte
        inc     hl

        ld      a,e             ; next row: DE += 64
        add     a,64
        ld      e,a
        jr      nc,.no_carry
        inc     d
.no_carry:
        djnz    .loop
        ret
```

VBlank budget: 15960
Cycles used: 2850