/* main.c - 20-segment snake, cc65 / NES, mapper 0 */
#include "nes.h"

#define SEGS   20
#define DELAY  4              /* frames between segments */
#define HMASK  127            /* history ring buffer, 128 entries */

static const unsigned char palette[32] = {
    /* background (unused, blank) */
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    /* sprites: 0 = body (green), 1 = head (yellow/red) */
    0x0F,0x1A,0x2A,0x30, 0x0F,0x28,0x16,0x30,
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30
};

static unsigned char hx[HMASK + 1];
static unsigned char hy[HMASK + 1];
static unsigned char h;       /* index of newest history entry (the head) */
static unsigned char x, y;
static signed char   dx, dy;  /* current heading */
static unsigned char pad;
static unsigned char rot;
static unsigned char i, k, seg, idx;

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    /* start in the middle row, heading right; pre-fill the history with the
       straight trail behind the head so the body is laid out from frame 0 */
    x = 100;
    y = 112;
    dx = 1;
    dy = 0;
    for (i = 0; i < (HMASK + 1); ++i) {
        hx[i] = x;            /* entries older than the trail just repeat the tail */
        hy[i] = y;
    }
    h = 0;
    for (i = 0; i <= (SEGS - 1) * DELAY; ++i) {
        /* i frames ago the head was i pixels further left */
        hx[(h - i) & HMASK] = x - i;
        hy[(h - i) & HMASK] = y;
    }

    rot = 0;

    for (;;) {
        /* ---- update ---- */
        pad = pad_read();
        if (pad & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN)) {
            dx = 0;
            dy = 0;
            if (pad & PAD_LEFT)  dx = -1;
            if (pad & PAD_RIGHT) dx = 1;
            if (pad & PAD_UP)    dy = -1;
            if (pad & PAD_DOWN)  dy = 1;
        }
        /* otherwise keep the last heading: the snake never stops */

        if (dx < 0) { if (x > 0)   --x; }
        else if (dx > 0) { if (x < 248) ++x; }
        if (dy < 0) { if (y > 8)   --y; }
        else if (dy > 0) { if (y < 222) ++y; }

        h = (h + 1) & HMASK;
        hx[h] = x;
        hy[h] = y;

        /* ---- fill OAM ----
           The PPU shows at most 8 sprites per scanline, and a straight
           horizontal snake puts all 20 on the same lines. Rotating which
           segment gets which OAM slot spreads the dropped ones over all
           segments; neighbours overlap (1 px/frame * 4 frames = 4 px
           spacing), so the gaps are small and move around. */
        for (k = 0; k < SEGS; ++k) {
            seg = k + rot;
            if (seg >= SEGS) seg -= SEGS;
            idx = (h - seg * DELAY) & HMASK;
            OAM_Y(k)    = hy[idx];
            OAM_TILE(k) = 0x04;
            OAM_ATTR(k) = (seg == 0) ? 1 : 0;
            OAM_X(k)    = hx[idx];
        }
        rot += 7;
        if (rot >= SEGS) rot -= SEGS;

        oam_ready();
        wait_frame();
    }
}
