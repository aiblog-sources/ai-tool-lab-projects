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
 * Speed: static OAM fields (tile, attribute, boss Y) are written once at
 * startup; per frame only positions are updated, using a precomputed sine
 * table.
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

/* signed sine, angle 0..255 = full circle, result -56..56 */
static int isin(unsigned char a)
{
    unsigned char idx = a & 63;
    int v;
    if (a & 64) v = sin_q(64 - idx);
    else        v = sin_q(idx);
    if (a & 128) v = -v;
    return v;
}

static signed char sintab[256];

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

/* one-time setup of everything that never changes */
static void init_static(void)
{
    unsigned char r, c, i, t, s;
    unsigned char *o;

    i = 0;
    do {
        sintab[i] = (signed char)isin(i);
    } while (++i != 0);

    o = OAM;
    for (i = 0; i < 8; ++i) {
        o[1] = 0x04;
        o[2] = 0x01;
        o += 4;
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
            else if ((r == 2 && (c == 2 || c == 3)) ||
                     (r == 4)) t = 0x03;
            else t = 0x02;
            OAM_Y(s)    = BOSS_TOP + (r << 3) - 1;
            OAM_TILE(s) = t;
            OAM_ATTR(s) = 0x00;
        }
    }
}

static void draw(void)
{
    unsigned char r, i, a, cx, x0, x1, x2, x3, x4, x5, ty;
    unsigned char *o;

    cx = (unsigned char)(128 + sintab[sway_ang]);   /* boss centre X: 72..184 */

    /* fireballs: slots 0..7 */
    o = OAM;
    a = orbit_ang;
    for (i = 0; i < 8; ++i) {
        o[0] = (unsigned char)(83 + sintab[a]);                 /* BOSS_CY + sin - 4 - 1 */
        o[3] = (unsigned char)(cx - 4 + sintab[(unsigned char)(a + 64)]);
        o += 4;
        a += 32;
    }

    /* player */
    ty = py - 1;
    OAM_Y(SPR_PLAYER + 0) = ty;
    OAM_Y(SPR_PLAYER + 1) = ty;
    ty = py + 7;
    OAM_Y(SPR_PLAYER + 2) = ty;
    OAM_Y(SPR_PLAYER + 3) = ty;
    OAM_X(SPR_PLAYER + 0) = px;
    OAM_X(SPR_PLAYER + 1) = px + 8;
    OAM_X(SPR_PLAYER + 2) = px;
    OAM_X(SPR_PLAYER + 3) = px + 8;

    /* boss: only X changes */
    x0 = cx - 24;
    x1 = x0 + 8;
    x2 = x0 + 16;
    x3 = x0 + 24;
    x4 = x0 + 32;
    x5 = x0 + 40;
    o = OAM + (SPR_BOSS * 4) + 3;
    for (r = 0; r < 6; ++r) {
        o[0]  = x0;
        o[4]  = x1;
        o[8]  = x2;
        o[12] = x3;
        o[16] = x4;
        o[20] = x5;
        o += 24;
    }
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();                 /* unused slots 48..63 stay hidden */

    px = 120;
    py = 192;
    orbit_ang = 0;
    sway_ang = 0;

    init_static();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
