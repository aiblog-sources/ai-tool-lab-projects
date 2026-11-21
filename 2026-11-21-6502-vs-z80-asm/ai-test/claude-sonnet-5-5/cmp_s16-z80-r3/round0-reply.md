```asm
cmp_s16:
        ld   a,h
        xor  d              ; bit 7 set if signs differ; also clears carry
        jp   m,.diff
        sbc  hl,de          ; same signs: no overflow, carry = (a < b)
        ret
.diff:
        ld   a,h
        rla                 ; signs differ: a < b iff a is negative, carry = bit 7 of H
        ret
```
Worst-case cycles: 60