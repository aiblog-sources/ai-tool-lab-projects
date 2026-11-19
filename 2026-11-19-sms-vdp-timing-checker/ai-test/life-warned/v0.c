#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Hardware access                                                     */
/* ------------------------------------------------------------------ */
__sfr __at (0xbe) VDPData;
__sfr __at (0x7e) VDPVCounter;
__sfr __at (0xdc) IOPortA;

/* ------------------------------------------------------------------ */
/* Graphics data (4bpp planar, 8 rows x 4 bytes per tile)              */
/* ------------------------------------------------------------------ */
/* tile 0 = dead (blank, colour 0), tile 1 = alive (square, colour 1) */
const unsigned char tiles[64] = {
    /* tile 0: dead */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* tile 1: alive */
    0x00,0,0,0,
    0x7E,0,0,0, 0x7E,0,0,0, 0x7E,0,0,0,
    0x7E,0,0,0, 0x7E,0,0,0, 0x7E,0,0,0,
    0x00,0,0,0
};

const unsigned char palette[16] = {
    RGB(0,0,1),          /* 0: dead background */
    RGB(1,3,1),          /* 1: live cell       */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ------------------------------------------------------------------ */
/* Life state                                                          */
/* ------------------------------------------------------------------ */
#define CELLS 768

static unsigned char bufA[CELLS];
static unsigned char bufB[CELLS];
static unsigned char *cur;
static unsigned char *nxt;
static unsigned int rng;

static unsigned int nextRandom(void)
{
    rng ^= rng << 7;
    rng ^= rng >> 9;
    rng ^= rng << 8;
    return rng;
}

static void randomize(unsigned int seed)
{
    unsigned int i;
    rng = seed ? seed : 0xACE1;
    for (i = 0; i < CELLS; i++) {
        cur[i] = (((nextRandom() >> 8) & 7) < 3) ? 1 : 0;
    }
}

/* one generation, toroidal wrap */
static void step(void)
{
    unsigned char y, x, xl, xr, n;
    const unsigned char *up, *mid, *dn;
    unsigned char *out;
    unsigned char *t;

    for (y = 0; y < 24; y++) {
        up  = cur + ((unsigned int)(y == 0  ? 23 : y - 1) << 5);
        mid = cur + ((unsigned int)y << 5);
        dn  = cur + ((unsigned int)(y == 23 ? 0  : y + 1) << 5);
        out = nxt + ((unsigned int)y << 5);
        for (x = 0; x < 32; x++) {
            xl = (x - 1) & 31;
            xr = (x + 1) & 31;
            n = up[xl] + up[x] + up[xr] + mid[xl] + mid[xr]
              + dn[xl] + dn[x] + dn[xr];
            out[x] = (n == 3 || (n == 2 && mid[x])) ? 1 : 0;
        }
    }
    t = cur; cur = nxt; nxt = t;
}

/* ------------------------------------------------------------------ */
/* Rendering: the whole map is written to the NON-displayed name       */
/* table (double buffered via VDP register 2), with a safe pace of     */
/* >26 Z80 cycles between data port writes so it is also correct on    */
/* real hardware during the active display. The tables are swapped     */
/* at the start of vblank.                                             */
/* Table 0 at 0x3800 (reg2 = 0xFF), table 1 at 0x3000 (reg2 = 0xFD).   */
/* ------------------------------------------------------------------ */
static void renderTo(unsigned int writeAddr)
{
    const unsigned char *p = cur;
    unsigned int i;

    SMS_setAddr(writeAddr);
    for (i = 0; i < CELLS; i++) {
        VDPData = *p++;
        __asm
            nop
            nop
            nop
            nop
        __endasm;
        VDPData = 0;            /* high byte: no flags, tile < 256 */
        __asm
            nop
            nop
            nop
            nop
        __endasm;
    }
}

static void waitVBlankStart(void)
{
    /* poll the V counter (interrupts are disabled) */
    while (VDPVCounter != 0xC1) { }
}

static void showTable(unsigned char which)
{
    SMS_VDPControlPort = which ? 0xFD : 0xFF;
    SMS_VDPControlPort = 0x82;
}

/* ------------------------------------------------------------------ */
void main(void)
{
    unsigned char back;          /* table being written (not displayed) */
    unsigned int frames = 0;
    unsigned char prevBtn = 0, btn;
    unsigned int seed;

    SMS_displayOff();
    SMS_loadTiles(tiles, 0, sizeof(tiles));
    SMS_loadBGPalette(palette);
    SMS_loadSpritePalette(palette);
    SMS_setBackdropColor(0);

    /* we poll the V counter, so keep interrupts off from now on */
    __asm di __endasm;

    cur = bufA;
    nxt = bufB;

    seed = 0xACE1;
    seed ^= (unsigned int)VDPVCounter << 8;
    seed ^= VDPVCounter;
    randomize(seed);

    back = 1;
    renderTo(0x7000);
    showTable(1);
    back = 0;
    SMS_displayOn();

    for (;;) {
        /* restart with a new random pattern on button 1 / 2 */
        btn = ((IOPortA & 0x30) != 0x30) ? 1 : 0;
        if (btn && !prevBtn) {
            seed = frames * 31 + VDPVCounter + 1;
            randomize(seed ^ 0x5A5A);
        } else {
            step();
        }
        prevBtn = btn;

        renderTo(back ? 0x7000 : 0x7800);
        waitVBlankStart();
        showTable(back);
        back ^= 1;
        frames++;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
