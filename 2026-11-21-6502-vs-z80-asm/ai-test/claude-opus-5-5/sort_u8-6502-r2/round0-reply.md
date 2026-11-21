```asm
; ---------------------------------------------------------------------------
; sort_u8: in-place ascending heapsort of unsigned bytes
;   in : ($00/$01) = pointer to array, X = length (0..255)
;   out: array sorted in place
;   zp : $02/$03 = ptr-1 (1-based base), $04 = heap size, $05 = sift value,
;        $06 = hole index, $07 = temp, $08 = build counter
; ---------------------------------------------------------------------------

sort_u8_pm1   = $02         ; pointer - 1 (so a[1..n] = (pm1),Y)
sort_u8_n     = $04         ; current heap size
sort_u8_val   = $05         ; value being sifted
sort_u8_hole  = $06         ; current hole index
sort_u8_tmp   = $07
sort_u8_bi    = $08         ; build-phase index

sort_u8:
        cpx #2
        bcc @ret                    ; 0 or 1 element: already sorted
        stx sort_u8_n

        lda $00                     ; pm1 = ptr - 1
        sec
        sbc #1
        sta sort_u8_pm1
        lda $01
        sbc #0
        sta sort_u8_pm1+1

        ; ---- build max-heap: i = n/2 down to 1 ----
        txa
        lsr a
        sta sort_u8_bi
@build:
        ldy sort_u8_bi
        lda (sort_u8_pm1),y
        sta sort_u8_val
        jsr sort_u8_sift
        dec sort_u8_bi
        bne @build

        ; ---- extraction: m = n down to 2 ----
@sortloop:
        ldy sort_u8_n
        lda (sort_u8_pm1),y         ; val = a[m]
        sta sort_u8_val
        ldy #1
        lda (sort_u8_pm1),y         ; a[m] = a[1] (current max)
        ldy sort_u8_n
        sta (sort_u8_pm1),y
        dec sort_u8_n               ; heap size m-1
        ldy #1
        jsr sort_u8_sift            ; sift val down from root
        lda sort_u8_n
        cmp #2
        bcs @sortloop
@ret:
        rts

; ---------------------------------------------------------------------------
; sort_u8_sift: Y = hole index, val = value to place, n = heap size
; ---------------------------------------------------------------------------
sort_u8_sift:
@sift:
        sty sort_u8_hole
        tya
        asl a                       ; c = 2*hole
        bcs @done                   ; c >= 256 > n: no children
        cmp sort_u8_n
        bcs @last                   ; c >= n
        tay                         ; two children: c and c+1
        lda (sort_u8_pm1),y         ; a[c]
        iny
        cmp (sort_u8_pm1),y         ; a[c] vs a[c+1]
        bcs @takec
        lda (sort_u8_pm1),y         ; a[c+1] larger, Y = c+1
        bcc @have                   ; always
@takec:
        dey                         ; Y = c, A = a[c]
@have:
        cmp sort_u8_val
        bcc @done                   ; child < val: stop
        sty sort_u8_tmp
        ldy sort_u8_hole
        sta (sort_u8_pm1),y         ; a[hole] = a[child]
        ldy sort_u8_tmp             ; hole = child (nonzero)
        bne @sift                   ; always

@last:
        bne @done                   ; c > n: no children
        tay                         ; c == n: single child
        lda (sort_u8_pm1),y
        cmp sort_u8_val
        bcc @done
        sty sort_u8_tmp
        ldy sort_u8_hole
        sta (sort_u8_pm1),y         ; a[hole] = a[c]
        ldy sort_u8_tmp             ; c is a leaf: place val there
        lda sort_u8_val
        sta (sort_u8_pm1),y
        rts

@done:
        ldy sort_u8_hole
        lda sort_u8_val
        sta (sort_u8_pm1),y
        rts
```

Worst-case cycles: 143600