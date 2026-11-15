This version rotates the OAM order of all six characters, the player included, instead of keeping the player first. I haven't compiled it or run it through the checker.

Which sprites the PPU drops depends only on OAM order, so rotating the order shares the 8-per-line overflow between characters. On a line where the player overlaps the enemies, 12 sprites compete for 8 slots, so two of the six characters lose out each frame. Rotating the start by 2 (mod 6) means each character is dropped on one frame in three and never on two frames in a row. When the player isn't on a line, only one enemy is dropped there, and that rotates too.

The cost is that the player now flickers briefly when overlapping enemies, which the old "player never dropped" rule avoided. The objects, sizes, palettes, positions and movement are unchanged.

```c
#include "nes.h"

#define NUM_ENEMIES 5
#define NUM_CHARS   (NUM_ENEMIES + 1)
#define GROUND_Y    200   /* feet line */
#define CHAR_H      32
#define CHAR_W      16
#define MIN_X       8
#define MAX_X       232
#define MIN_Y       120
#define MAX_Y       208

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

static void fill_oam(void)
{
    unsigned char n, c, slot;

    /* All six characters (player = 0, enemies = 1..5) share one rotating
       OAM order. Where a line is overloaded (12 sprites wanted, 8 fit),
       two characters lose out; stepping the start by 2 (mod 6) makes each
       character lose exactly one frame in three, never two in a row. */
    slot = 0;
    for (n = 0; n < NUM_CHARS; ++n) {
        c = n + rot;
        if (c >= NUM_CHARS) c -= NUM_CHARS;
        if (c == 0) {
            draw_char(slot, px, py - CHAR_H, 0, pface);
        } else {
            draw_char(slot, (unsigned char)(enemies[c - 1].x >> 8), GROUND_Y - CHAR_H,
                      1 + ((c - 1) % 3), enemies[c - 1].dir);
        }
        slot += 4;
    }
    rot += 2;
    if (rot >= NUM_CHARS) rot -= NUM_CHARS;
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
    }
    px = 128; py = GROUND_Y + 8; pface = 0; rot = 0;

    for (;;) {
        update();
        fill_oam();
        oam_ready();
        wait_frame();
    }
}
```