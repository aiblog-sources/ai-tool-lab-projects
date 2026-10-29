/*
 * main.c - SGDK 2.09 - Sega Mega Drive / Genesis
 * Optimised: column writes use a single VDP address setup with a 128-byte
 * auto-increment and raw data-port writes; sprite update only when moved.
 */

#include <genesis.h>

#define SCROLL_SPEED    2
#define PLAYER_SPEED    2
#define MAP_COLS        64
#define MAP_ROWS        28
#define SCREEN_W        320
#define SCREEN_H        224
#define AHEAD_COLS      41

#define MIN_H           9
#define MAX_H           22

#define TILE_BG_BASE    (TILE_USER_INDEX)
#define T_SKY           (TILE_BG_BASE + 0)
#define T_GRASS         (TILE_BG_BASE + 1)
#define T_DIRT          (TILE_BG_BASE + 2)
#define T_SLOPE_UP      (TILE_BG_BASE + 3)
#define T_SLOPE_DOWN    (TILE_BG_BASE + 4)
#define BG_TILE_COUNT   5

#define TILE_SPR_BASE   (TILE_BG_BASE + BG_TILE_COUNT)
#define SPR_TILE_COUNT  4

/* Pre-built attributes (PAL0, no priority, no flip) */
#define ATTR_SKY        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_SKY)
#define ATTR_DIRT       TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_DIRT)

static const u32 bgTiles[BG_TILE_COUNT * 8] =
{
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    0x11111111, 0x11111111, 0x22222222, 0x33333333,
    0x33433333, 0x33333343, 0x34333333, 0x33333433,

    0x33333333, 0x33343333, 0x33333333, 0x43333333,
    0x33333334, 0x33433333, 0x33333333, 0x33333433,

    0x00000001, 0x00000011, 0x00000112, 0x00001123,
    0x00011233, 0x00112333, 0x01123333, 0x11233333,

    0x10000000, 0x11000000, 0x21100000, 0x32110000,
    0x33211000, 0x33321100, 0x33332110, 0x33333211
};

static const u32 playerTiles[SPR_TILE_COUNT * 8] =
{
    0x00000111, 0x00011222, 0x00122222, 0x01223322,
    0x01233222, 0x12222222, 0x12244222, 0x12241222,
    0x12222222, 0x12222222, 0x12212222, 0x12221111,
    0x12222222, 0x01222222, 0x00122222, 0x00011111,
    0x11100000, 0x22211000, 0x22222100, 0x22332210,
    0x22233210, 0x22222221, 0x22244221, 0x22214221,
    0x22222221, 0x22222221, 0x22221221, 0x11112221,
    0x22222221, 0x22222210, 0x22222100, 0x11111000
};

static const u16 bgPalette[5] =
{
    0x0EC6, 0x00E4, 0x00A0, 0x026A, 0x0248
};

static const u16 playerPalette[5] =
{
    0x0000, 0x0002, 0x000E, 0x068E, 0x0EEE
};

static u16 rngState = 0xACE1;
static s16 curH     = 16;
static s16 trend    = 0;
static u16 runLeft  = 0;

static u16 rnd(void)
{
    u16 x = rngState;
    x ^= (u16)(x << 7);
    x ^= (u16)(x >> 9);
    x ^= (u16)(x << 8);
    rngState = x;
    return x;
}

static s16 nextHeight(void)
{
    s16 step, n;

    if (runLeft == 0)
    {
        trend   = (s16)(rnd() % 3) - 1;
        runLeft = 3 + (rnd() & 7);
    }
    runLeft--;

    step = trend;
    n = curH + step;
    if (n < MIN_H || n > MAX_H)
    {
        trend = -trend;
        step  = 0;
    }
    return curH + step;
}

/* Generate world column 'col' and write it into plane B. */
static void genColumn(u32 col)
{
    const u16 mx   = (u16)(col & (MAP_COLS - 1));
    const s16 top  = curH;
    const s16 next = nextHeight();
    u16 surfTile;
    s16 surfRow;
    s16 r;

    if (next == top)
    {
        surfTile = T_GRASS;
        surfRow  = top;
    }
    else if (next < top)
    {
        surfTile = T_SLOPE_UP;
        surfRow  = top - 1;
    }
    else
    {
        surfTile = T_SLOPE_DOWN;
        surfRow  = top;
    }

    {
        vu16 *data = (vu16 *)VDP_DATA_PORT;
        const u16 sky  = ATTR_SKY;
        const u16 dirt = ATTR_DIRT;
        const u16 surf = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, surfTile);

        /* one address setup, step down a column (plane is 64 tiles wide) */
        VDP_setAutoInc(128);
        *(vu32 *)VDP_CTRL_PORT = VDP_WRITE_VRAM_ADDR((u32)VDP_getPlaneAddress(BG_B, mx, 0));

        for (r = 0; r < surfRow; r++) *data = sky;
        *data = surf;
        for (r = surfRow + 1; r < MAP_ROWS; r++) *data = dirt;

        VDP_setAutoInc(2);
    }

    curH = next;
}

int main(bool hardReset)
{
    u32 camX   = 0;
    u32 genCol = 0;
    s16 px = 64, py = 100;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    PAL_setColors(PAL0 * 16, bgPalette, 5, CPU);
    PAL_setColors(PAL1 * 16, playerPalette, 5, CPU);

    VDP_loadTileData(bgTiles, TILE_BG_BASE, BG_TILE_COUNT, CPU);
    VDP_loadTileData(playerTiles, TILE_SPR_BASE, SPR_TILE_COUNT, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_B, 0);

    while (genCol <= AHEAD_COLS)
        genColumn(genCol++);

    VDP_resetSprites();
    VDP_setSpriteFull(0, px + 0x80, py + 0x80, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_SPR_BASE), 0);
    VDP_updateSprites(1, DMA_QUEUE);

    while (TRUE)
    {
        const u16 pad = JOY_readJoypad(JOY_1);
        const s16 oldX = px, oldY = py;

        if (pad & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (pad & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (pad & BUTTON_UP)    py -= PLAYER_SPEED;
        if (pad & BUTTON_DOWN)  py += PLAYER_SPEED;

        if (px < 0)               px = 0;
        if (px > SCREEN_W - 16)   px = SCREEN_W - 16;
        if (py < 0)               py = 0;
        if (py > SCREEN_H - 16)   py = SCREEN_H - 16;

        camX += SCROLL_SPEED;
        VDP_setHorizontalScroll(BG_B, -(s16)(camX & 0x1FF));

        {
            const u32 limit = (camX >> 3) + AHEAD_COLS;
            while (genCol <= limit)
                genColumn(genCol++);
        }

        /* sprite: only touch it when it actually moved */
        if (px != oldX || py != oldY)
        {
            VDP_setSpritePosition(0, px + 0x80, py + 0x80);
            VDP_updateSprites(1, DMA_QUEUE);
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
