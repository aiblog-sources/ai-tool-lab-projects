; vblank_update
; Copies 24 name table entries from $C100-$C12F down one column of the
; name table at VRAM $3800. The column number is read from $C000.
; Entry i goes to VRAM $3800 + i*64 + column*2.
; The caller has already read $BF, so the control port latch is reset.

vblank_update:
        ld a,($C000)            ; 13  column, 0..31
        and 31                  ; 7   keep it in range
        add a,a                 ; 4   column*2 = 0..62
        ld e,a                  ; 4   E = low byte of VRAM address
        ld d,$38|$40            ; 7   D = high byte with the write flag ($78)
        ld hl,$C100             ; 10  source buffer
        ld bc,(24<<8)|$BF       ; 10  B = 24 rows, C = VDP control port
.loop:
        out (c),e               ; 12  address low byte
        out (c),d               ; 12  address high byte, write mode
        ld a,(hl)               ; 7
        out ($BE),a             ; 11  entry low byte
        inc hl                  ; 6
        ld a,(hl)               ; 7
        out ($BE),a             ; 11  entry high byte
        inc hl                  ; 6
        ld a,e                  ; 4   next row: address + 64
        add a,64                ; 7
        ld e,a                  ; 4
        jr nc,.nocarry          ; 12/7
        inc d                   ; 4   highest D used is $7D, so bit 6 stays set
.nocarry:
        djnz .loop              ; 13/8
        ret
