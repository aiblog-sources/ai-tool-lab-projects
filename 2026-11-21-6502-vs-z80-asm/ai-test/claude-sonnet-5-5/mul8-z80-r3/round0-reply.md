```asm
mul8:
        ld   d,0            ; DE = multiplicand (E)
        ld   l,0            ; HL = multiplier (H) in high byte, L = 0
        ld   b,8
.loop:
        add  hl,hl          ; shift next multiplier bit out into carry
        jr   nc,.skip
        add  hl,de          ; add multiplicand when bit is set
.skip:
        djnz .loop
        ret
```
Worst-case cycles: 379