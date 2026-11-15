/* main.c - 20-segment snake for the NES (cc65, mapper 0) */
#include "nes.h"

#define SEGS     20
#define DELAY    4      /* frames between one segment and the next */
#define SPEED    2      /* pixels per frame */
#define HMASK    127    /* history ring buffer of 128 entries */

#define TILE_BODY 0x04  /* ball */
#define TILE_HEAD 0x05  /* ring */

#define DIR_RIGHT 0
#define DIR_LEFT  1
#define DIR_UP    2
#define DIR_DOWN  3

/* The PPU draws only 8 sprites per line: the head plus 7 body sprites.
   Advancing the OAM rotation by this many body segments per frame makes
   the visible set hop around the snake, so each segment is drawn at
   least once every 3 frames instead of vanishing for a long stretch. */
#define ROT_STEP 7

static const unsigned char palette[32] = {
    /* background (unused, blank) */
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    /* sprites: pal 0 = green body, pal 1 = red/yellow head */
    0x0F, 0x1A, 0x2A, 0x3A,  0x0F, 0x16, 0x27, 0x38,
    0x0F, 0x12, 0x22, 0x32,  0x0F, 0x13, 0x23, 0x33
};

static unsigned char hist_x[128];
static unsigned char hist_y[128];   /* stored as OAM Y values */
static unsigned char h;             /* index of the head's current position */
static unsigned char hx, hy;
static unsigned char dir;
static unsigned char rot;           /* rotation of body sprites in OAM */

static void init_snake(void)
{
    unsigned char k, idx;

    hx = 160;
    hy = 118;                       /* middle row (lines 119..126) */
    dir = DIR_RIGHT;
    h = HMASK;
    rot = 0;

    /* pre-fill the trail so the snake starts stretched out to the left */
    for (k = 0; k < 128; ++k) {
        idx = (unsigned char)((HMASK - k) & HMASK);
        hist_x[idx] = (k <= (SEGS - 1) * DELAY) ? (unsigned char)(160 - SPEED * k) : 8;
        hist_y[idx] = hy;
    }
}

static void update(void)
{
    unsigned char pad = pad_read();

    /* turn, but don't reverse straight into the neck */
    if ((pad & PAD_RIGHT) && dir != DIR_LEFT)  dir = DIR_RIGHT;
    else if ((pad & PAD_LEFT)  && dir != DIR_RIGHT) dir = DIR_LEFT;
    else if ((pad & PAD_UP)    && dir != DIR_DOWN)  dir = DIR_UP;
    else if ((pad & PAD_DOWN)  && dir != DIR_UP)    dir = DIR_DOWN;

    switch (dir) {
    case DIR_RIGHT: hx += SPEED; break;             /* wraps at 256 */
    case DIR_LEFT:  hx -= SPEED; break;
    case DIR_UP:
        if (hy < SPEED) hy = hy + 240 - SPEED; else hy -= SPEED;
        break;
    case DIR_DOWN:
        hy += SPEED;
        if (hy >= 240) hy -= 240;
        break;
    }

    h = (h + 1) & HMASK;
    hist_x[h] = hx;
    hist_y[h] = hy;
}

static void fill_oam(void)
{
    unsigned char j, seg, idx;

    /* head always in slot 0 (highest priority) */
    OAM_Y(0)    = hist_y[h];
    OAM_TILE(0) = TILE_HEAD;
    OAM_ATTR(0) = 1;
    OAM_X(0)    = hist_x[h];

    /* body: rotate which segments get the low OAM slots each frame, so
       when more than 8 sprites share a scanline the dropped ones vary
       (flicker) instead of the same segments vanishing. The rotation
       jumps by ROT_STEP so every segment gets drawn at least once in
       any 3 consecutive frames. */
    seg = 1 + rot;
    for (j = 1; j < SEGS; ++j) {
        idx = (h - (unsigned char)(seg * DELAY)) & HMASK;
        OAM_Y(j)    = hist_y[idx];
        OAM_TILE(j) = TILE_BODY;
        OAM_ATTR(j) = 0;
        OAM_X(j)    = hist_x[idx];
        ++seg;
        if (seg >= SEGS) seg = 1;
    }

    rot += ROT_STEP;
    if (rot >= SEGS - 1) rot -= SEGS - 1;
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();
    init_snake();

    for (;;) {
        update();
        fill_oam();
        oam_ready();
        wait_frame();
    }
}
