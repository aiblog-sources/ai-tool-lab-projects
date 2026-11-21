; sort_u8 - counting sort of B unsigned bytes at HL, ascending, in place.
; It builds a 256-byte histogram on the stack, in the page-aligned block just
; below SP. Uses about 256-511 bytes of stack. No scratch RAM is used.

sort_u8:
        ld      a,b
        or      a
        ret     z                   ; length 0: nothing to do

        ex      de,hl               ; DE = array pointer
        ld      iyh,d               ; IY = array start (kept for output)
        ld      iyl,e
        ld      ix,0
        add     ix,sp               ; IX = original SP

        ld      hl,0
        add     hl,sp
        ld      l,0                 ; HL = page boundary at or below SP
        ld      sp,hl               ; pushes fill page H-1 (below SP)
        ld      hl,0
        ld      c,8
.clr:
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        push    hl
        dec     c
        jr      nz,.clr

        ld      hl,0
        add     hl,sp               ; HL = table base (page aligned, L=0)
        ld      sp,ix               ; restore SP (stack not used below)

.cnt:                               ; histogram
        ld      a,(de)
        ld      l,a
        inc     (hl)
        inc     de
        djnz    .cnt

        ld      e,iyl               ; DE = array start
        ld      d,iyh
        ld      l,0

.s0:    ld      a,(hl)              ; scan buckets, unrolled x4
        or      a
        jr      nz,.e0
        inc     l
.s1:    ld      a,(hl)
        or      a
        jr      nz,.e1
        inc     l
.s2:    ld      a,(hl)
        or      a
        jr      nz,.e2
        inc     l
.s3:    ld      a,(hl)
        or      a
        jr      nz,.e3
        inc     l
        jr      nz,.s0
        ret

.e0:    ld      b,a
        ld      a,l
.w0:    ld      (de),a
        inc     de
        djnz    .w0
        inc     l
        jr      .s1

.e1:    ld      b,a
        ld      a,l
.w1:    ld      (de),a
        inc     de
        djnz    .w1
        inc     l
        jr      .s2

.e2:    ld      b,a
        ld      a,l
.w2:    ld      (de),a
        inc     de
        djnz    .w2
        inc     l
        jr      .s3

.e3:    ld      b,a
        ld      a,l
.w3:    ld      (de),a
        inc     de
        djnz    .w3
        inc     l
        jr      nz,.s0
        ret
