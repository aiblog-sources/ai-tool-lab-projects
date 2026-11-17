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

static void draw(void)
{
    unsigned char r, c, i, t;
    int cx, x, y;

    cx = 128 + isin(sway_ang);              /* boss centre X: 72..184 */

    /* fireballs: slots 0..7 */
    for (i = 0; i < 8; ++i) {
        unsigned char a = orbit_ang + (i << 5);
        x = cx + isin(a + 64) - 4;
        y = BOSS_CY + isin(a) - 4;
        OAM_Y(SPR_FIRE + i)    = (unsigned char)(y - 1);
        OAM_TILE(SPR_FIRE + i) = 0x04;
        OAM_ATTR(SPR_FIRE + i) = 0x01;
        OAM_X(SPR_FIRE + i)    = (unsigned char)x;
    }

    /* player: 2x2 ship tiles, mirrored into a symmetric shape */
    for (i = 0; i < 4; ++i) {
        unsigned char s = SPR_PLAYER + i;
        OAM_Y(s)    = (unsigned char)(py + ((i >> 1) << 3) - 1);
        OAM_TILE(s) = 0x06;
        OAM_ATTR(s) = 0x02 | ((i & 1) ? 0x40 : 0) | ((i & 2) ? 0x80 : 0);
        OAM_X(s)    = (unsigned char)(px + ((i & 1) << 3));
    }

    /* boss: 6x6 sprites */
    x = cx - 24;
    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            unsigned char s = SPR_BOSS + r * 6 + c;
            if (r == 0 || r == 5 || c == 0 || c == 5) t = 0x01;       /* edge */
            else if ((r == 2 && (c == 2 || c == 3)) ||
                     (r == 4)) t = 0x03;                               /* eyes, mouth */
            else t = 0x02;                                             /* body */
            OAM_Y(s)    = BOSS_TOP + (r << 3) - 1;
            OAM_TILE(s) = t;
            OAM_ATTR(s) = 0x00;
            OAM_X(s)    = (unsigned char)(x + (c << 3));
        }
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

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
