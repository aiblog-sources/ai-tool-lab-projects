#include "SMSlib.h"

#define W   32
#define H   24
#define PW  34              /* padded width (torus wrap border) */
#define PH  26

/* ---------- assets (all defined here) ---------- */

/* 4bpp planar tiles: tile 0 = dead (empty), tile 1 = alive (block, colour 1) */
const unsigned char tiles[64] = {
    /* tile 0: dead */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* tile 1: alive (bitplane 0 only -> colour index 1) */
    0x00,0,0,0, 0x7E,0,0,0, 0x7E,0,0,0, 0x7E,0,0,0,
    0x7E,0,0,0, 0x7E,0,0,0, 0x7E,0,0,0, 0x00,0,0,0
};

const unsigned char palette[16] = {
    RGB(0,0,1), RGB(0,3,1), RGB(3,3,3), RGB(3,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* ---------- state ---------- */

static unsigned char grid[2][PW * PH];
static unsigned int  tilemap[W * H];     /* what gets pushed to the VRAM name table */
static unsigned int  rng = 0xACE1;
static unsigned int  generation;

static unsigned int next_rand(void)
{
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

static void randomize(unsigned char *g)
{
    unsigned char x, y;
    unsigned char *p;
    for (y = 0; y < PH * PW / 256 + 1; y++) { }   /* (no-op, keeps code simple) */
    for (x = 0; x < PH; x++) {
        p = g + (unsigned int)x * PW;
        for (y = 0; y < PW; y++)
            p[y] = 0;
    }
    for (y = 1; y <= H; y++) {
        p = g + (unsigned int)y * PW + 1;
        for (x = 0; x < W; x++)
            p[x] = (next_rand() >> 8) & 1;
    }
}

static void wrap_edges(unsigned char *g)
{
    unsigned char y, x;
    unsigned char *p;
    for (y = 1; y <= H; y++) {
        p = g + (unsigned int)y * PW;
        p[0]     = p[W];
        p[W + 1] = p[1];
    }
    p = g;
    for (x = 0; x < PW; x++) {
        p[x]                       = p[(unsigned int)H * PW + x];
        p[(unsigned int)(H + 1) * PW + x] = p[PW + x];
    }
}

/* advance one generation: src -> dst, also fills tilemap; returns population */
static unsigned int step(const unsigned char *src, unsigned char *dst)
{
    unsigned char x, y, n, a, r;
    const unsigned char *s;
    unsigned char *d;
    unsigned int *t = tilemap;
    unsigned int pop = 0;

    wrap_edges((unsigned char *)src);

    for (y = 1; y <= H; y++) {
        s = src + (unsigned int)y * PW + 1;
        d = dst + (unsigned int)y * PW + 1;
        for (x = 0; x < W; x++) {
            n = s[-PW - 1] + s[-PW] + s[-PW + 1]
              + s[-1]               + s[1]
              + s[PW - 1]  + s[PW]  + s[PW + 1];
            a = *s;
            r = (n == 3) || (a && n == 2);
            *d++ = r;
            *t++ = r;
            pop += r;
            s++;
        }
    }
    return pop;
}

void main(void)
{
    unsigned char cur = 0;
    unsigned int pop;

    SMS_displayOff();
    SMS_loadBGPalette(palette);
    SMS_loadSpritePalette(palette);
    SMS_setBackdropColor(0);
    SMS_loadTiles(tiles, 0, sizeof(tiles));
    SMS_VRAMmemsetW(XYtoADDR(0, 0), 0, W * H);   /* clear name table */

    randomize(grid[0]);
    generation = 0;
    SMS_displayOn();

    for (;;) {
        /* compute next generation while the display is running */
        pop = step(grid[cur], grid[cur ^ 1]);
        cur ^= 1;
        generation++;

        /* reseed when the world dies out or periodically */
        if (pop == 0 || generation >= 1500) {
            randomize(grid[cur]);
            generation = 0;
        }

        /* redraw the entire 32x24 tile map; display is blanked during the
           copy because 1536 bytes don't fit in VBlank alone */
        SMS_waitForVBlank();
        SMS_displayOff();
        SMS_loadTileMap(0, 0, tilemap, sizeof(tilemap));
        SMS_displayOn();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
