```asm
utoa16:
        LDX #0                  ; table index
        LDY #0                  ; output index
@digit:
        LDA #'0'
        STA $04                 ; current digit char
@sub:
        LDA $00
        SEC
        SBC utoa16_lo,X
        STA $05                 ; tentative low result
        LDA $01
        SBC utoa16_hi,X
        BCC @done               ; went negative: digit finished
        STA $01
        LDA $05
        STA $00
        INC $04
        JMP @sub
@done:
        LDA $04
        STA ($02),Y
        INY
        INX
        CPX #5
        BNE @digit
        RTS

utoa16_lo:
        .byte <10000, <1000, <100, <10, <1
utoa16_hi:
        .byte >10000, >1000, >100, >10, >1
```

Worst-case cycles: 1895