; Routines with cycle costs fixed by hand (MIT). No branches, so no
; page-crossing penalties: every cost below is exact on a 6502.
;   _burn400:   200 NOPs (2 cycles each) + RTS (6)        = 406 cycles self
;   _burn_pair: JSR _burn400 twice (6 each), 10 NOPs, RTS = 38 self, 850 inclusive
        .export _burn400, _burn_pair
.segment "CODE"
_burn400:
        .repeat 200
        nop
        .endrepeat
        rts
_burn_pair:
        jsr _burn400
        jsr _burn400
        .repeat 10
        nop
        .endrepeat
        rts
