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
static unsigned char slot;

/* write segment 'seg' into the next OAM slot */
static void put_seg(void)
{
    idx = (h - seg * DELAY) & HMASK;
    OAM_Y(slot)    = hy[idx];
    OAM_TILE(slot) = 0x04;
    OAM_ATTR(slot) = (seg == 0) ? 1 : 0;
    OAM_X(slot)    = hx[idx];
    ++slot;
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    x = 100;
    y = 112;
    dx = 1;
    dy = 0;
    for (i = 0; i < (HMASK + 1); ++i) {
        hx[i] = x;
        hy[i] = y;
    }
    h = 0;
    for (i = 0; i <= (SEGS - 1) * DELAY; ++i) {
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

        if (dx < 0) { if (x > 0)   --x; }
        else if (dx > 0) { if (x < 248) ++x; }
        if (dy < 0) { if (y > 8)   --y; }
        else if (dy > 0) { if (y < 222) ++y; }

        h = (h + 1) & HMASK;
        hx[h] = x;
        hy[h] = y;

        /* ---- fill OAM ----
           Slot 0 is always the head. The body segments follow in one
           cyclic order whose starting point advances by one each frame,
           so on any crowded scanline every sprite gets an even share of
           the 8 drawable positions and no segment stays hidden long. */
        slot = 0;

        seg = 0;
        put_seg();

        seg = 1 + rot;
        for (k = 0; k < SEGS - 1; ++k) {
            put_seg();
            ++seg;
            if (seg >= SEGS) seg = 1;
        }

        ++rot;
        if (rot >= SEGS - 1) rot = 0;

        oam_ready();
        wait_frame();
    }
}
