; vblank_update
; Copies the 24 name table entries at $C100-$C12F down one screen column.
; The column number (0-31) is read from $C000.
; Entry i is written to VRAM $3800 + i*64 + column*2.
; It changes A, B, D, E, H, L and the flags. It writes no RAM.

vblank_update:
        ld      a,($C000)
        and     $1F                     ; keep the column in 0-31
        add     a,a                     ; column * 2
        ld      e,a
        ld      d,$78                   ; DE = $3800 | $4000 (VRAM write) + column*2
        ld      hl,$C100                ; source entries
        ld      b,24                    ; 24 rows
.loop:
        ld      a,e
        out     ($BF),a                 ; low byte of the VRAM address
        ld      a,d
        out     ($BF),a                 ; high byte, plus the write command
        ld      a,(hl)
        out     ($BE),a                 ; low byte of the entry
        inc     hl
        nop                             ; keeps data writes at least 26 cycles apart
        ld      a,(hl)
        out     ($BE),a                 ; high byte of the entry
        inc     hl
        ld      a,e                     ; next row: address + 64
        add     a,64
        ld      e,a
        jr      nc,.no_carry
        inc     d
.no_carry:
        djnz    .loop
        ret
