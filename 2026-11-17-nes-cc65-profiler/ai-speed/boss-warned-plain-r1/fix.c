#include "nes.h"

#define BOSS_CY   88
#define BOSS_TOP  (BOSS_CY - 24)
#define ORBIT_R   56

#define SPR_FIRE   0
#define SPR_PLAYER 8
#define SPR_BOSS   12

static const unsigned char palette[32] = {
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x06, 0x16, 0x28,
    0x0F, 0x16, 0x27, 0x30,
    0x0F, 0x11, 0x21, 0x30,
    0x0F, 0x00, 0x10, 0x30
};

static const unsigned char qsin[10] = { 0, 11, 21, 31, 40, 47, 52, 55, 56, 56 };

static unsigned char sin_q(unsigned char idx)
{
    unsigned char i = idx >> 3;
    unsigned char f = idx & 7;
    return qsin[i] + (unsigned char)(((unsigned char)(qsin[i + 1] - qsin[i]) * f) >> 3);
}

static int isin(unsigned char a)
{
    unsigned char idx = a & 63;
    int v;
    if (a & 64) v = sin_q(64 - idx);
    else        v = sin_q(idx);
    if (a & 128) v = -v;
    return v;
}

/* precomputed sine table, built once at startup */
static signed char sintab[256];

static unsigned char px, py;
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

/* everything that never changes is written to OAM once */
static void init_static(void)
{
    unsigned char r, c, i, t, s;
    unsigned int k;

    k = 0;
    do {
        sintab[k] = (signed char)isin((unsigned char)k);
    } while (++k < 256);

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
    unsigned char c, i, a, bx, xc, cx, yy;
    unsigned char *p;

    cx = (unsigned char)(128 + sintab[sway_ang]);

    /* fireballs */
    p = &OAM_Y(SPR_FIRE);
    a = orbit_ang;
    for (i = 0; i < 8; ++i) {
        p[0] = (unsigned char)(83 + sintab[a]);
        p[3] = (unsigned char)(cx - 4 + sintab[(unsigned char)(a + 64)]);
        p += 4;
        a += 32;
    }

    /* player */
    yy = py - 1;
    p = &OAM_Y(SPR_PLAYER);
    p[0]  = yy;      p[3]  = px;
    p[4]  = yy;      p[7]  = px + 8;
    p[8]  = yy + 8;  p[11] = px;
    p[12] = yy + 8;  p[15] = px + 8;

    /* boss: only X changes; one column of 6 sprites at a time */
    bx = cx - 24;
    p = &OAM_X(SPR_BOSS);
    for (c = 0; c < 6; ++c) {
        xc = bx;
        p[0]   = xc;
        p[24]  = xc;
        p[48]  = xc;
        p[72]  = xc;
        p[96]  = xc;
        p[120] = xc;
        p += 4;
        bx += 8;
    }
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

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
