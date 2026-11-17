/* Boss fight for cc65 / NES, mapper 0.
 *
 * Sprite budget (8 per scanline, 64 total):
 *   boss    : 6x6 sprites of 8x8 = 36 sprites, 6 on every boss scanline
 *   fireball: 8 sprites, orbit radius 56 around the boss centre
 *   player  : 2x2 sprites = 4 sprites
 *   total   : 48 of 64
 *
 * A 48-pixel-wide boss needs 6 sprites on each of its scanlines, which leaves
 * room for only 2 more there. The orbit radius is 56, so a fireball overlaps
 * the boss scanlines (centre within 24+3.5 px of the boss centre) only while it
 * is within about 29 degrees of horizontal. Fireballs are 45 degrees apart, so
 * at most one fireball on each side of the boss can be on any boss scanline.
 * That gives 6 + 2 = 8 sprites at most, so nothing is dropped.
 * The player stays at y >= 168, well below the lowest fireball (about y 148),
 * so its 2 sprites per line never combine with the others.
 * OAM slots are fixed (never reordered) and every other slot stays hidden,
 * so nothing flickers.
 *
 * Speed: constant OAM fields (tiles, attributes, boss Y) are written once at
 * start-up, and sine values come from a 256-entry table built at start-up, so
 * each frame only writes the fields that actually change.
 */
#include "nes.h"

#define BOSS_CY   88          /* boss centre Y */
#define BOSS_TOP  (BOSS_CY - 24)
#define ORBIT_R   56

#define SPR_FIRE   0          /* OAM slots 0..7  */
#define SPR_PLAYER 8          /* slots 8..11     */
#define SPR_BOSS   12         /* slots 12..47    */

static const unsigned char palette[32] = {
    /* background (unused, blank) */
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    /* sprites */
    0x0F, 0x06, 0x16, 0x28,  /* 0: boss (dark red edge, red body, yellow face) */
    0x0F, 0x16, 0x27, 0x30,  /* 1: fireballs */
    0x0F, 0x11, 0x21, 0x30,  /* 2: player */
    0x0F, 0x00, 0x10, 0x30   /* 3: spare */
};

/* 56 * sin(i * 11.25 deg), i = 0..8, plus a guard entry */
static const unsigned char qsin[10] = { 0, 11, 21, 31, 40, 47, 52, 55, 56, 56 };

/* sin of a quarter wave: idx 0..64 -> 0..56 */
static unsigned char sin_q(unsigned char idx)
{
    unsigned char i = idx >> 3;
    unsigned char f = idx & 7;
    return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);
}

/* full signed sine table, stored as two's-complement bytes */
static unsigned char sintab[256];

static void build_sin(void)
{
    unsigned char a = 0;
    do {
        unsigned char idx = a & 63;
        unsigned char v;
        if (a & 64) v = sin_q(64 - idx);
        else        v = sin_q(idx);
        if (a & 128) v = (unsigned char)(-v);
        sintab[a] = v;
    } while (++a != 0);
}

static unsigned char px, py;       /* player top-left */
static unsigned char orbit_ang;
static unsigned char sway_ang;

static void update(void)
{
    unsigned char pad = pad_read();

    if ((pad & PAD_LEFT)  && px > 8)    px -= 2;
    if ((pad & PAD_RIGHT) && px < 232)  px += 2;
    if ((pad & PAD_UP)    && py > 168)  py -= 2;
    if ((pad & PAD_DOWN)  && py < 208)  py += 2;

    orbit_ang += 2;
    sway_ang  += 1;
}

/* write everything that never changes */
static void init_oam(void)
{
    unsigned char i, r, c, t, s;

    for (i = 0; i < 8; ++i) {
        OAM_TILE(SPR_FIRE + i) = 0x04;
        OAM_ATTR(SPR_FIRE + i) = 0x01;
    }

    for (i = 0; i < 4; ++i) {
        s = SPR_PLAYER + i;
        OAM_TILE(s) = 0x06;
        OAM_ATTR(s) = 0x02 | ((i & 1) ? 0x40 : 0) | ((i & 2) ? 0x80 : 0);
    }

    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            s = SPR_BOSS + r * 6 + c;
            if (r == 0 || r == 5 || c == 0 || c == 5) t = 0x01;
            else if ((r == 2 && (c == 2 || c == 3)) || (r == 4)) t = 0x03;
            else t = 0x02;
            OAM_Y(s)    = BOSS_TOP + (r << 3) - 1;
            OAM_TILE(s) = t;
            OAM_ATTR(s) = 0x00;
        }
    }
}

static void draw(void)
{
    unsigned char r, i, a, cx, bx;
    unsigned char *p;

    cx = 128 + sintab[sway_ang];            /* boss centre X: 72..184 */

    /* fireballs: slots 0..7 */
    a = orbit_ang;
    p = &OAM[(SPR_FIRE << 2)];
    for (i = 0; i < 8; ++i) {
        p[0] = (unsigned char)(BOSS_CY - 5) + sintab[a];
        p[3] = (unsigned char)(cx - 4) + sintab[(unsigned char)(a + 64)];
        p += 4;
        a += 32;
    }

    /* player */
    OAM_Y(SPR_PLAYER + 0) = py - 1;
    OAM_Y(SPR_PLAYER + 1) = py - 1;
    OAM_Y(SPR_PLAYER + 2) = py + 7;
    OAM_Y(SPR_PLAYER + 3) = py + 7;
    OAM_X(SPR_PLAYER + 0) = px;
    OAM_X(SPR_PLAYER + 1) = px + 8;
    OAM_X(SPR_PLAYER + 2) = px;
    OAM_X(SPR_PLAYER + 3) = px + 8;

    /* boss: only X changes */
    bx = cx - 24;
    p = &OAM[(SPR_BOSS << 2) + 3];
    for (r = 0; r < 6; ++r) {
        p[0]  = bx;
        p[4]  = bx + 8;
        p[8]  = bx + 16;
        p[12] = bx + 24;
        p[16] = bx + 32;
        p[20] = bx + 40;
        p += 24;
    }
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();                 /* unused slots 48..63 stay hidden */
    build_sin();
    init_oam();

    px = 120;
    py = 192;
    orbit_ang = 0;
    sway_ang = 0;

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
