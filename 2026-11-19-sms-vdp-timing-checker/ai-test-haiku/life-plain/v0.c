/*
 * Conway's Game of Life on the SMS background (32x24 tiles).
 *
 * Each cell is one background tile: tile 0 = dead, tile 1 = alive.
 * The grid is stored bit-packed (one unsigned long per 32-cell row) and
 * advanced with bit-parallel logic, which is much faster on the Z80 than
 * per-cell neighbour counting. The grid wraps around at the edges (torus).
 * Each frame: step one generation, wait for VBlank, redraw the whole map.
 *
 * Build: sdcc -c -mz80 main.c  (link with crt0_sms.rel and SMSlib.lib, then makesms)
 */

#include "SMSlib.h"

#define COLS        32
#define ROWS        24
#define TILE_BYTES  64      /* 2 tiles x 32 bytes (4bpp) */

/* ------------------------------------------------------------------------ */
/* Graphics data (defined in C, no external assets)                          */
/* ------------------------------------------------------------------------ */

/* 4bpp tiles: 8 rows x 4 bitplane bytes per tile. Colour index = plane0 bit
 * for index 1, so an all-0xFF plane0 with zeros elsewhere is colour 1.      */
static const unsigned char tile_data[TILE_BYTES] = {
    /* tile 0 = dead: colour 0 everywhere */
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    /* tile 1 = alive: colour 1 everywhere */
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,
    0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00,  0xFF,0x00,0x00,0x00
};

/* Background palette: entry 0 = dead colour (and backdrop), entry 1 = alive colour */
static const unsigned char palette[16] = {
    RGB(0,0,0),   /* 0: dead cell */
    RGB(0,3,0),   /* 1: alive cell */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------------ */
/* Simulation state                                                          */
/* ------------------------------------------------------------------------ */

static unsigned long bufA[ROWS];
static unsigned long bufB[ROWS];
static unsigned long *cur = bufA;   /* current generation */
static unsigned long *nxt = bufB;   /* next generation    */

/* 16-bit xorshift PRNG; it never reaches 0 from a non-zero seed */
static unsigned int rng = 0xACE1;

static unsigned int rnd(void)
{
    unsigned int x = rng;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    rng = x;
    return x;
}

/* Fill the grid with a random pattern (~25% alive) */
static void randomize(void)
{
    unsigned char r;
    unsigned int h, l;
    unsigned long a, b;

    for (r = 0; r < ROWS; r++) {
        h = rnd(); l = rnd();
        a = ((unsigned long)h << 16) | l;
        h = rnd(); l = rnd();
        b = ((unsigned long)h << 16) | l;
        cur[r] = a & b;
    }
}

/*
 * Bit-sliced neighbour counter: adds bit vector x into the 4-bit
 * per-column counter (eights:fours:twos:ones).
 */
#define ADD_BIT(x) do {                       \
        unsigned long c1_ = ones & (x);       \
        ones ^= (x);                          \
        {                                     \
            unsigned long c2_ = twos & c1_;   \
            twos ^= c1_;                      \
            {                                 \
                unsigned long c3_ = fours & c2_; \
                fours ^= c2_;                 \
                eights ^= c3_;                \
            }                                 \
        }                                     \
    } while (0)

/* Advance one generation: cur -> nxt, then swap buffers */
static void step(void)
{
    unsigned char r;

    for (r = 0; r < ROWS; r++) {
        unsigned char ra = (r == 0) ? (ROWS - 1) : (r - 1);
        unsigned char rb = (r == ROWS - 1) ? 0 : (r + 1);

        unsigned long a = cur[ra];
        unsigned long m = cur[r];
        unsigned long b = cur[rb];

        /* horizontal neighbours, with wrap-around */
        unsigned long aL = (a << 1) | (a >> 31);
        unsigned long aR = (a >> 1) | (a << 31);
        unsigned long mL = (m << 1) | (m >> 31);
        unsigned long mR = (m >> 1) | (m << 31);
        unsigned long bL = (b << 1) | (b >> 31);
        unsigned long bR = (b >> 1) | (b << 31);

        unsigned long ones = 0, twos = 0, fours = 0, eights = 0;

        /* the 8 neighbours of every cell in row r */
        ADD_BIT(aL); ADD_BIT(a);  ADD_BIT(aR);
        ADD_BIT(mL); ADD_BIT(mR);
        ADD_BIT(bL); ADD_BIT(b);  ADD_BIT(bR);

        /*
         * Life rule:
         *   count == 3                -> alive
         *   count == 2 and alive      -> alive
         * count == 2 means twos=1, fours=0, eights=0, ones=0
         * count == 3 means twos=1, fours=0, eights=0, ones=1
         */
        nxt[r] = twos & ~fours & ~eights & (ones | m);
    }

    {
        unsigned long *t = cur;
        cur = nxt;
        nxt = t;
    }
}

/* Redraw the entire 32x24 tile map from the current generation */
static void draw(void)
{
    unsigned char r, i, k;

    SMS_setNextTileatXY(0, 0);
    for (r = 0; r < ROWS; r++) {
        /* little-endian: byte 0 holds columns 0..7, byte 1 holds 8..15, ... */
        unsigned char *p = (unsigned char *)&cur[r];
        for (i = 0; i < 4; i++) {
            unsigned char bits = p[i];
            for (k = 0; k < 8; k++) {
                SMS_setTile(bits & 1);   /* tile 0 = dead, tile 1 = alive */
                bits >>= 1;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Main                                                                      */
/* ------------------------------------------------------------------------ */

void main(void)
{
    SMS_displayOff();
    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ);

    SMS_loadBGPalette(palette);
    SMS_loadTiles(tile_data, 0, TILE_BYTES);

    SMS_initSprites();
    SMS_copySpritestoSAT();

    /*
     * Start-up: the pattern keeps changing until button 1 is pressed, so
     * the human timing of the press provides the random seed.
     */
    randomize();
    SMS_waitForVBlank();
    draw();
    SMS_displayOn();

    while (!(SMS_getKeysPressed() & PORT_A_KEY_1)) {
        randomize();
        SMS_waitForVBlank();
        draw();
    }

    /* Main loop: one generation per frame, whole map redrawn each frame.
     * Pressing button 1 during the simulation starts a new random pattern. */
    while (1) {
        if (SMS_getKeysPressed() & PORT_A_KEY_1) {
            randomize();
        }
        step();
        SMS_waitForVBlank();
        draw();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
