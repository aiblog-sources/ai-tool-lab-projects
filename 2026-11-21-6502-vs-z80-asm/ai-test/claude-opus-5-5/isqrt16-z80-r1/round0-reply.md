```asm
; isqrt16: A = floor(sqrt(HL)), HL unsigned 16-bit.
;
; Method:
;   1. Look up the top 4 root bits from the high byte of n. Two 256-byte
;      tables give r4 = isqrt(H) and T4 = H - r4*r4.
;   2. Get the low 4 root bits with a non-restoring recurrence. Each step
;      computes S = 4*X + bb - 1, where bb is the next two bits of n:
;        after a 1-bit (state P): X = R - r'      with r' = 2r + 1
;        after a 0-bit (state N): X = R + r' + 1  with r' = 2r
;      The next root bit is 1 when S >= 0.
;   3. Steps 5 and 6 fit in signed 8 bits. Steps 7 and 8 use rearranged
;      sign tests so that 8-bit arithmetic is still exact.
;   The two states are unrolled as separate code paths, so no restore step
;   is needed.

isqrt16:
        ld      e,l                     ; 4   keep n low byte
        ld      l,h                     ; 4   index = n high byte
        ld      h,isqrt16_root>>8       ; 7
        ld      d,(hl)                  ; 7   D = r4
        inc     h                       ; 4
        ld      a,(hl)                  ; 7   A = T4 (remainder)
        ld      h,e                     ; 4   H = n low byte (bits to shift in)

        ; ---- step 5 (state P, D already = r4)
        sub     d                       ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        dec     a                       ; 4
        jp      m,.n6                   ; 10

        ; ---- step 6, state P
        scf                             ; 4
        rl      d                       ; 8   D = 2D+1
        sub     d                       ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        dec     a                       ; 4
        jp      m,.n7                   ; 10

.p7:    ; ---- step 7, state P
        scf                             ; 4
        rl      d                       ; 8   D = r6
        sub     d                       ; 4   A = X
        add     hl,hl                   ; 11
        rla                             ; 4   A = W = 2X+b1
        ld      e,a                     ; 4
        add     hl,hl                   ; 11  carry = b0
        adc     a,$FF                   ; 7   A = W+b0-1, sign = !b7
        jp      m,.n8                   ; 10

.p8:    ; ---- step 8, state P
        add     a,e                     ; 4   A = S7 (mod 256)
        scf                             ; 4
        rl      d                       ; 8   D = r7, carry = 0
        ld      c,a                     ; 4
        ld      a,h                     ; 4
        cp      $40                     ; 7   carry = (last two bits == 0)
        ld      a,c                     ; 4
        sbc     a,d                     ; 4   sign = !b8
        rla                             ; 4
        ccf                             ; 4   carry = b8
        ld      a,d                     ; 4
        adc     a,a                     ; 4   A = 2*r7 + b8
        ret                             ; 10

.n6:    ; ---- step 6, state N
        sla     d                       ; 8   D = 2D
        scf                             ; 4
        adc     a,d                     ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        add     hl,hl                   ; 11
        rla                             ; 4
        dec     a                       ; 4
        jp      p,.p7                   ; 10

.n7:    ; ---- step 7, state N
        sla     d                       ; 8   D = r6
        scf                             ; 4
        adc     a,d                     ; 4   A = X
        add     hl,hl                   ; 11
        rla                             ; 4
        ld      e,a                     ; 4
        add     hl,hl                   ; 11
        adc     a,$FF                   ; 7
        jp      p,.p8                   ; 10

.n8:    ; ---- step 8, state N
        add     a,e                     ; 4   A = S7 (mod 256)
        sla     d                       ; 8   D = r7
        ld      c,a                     ; 4
        ld      a,h                     ; 4
        add     a,$C0                   ; 7   carry = (last two bits != 0)
        ld      a,c                     ; 4
        adc     a,d                     ; 4   sign = !b8
        rla                             ; 4
        ccf                             ; 4
        ld      a,d                     ; 4
        adc     a,a                     ; 4
        ret                             ; 10

        ALIGN   256
isqrt16_root:                           ; isqrt(i), i = 0..255
        DS      1,0
        DS      3,1
        DS      5,2
        DS      7,3
        DS      9,4
        DS      11,5
        DS      13,6
        DS      15,7
        DS      17,8
        DS      19,9
        DS      21,10
        DS      23,11
        DS      25,12
        DS      27,13
        DS      29,14
        DS      31,15
isqrt16_rem:                            ; i - isqrt(i)^2  (must be root+256)
        DB      0
        DB      0,1,2
        DB      0,1,2,3,4
        DB      0,1,2,3,4,5,6
        DB      0,1,2,3,4,5,6,7,8
        DB      0,1,2,3,4,5,6,7,8,9,10
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
        DB      0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
```

The tables use `ALIGN 256`, which can add up to 255 bytes of padding. The routine needs 512 bytes of tables and uses no RAM besides the CALL's return address.

I have not assembled or run this. I checked it by hand only for n = 0, 1, 3, 4, 8, 9 and $FFFF.

Worst-case cycles: 290