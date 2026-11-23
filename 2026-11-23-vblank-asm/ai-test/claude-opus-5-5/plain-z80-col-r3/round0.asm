; vblank_update
; Copies 24 name table entries from RAM $C100-$C12F into one column of the
; name table at VRAM $3800. The column number (0-31) is read from $C000.
; Entry i is written to VRAM $3800 + i*64 + column*2.
;
; This runs inside VBlank, so the VDP accepts writes without the ~29-cycle
; spacing needed during active display. The whole loop takes about 2.3k
; cycles, far less than NTSC VBlank (about 15k cycles).
; The interrupt handler already reset the control port latch by reading $BF.
; Uses no RAM besides the stack (not even the scratch area).

vblank_update:
        ld      a,($C000)
        and     31              ; clamp the column to 0-31
        add     a,a             ; column * 2 (each entry is 2 bytes)
        ld      e,a
        ld      d,$78           ; $3800 | $4000 (VRAM write command)
        ld      hl,$C100        ; source entries
        ld      b,24            ; 24 rows
.loop:
        ld      a,e
        out     ($BF),a         ; address low byte
        ld      a,d
        out     ($BF),a         ; address high byte + write command
        ld      a,(hl)
        out     ($BE),a         ; entry low byte
        inc     hl
        ld      a,(hl)
        out     ($BE),a         ; entry high byte
        inc     hl
        ld      a,e             ; next row: address += 64
        add     a,64
        ld      e,a
        jr      nc,.nocarry
        inc     d
.nocarry:
        djnz    .loop
        ret
