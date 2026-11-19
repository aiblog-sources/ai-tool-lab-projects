/*
 * Conway's Game of Life on the SMS background tilemap (32x24 cells).
 *
 * Each cell is one background tile: tile 0 = dead, tile 1 = alive.
 * Edges wrap around (torus). One generation is computed per frame and the
 * whole tilemap is rewritten every frame.
 *
 * The name table is double-buffered: the next frame is written into the
 * hidden name table while the visible one is on screen, then the VDP
 * name-table base register is switched during VBlank. This avoids tearing.
 *
 * Controls: button 1 re-randomizes the field (the SMS pad has no START button;
 * the console's pause button is wired to NMI).
 *
 * Build: sdcc -c -mz80 main.c, link with crt0_sms.rel and SMSlib.lib, then makesms.
 */

#include <SMSlib.h>

#define GRID_W        32
#define GRID_H        24
#define ROW_BYTES     (GRID_W * 2)            /* name table: 2 bytes per cell */
#define TABLE_BYTES   (GRID_W * GRID_H * 2)   /* 1536 bytes */

/* VRAM write addresses of the two name tables (bit 14 = VRAM write) */
#define TABLE_A_ADDR  0x7800                  /* name table at 0x3800 */
#define TABLE_B_ADDR  0x7000                  /* name table at 0x3000 */

/* VDP register 2 values selecting those tables (bits 1..3 = A13..A11) */
#define REG2_TABLE_A  0xFF                    /* base 0x3800 */
#define REG2_TABLE_B  0xFD                    /* base 0x3000 */

/* Tiles 0 and 1 only are used, so tile data never overlaps name table B */

static const unsigned char bg_palette[16] = {
    RGB(0,0,0),   /* 0: dead cell / backdrop */
    RGB(0,3,0),   /* 1: alive cell */
    RGB(0,1,0),   /* 2: border of alive cell */
    0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* 4bpp tiles: 8 rows x 4 bytes (one byte per bitplane) */
static const unsigned char tile_data[64] = {
    /* tile 0: dead, all pixels colour 0 */
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
    /* tile 1: alive, colour 1 inside, colour 2 on a 1-pixel border */
    0x00,0xFF,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x7E,0x81,0x00,0x00,
    0x00,0xFF,0x00,0x00
};

/* Two name-table images. Entry for cell i is at [2*i]; [2*i+1] (high byte,
   tile flags) stays 0 forever because it is never written. */
static unsigned char gridA[TABLE_BYTES];
static unsigned char gridB[TABLE_BYTES];

/* Hidden name table (not on screen) and the register value that shows it */
static unsigned int   hidden_addr = TABLE_B_ADDR;
static unsigned char  hidden_reg2 = REG2_TABLE_B;

/* 16-bit xorshift PRNG */
static unsigned int rng_state = 0xACE1u;

static unsigned char rng_bit(void)
{
    unsigned int s = rng_state;
    s ^= (unsigned int)(s << 7);
    s ^= (unsigned int)(s >> 9);
    s ^= (unsigned int)(s << 8);
    rng_state = s;
    return (unsigned char)(s & 1u);
}

static void rng_tick(void)
{
    rng_state += 0x3D5u;
    if (rng_state == 0) rng_state = 1;   /* xorshift must never be 0 */
}

static void randomize(unsigned char *g)
{
    unsigned int i;
    for (i = 0; i < TABLE_BYTES; i += 2)
        g[i] = rng_bit();
}

/* Computes one generation from src into dst (toroidal edges). */
static void step(const unsigned char *src, unsigned char *dst)
{
    unsigned char x, k, xl, xr, n;
    unsigned char y, yu, yd;
    unsigned char colsum[GRID_W];
    const unsigned char *pu, *pc, *pd;
    unsigned char *po;

    for (y = 0; y < GRID_H; y++) {
        yu = (y == 0) ? (GRID_H - 1) : (y - 1);
        yd = (y == GRID_H - 1) ? 0 : (y + 1);

        pu = src + (unsigned int)yu * ROW_BYTES;
        pc = src + (unsigned int)y  * ROW_BYTES;
        pd = src + (unsigned int)yd * ROW_BYTES;
        po = dst + (unsigned int)y  * ROW_BYTES;

        /* vertical 3-cell sum for every column (cells are 0 or 1) */
        for (x = 0; x < GRID_W; x++) {
            k = (unsigned char)(x << 1);
            colsum[x] = pu[k] + pc[k] + pd[k];
        }

        /* neighbours = both side columns (3 cells each) + up/down of centre */
        for (x = 0; x < GRID_W; x++) {
            xl = (x + (GRID_W - 1)) & (GRID_W - 1);
            xr = (x + 1) & (GRID_W - 1);
            k  = (unsigned char)(x << 1);
            n  = colsum[xl] + colsum[xr] + pu[k] + pd[k];
            po[k] = (n == 3) || (pc[k] && (n == 2));
        }
    }
}

/* Writes the image into the hidden name table, waits for VBlank, then
   switches the VDP to show it. The image on screen is never written to. */
static void present(const unsigned char *g)
{
    __asm di __endasm;
    SMS_VRAMmemcpy(hidden_addr, g, TABLE_BYTES);
    __asm ei __endasm;

    SMS_waitForVBlank();

    __asm di __endasm;
    SMS_VDPControlPort = hidden_reg2;   /* VDP register 2: name table base */
    SMS_VDPControlPort = 0x82;
    __asm ei __endasm;

    /* the table just shown is now the visible one; the other becomes hidden */
    if (hidden_addr == TABLE_B_ADDR) {
        hidden_addr = TABLE_A_ADDR;
        hidden_reg2 = REG2_TABLE_A;
    } else {
        hidden_addr = TABLE_B_ADDR;
        hidden_reg2 = REG2_TABLE_B;
    }
}

void main(void)
{
    unsigned char *cur = gridA;
    unsigned char *nxt = gridB;
    unsigned char *tmp;

    SMS_displayOff();
    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ);   /* needed by SMS_waitForVBlank */

    SMS_loadBGPalette(bg_palette);
    SMS_loadTiles(tile_data, 0, sizeof(tile_data));

    SMS_initSprites();                           /* no sprites are used */
    SMS_copySpritestoSAT();

    /* generation 0: random, written to name table A while display is off */
    randomize(cur);
    SMS_VRAMmemcpy(TABLE_A_ADDR, cur, TABLE_BYTES);
    SMS_VDPControlPort = REG2_TABLE_A;
    SMS_VDPControlPort = 0x82;
    SMS_displayOn();
    /* hidden table is B (set by its initializer) */

    for (;;) {
        rng_tick();

        if (SMS_getKeysPressed() & PORT_A_KEY_1) {
            randomize(cur);          /* new random field replaces current one */
            present(cur);
        } else {
            step(cur, nxt);          /* generation n+1 into nxt */
            present(nxt);
            tmp = cur;
            cur = nxt;
            nxt = tmp;
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
