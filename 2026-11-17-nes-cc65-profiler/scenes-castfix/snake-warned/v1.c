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
static unsigned char used[SEGS];

/* write segment 'seg' into the next OAM slot */
static void put_seg(void)
{
    idx = (h - seg * DELAY) & HMASK;
    OAM_Y(slot)    = hy[idx];
    OAM_TILE(slot) = 0x04;
    OAM_ATTR(slot) = (seg == 0) ? 1 : 0;
    OAM_X(slot)    = hx[idx];
    used[seg] = 1;
    ++slot;
}

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
           Only 8 sprites per scanline can be drawn, so the first 8 OAM
           slots decide what shows on a crowded line. Slot 0 is always the
           head; the next 7 slots take body segments spaced 3 apart (about
           12 px, leaving small 4 px gaps) starting at an offset that moves
           every frame, so every segment gets drawn regularly. The other
           segments follow in slots 8..19. */
        for (k = 0; k < SEGS; ++k) used[k] = 0;
        slot = 0;

        seg = 0;
        put_seg();

        for (k = 0; k < 7; ++k) {
            seg = 1 + (unsigned char)((rot + 3 * k) % (SEGS - 1));
            put_seg();
        }

        for (seg = 1; seg < SEGS; ++seg) {
            if (!used[seg]) put_seg();
        }

        ++rot;
        if (rot >= SEGS - 1) rot = 0;

        oam_ready();
        wait_frame();
    }
}
