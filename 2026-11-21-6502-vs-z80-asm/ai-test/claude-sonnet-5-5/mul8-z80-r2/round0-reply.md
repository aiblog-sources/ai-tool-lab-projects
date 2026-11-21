```asm
mul8:
        ld   l,0            ; 7
        ld   d,0            ; 7
        ld   b,8            ; 7
.loop:
        add  hl,hl          ; 11  shift multiplier's top bit into carry
        jr   nc,.skip       ; 7 not taken / 12 taken
        add  hl,de          ; 11
.skip:
        djnz .loop          ; 13 taken / 8 last
        ret                 ; 10
```

Worst-case cycles: 379