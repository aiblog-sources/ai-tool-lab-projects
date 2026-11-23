```asm
; vblank_update
; Copies 24 name table entries ($C100-$C12F, 2 bytes each, low byte first)
; down the column given by ($C000), into VRAM $3800 + i*64 + column*2.
; Called from the VBlank handler. The VDP control latch is already reset.
; Only VRAM writes are made; no VDP registers are changed.
; During vertical blank the VDP accepts back-to-back port writes,
; so OUTI is used for the data bytes.

vblank_update:
        ld      a,($C000)       ; 13  column 0..31
        and     31              ;  7  keep it in range
        add     a,a             ;  4  column*2
        ld      e,a             ;  4  E = low byte of VRAM address
        ld      d,$78           ;  7  D = high byte ($38 | $40 = write command)
        ld      hl,$C100        ; 10  source pointer
        ld      bc,$30BE        ; 10  B = 48 bytes to send, C = VDP data port

.loop:
        ld      a,e             ;  4
        out     ($BF),a         ; 11  address low byte
        ld      a,d             ;  4
        out     ($BF),a         ; 11  address high byte + write command
        ld      a,e             ;  4  next row: address += 64
        add     a,64            ;  7
        ld      e,a             ;  4
        jr      nc,.nocarry     ; 12/7
        inc     d               ;  4
.nocarry:
        outi                    ; 16  entry low byte
        outi                    ; 16  entry high byte (Z set when B = 0)
        jr      nz,.loop        ; 12/7
        ret
```

VBlank budget: 15960
Cycles used: 2478