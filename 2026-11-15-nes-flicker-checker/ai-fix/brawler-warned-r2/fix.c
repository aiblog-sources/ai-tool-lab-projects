#include "nes.h"

#define NUM_ENEMIES 5
#define GROUND_Y    200   /* feet line */
#define CHAR_H      32
#define CHAR_W      16
#define MIN_X       8
#define MAX_X       232
#define MIN_Y       120
#define MAX_Y       208

/* Enemy line bands (screen lines covered by the 8x16 sprites) */
#define EN_TOP      (GROUND_Y - CHAR_H)       /* 168 */
#define HEAD_LO     EN_TOP
#define HEAD_HI     (EN_TOP + 15)             /* 183 */
#define BODY_LO     (EN_TOP + 16)             /* 184 */
#define BODY_HI     (EN_TOP + 31)             /* 199 */

static const unsigned char palette[32] = {
    /* background */
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    /* sprites: 0 player, 1-3 enemies */
    0x0F,0x2A,0x30,0x1A, 0x0F,0x16,0x30,0x26, 0x0F,0x12,0x30,0x22, 0x0F,0x17,0x30,0x28
};

typedef struct {
    unsigned int  x;     /* 8.8 fixed point */
    unsigned int  spd;   /* 8.8 pixels per frame */
    unsigned char dir;   /* 0 = right, 1 = left */
} Enemy;

static Enemy enemies[NUM_ENEMIES];
static unsigned char px, py;
static unsigned char pface;      /* 0 = right, 1 = left */
static unsigned char rot;
static unsigned char pad;

/* group 0 = head pairs, group 1 = body pairs */
static unsigned char age[2][NUM_ENEMIES];   /* consecutive frames hidden */
static unsigned char sel[2][NUM_ENEMIES];   /* shown this frame */

static const unsigned int speeds[NUM_ENEMIES] = { 0x0060, 0x00A0, 0x00E0, 0x0130, 0x0190 };
static const unsigned int starts[NUM_ENEMIES] = { 20, 70, 120, 170, 215 };

/* Draw one 16x32 character as four 8x16 sprites at OAM slots slot..slot+3.
   Top row: ball pair ($12) as head; bottom row: body pair ($10). */
static void draw_char(unsigned char slot, unsigned char x, unsigned char y,
                      unsigned char pal, unsigned char flip)
{
    unsigned char attr = pal | (flip ? 0x40 : 0);
    unsigned char xl = flip ? x + 8 : x;
    unsigned char xr = flip ? x : x + 8;
    unsigned char ty = y - 1;          /* OAM Y is line-1 */
    unsigned char by = y + 15;

    OAM_Y(slot)     = ty; OAM_TILE(slot)     = 0x12; OAM_ATTR(slot)     = attr; OAM_X(slot)     = xl;
    OAM_Y(slot + 1) = ty; OAM_TILE(slot + 1) = 0x12; OAM_ATTR(slot + 1) = attr; OAM_X(slot + 1) = xr;
    OAM_Y(slot + 2) = by; OAM_TILE(slot + 2) = 0x10; OAM_ATTR(slot + 2) = attr; OAM_X(slot + 2) = xl;
    OAM_Y(slot + 3) = by; OAM_TILE(slot + 3) = 0x10; OAM_ATTR(slot + 3) = attr; OAM_X(slot + 3) = xr;
}

/* One row (two side-by-side 8x16 sprites) of a character. */
static void draw_pair(unsigned char slot, unsigned char x, unsigned char oy,
                      unsigned char tile, unsigned char pal, unsigned char flip,
                      unsigned char show)
{
    unsigned char attr = pal | (flip ? 0x40 : 0);
    unsigned char xl = flip ? x + 8 : x;
    unsigned char xr = flip ? x : x + 8;
    unsigned char y = show ? oy : 0xFF;

    OAM_Y(slot)     = y; OAM_TILE(slot)     = tile; OAM_ATTR(slot)     = attr; OAM_X(slot)     = xl;
    OAM_Y(slot + 1) = y; OAM_TILE(slot + 1) = tile; OAM_ATTR(slot + 1) = attr; OAM_X(slot + 1) = xr;
}

static void update(void)
{
    unsigned char i;
    Enemy *e;

    pad = pad_read();
    if (pad & PAD_LEFT)  { if (px > MIN_X) --px; --px; pface = 1; if (px < MIN_X) px = MIN_X; }
    if (pad & PAD_RIGHT) { ++px; ++px; pface = 0; if (px > MAX_X) px = MAX_X; }
    if (pad & PAD_UP)    { if (py > MIN_Y + 2) py -= 2; else py = MIN_Y; }
    if (pad & PAD_DOWN)  { if (py < MAX_Y - 2) py += 2; else py = MAX_Y; }

    for (i = 0; i < NUM_ENEMIES; ++i) {
        e = &enemies[i];
        if (e->dir == 0) {
            e->x += e->spd;
            if ((e->x >> 8) >= MAX_X) { e->x = (unsigned int)MAX_X << 8; e->dir = 1; }
        } else {
            if (e->x < e->spd + ((unsigned int)MIN_X << 8)) {
                e->x = (unsigned int)MIN_X << 8; e->dir = 0;
            } else {
                e->x -= e->spd;
            }
        }
    }
}

/* Choose k enemies of group g to show: the ones hidden the longest first,
   ties broken by a rotating start. */
static void pick(unsigned char g, unsigned char k)
{
    unsigned char i, n, j, c, best, found;

    for (i = 0; i < NUM_ENEMIES; ++i) sel[g][i] = 0;

    for (c = 0; c < k; ++c) {
        found = 0;
        best = 0;
        for (n = 0; n < NUM_ENEMIES; ++n) {
            j = n + rot;
            if (j >= NUM_ENEMIES) j -= NUM_ENEMIES;
            if (!sel[g][j] && (!found || age[g][j] > age[g][best])) {
                best = j;
                found = 1;
            }
        }
        if (found) sel[g][best] = 1;
    }

    for (i = 0; i < NUM_ENEMIES; ++i) {
        if (sel[g][i]) age[g][i] = 0;
        else if (age[g][i] < 255) ++age[g][i];
    }
}

static void fill_oam(void)
{
    unsigned char i, slot, kh, kb, x, pal, fl;
    int ptop, pbot;

    /* player first: always drawn */
    draw_char(0, px, py - CHAR_H, 0, pface);

    /* The player covers 2 sprites on every line from py-32 to py-1.
       Work out how many whole enemy rows fit within 8 sprites per line,
       and only emit those, so the PPU never has to drop anything
       halfway through a sprite. */
    ptop = (int)py - CHAR_H;
    pbot = (int)py - 1;
    kh = (pbot >= HEAD_LO && ptop <= HEAD_HI) ? 3 : 4;
    kb = (pbot >= BODY_LO && ptop <= BODY_HI) ? 3 : 4;

    pick(0, kh);
    pick(1, kb);
    rot = (rot + 1 >= NUM_ENEMIES) ? 0 : rot + 1;

    slot = 4;
    for (i = 0; i < NUM_ENEMIES; ++i) {
        x   = (unsigned char)(enemies[i].x >> 8);
        pal = 1 + (i % 3);
        fl  = enemies[i].dir;
        draw_pair(slot,     x, GROUND_Y - CHAR_H - 1,      0x12, pal, fl, sel[0][i]);
        draw_pair(slot + 2, x, GROUND_Y - CHAR_H + 15,     0x10, pal, fl, sel[1][i]);
        slot += 4;
    }
}

void main(void)
{
    unsigned char i;

    ppu_setup(palette);
    oam_hide_all();
    ppu_ctrl |= CTRL_SPR_8X16;

    for (i = 0; i < NUM_ENEMIES; ++i) {
        enemies[i].x   = starts[i] << 8;
        enemies[i].spd = speeds[i];
        enemies[i].dir = i & 1;
        age[0][i] = 0;
        age[1][i] = 0;
    }
    px = 128; py = GROUND_Y + 8; pface = 0; rot = 0;

    for (;;) {
        update();
        fill_oam();
        oam_ready();
        wait_frame();
    }
}
