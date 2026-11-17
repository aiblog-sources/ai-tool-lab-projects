The main cost was `put_sprite` with `int` arguments, called 24 times a frame, plus the software shifts and stack helpers it drags in. I made these changes:

- **Static OAM bytes written once.** Tiles and attributes never change, and the boss and player Y values are fixed, so they are now set once before the loop. The OAM shadow persists between loops.
- **Boss X writes unrolled.** Each loop writes only the 36 boss X values, as constant-address stores.
- **Sine lookup table.** The branchy `sin64` is replaced by a 64-entry unsigned table offset by +36, so there is no sign extension.
- **8-bit arithmetic only.** All positions fit in 8 bits, so the results are identical to the original.
- **Cheaper fireball loop.** The fireballs write directly through a pointer.

I haven't compiled or profiled this, so the 60 fps result is untested.

```c
/* Boss fight demo for cc65 / NES
 * - 48x48 boss built from 36 8x8 sprites, swaying near the top
 * - 8 fireballs orbiting it on a circle of radius 36
 * - 16x16 player (4 sprites) moved with the d-pad along the bottom
 *
 * Sprite-per-scanline budget (hardware limit is 8):
 * The boss uses 6 sprites on each of its lines. The orbit radius (36) keeps
 * the fireballs outside the boss except around the left/right sides, where at
 * most one fireball per side overlaps the boss rows. That gives at most
 * 6 + 2 = 8 on any line. The player is far below the boss, so a line never
 * mixes the two groups.
 *
 * Speed: tiles, attributes and the fixed Y values are written to the OAM
 * shadow once at start; each loop only updates the moving X/Y bytes.
 */
#include "nes.h"

/* round(36 * sin(i * 360/64)) + 36, i = 0..63 */
static const unsigned char stab[64] = {
    36, 40, 43, 46, 50, 53, 56, 59, 61, 64, 66, 68, 69, 70, 71, 72,
    72, 72, 71, 70, 69, 68, 66, 64, 61, 59, 56, 53, 50, 46, 43, 40,
    36, 32, 29, 26, 22, 19, 16, 13, 11,  8,  6,  4,  3,  2,  1,  0,
     0,  0,  1,  2,  3,  4,  6,  8, 11, 13, 16, 19, 22, 26, 29, 32
};

/* 32-byte palette: 16 background, 16 sprite */
static const unsigned char palette[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    /* sprites: 0 boss, 1 player, 2 fireballs, 3 spare */
    0x0F, 0x16, 0x27, 0x30,
    0x0F, 0x11, 0x2A, 0x30,
    0x0F, 0x27, 0x37, 0x30,
    0x0F, 0x16, 0x27, 0x30
};

#define BOSS_CY   56
#define BOSS_HALF 24
#define ORBIT_R   36
#define PLAYER_Y  200
#define PLAYER_MIN_X 8
#define PLAYER_MAX_X 232

static unsigned char frame;
static unsigned char player_x;
static unsigned char pad;

static void put_sprite(unsigned char i, int x, int y,
                       unsigned char tile, unsigned char attr)
{
    OAM_Y(i)    = (unsigned char)(y - 1);
    OAM_TILE(i) = tile;
    OAM_ATTR(i) = attr;
    OAM_X(i)    = (unsigned char)x;
}

/* write everything that never changes, once */
static void init_oam(void)
{
    unsigned char i = 0;
    unsigned char r, c, tile;

    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            tile = 0x01;
            if (r == 2 && (c == 1 || c == 4)) tile = 0x03;      /* eyes */
            else if (r == 4 && (c == 2 || c == 3)) tile = 0x02; /* mouth */
            put_sprite(i++, 0, (BOSS_CY - BOSS_HALF) + (r << 3), tile, 0);
        }
    }
    for (c = 0; c < 8; ++c)
        put_sprite(i++, 0, 0, 0x04, 2);

    put_sprite(i++, 0, PLAYER_Y,     0x06, 1);
    put_sprite(i++, 0, PLAYER_Y,     0x06, 1 | 0x40);
    put_sprite(i++, 0, PLAYER_Y + 8, 0x06, 1 | 0x80);
    put_sprite(i++, 0, PLAYER_Y + 8, 0x06, 1 | 0xC0);
}

static void update(void)
{
    ++frame;
    pad = pad_read();
    if ((pad & PAD_LEFT) && player_x > PLAYER_MIN_X)  player_x -= 2;
    if ((pad & PAD_RIGHT) && player_x < PLAYER_MAX_X) player_x += 2;
}

#define BOSSCOL(c) \
    OAM_X(c) = x;      OAM_X((c) + 6) = x;  OAM_X((c) + 12) = x; \
    OAM_X((c) + 18) = x; OAM_X((c) + 24) = x; OAM_X((c) + 30) = x;

static void draw(void)
{
    unsigned char *p;
    unsigned char k, a, t, x, bx, cxm, ang;

    /* boss sway: cx = 56 + 2*t  (centre 128, +-72) */
    t = stab[(frame >> 2) & 63];
    t = (unsigned char)(t << 1);
    bx  = (unsigned char)(32 + t);   /* cx - 24 */
    cxm = (unsigned char)(16 + t);   /* cx - 40 */

    /* boss body: x of each column */
    x = bx; BOSSCOL(0)
    x += 8; BOSSCOL(1)
    x += 8; BOSSCOL(2)
    x += 8; BOSSCOL(3)
    x += 8; BOSSCOL(4)
    x += 8; BOSSCOL(5)

    /* 8 fireballs orbiting the boss centre */
    ang = frame >> 1;
    p = OAM + 144;                   /* sprite 36 */
    for (k = 0; k < 8; ++k) {
        a = (unsigned char)(ang + (k << 3));
        p[0] = (unsigned char)(15 + stab[a & 63]);                 /* Y */
        p[3] = (unsigned char)(cxm + stab[(a + 16) & 63]);         /* X */
        p += 4;
    }

    /* player */
    x = player_x;
    OAM_X(44) = x;
    OAM_X(46) = x;
    x += 8;
    OAM_X(45) = x;
    OAM_X(47) = x;
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    frame = 0;
    player_x = 120;

    init_oam();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
```