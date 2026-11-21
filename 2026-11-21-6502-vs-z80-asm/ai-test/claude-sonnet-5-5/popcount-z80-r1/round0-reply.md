```asm
popcount:
        ld b,a              ; SWAR popcount
        srl a
        and $55
        ld c,a
        ld a,b
        sub c               ; each 2-bit field = count of its bits
        ld b,a
        and $33
        ld c,a
        ld a,b
        rrca
        rrca
        and $33
        add a,c             ; each nibble = count of its bits
        ld b,a
        rrca
        rrca
        rrca
        rrca
        add a,b
        and $0F             ; A = total
        ret
```
Worst-case cycles: 127