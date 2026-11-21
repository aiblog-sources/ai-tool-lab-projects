```asm
; popcount: A = number of set bits in A (0..8)
; Uses a 256-byte page-aligned lookup table. Clobbers HL.
popcount:
        ld      h,popcount_table >> 8   ; 7  high byte of table page
        ld      l,a                     ; 4  index = A
        ld      a,(hl)                  ; 7  A = popcount
        ret                             ; 10

        ALIGN   256
popcount_table:
        ; row 0x0_
        db 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4
        ; row 0x1_
        db 1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
        ; row 0x2_
        db 1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
        ; row 0x3_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0x4_
        db 1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
        ; row 0x5_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0x6_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0x7_
        db 3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7
        ; row 0x8_
        db 1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
        ; row 0x9_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0xA_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0xB_
        db 3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7
        ; row 0xC_
        db 2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
        ; row 0xD_
        db 3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7
        ; row 0xE_
        db 3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7
        ; row 0xF_
        db 4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
```
Worst-case cycles: 45