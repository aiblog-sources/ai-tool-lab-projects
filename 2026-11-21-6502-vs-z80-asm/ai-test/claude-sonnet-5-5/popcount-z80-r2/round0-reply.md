```asm
popcount:
        ld   b,a
        rrca
        and  $55
        ld   c,a
        ld   a,b
        sub  c              ; 2-bit pair counts
        ld   b,a
        and  $33
        ld   c,a
        ld   a,b
        rrca
        rrca
        and  $33
        add  a,c            ; 4-bit nibble counts
        ld   b,a
        rrca
        rrca
        rrca
        rrca
        add  a,b
        and  $0F
        ret
```
Worst-case cycles: 123