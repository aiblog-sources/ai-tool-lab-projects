/*
 * main.c - SGDK 2.09 - Sega Mega Drive / Genesis
 *
 * - Plane B: endless procedurally generated hills scrolling left.
 *   Each new column of tiles is generated as it scrolls into view and
 *   written into the 64-tile-wide plane, which wraps around.
 * - Player sprite (16x16) moved with the pad 1 d-pad.
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

#define SCROLL_SPEED    2           /* pixels per frame                */
#define PLAYER_SPEED    2
#define MAP_COLS        64          /* plane B width in tiles (default) */
#define MAP_ROWS        28          /* visible rows (224 / 8)           */
#define SCREEN_W        320
#define SCREEN_H        224
#define AHEAD_COLS      41          /* keep this many columns ahead     */

#define MIN_H           9           /* highest surface row              */
#define MAX_H           22          /* lowest surface row               */

/* Tile indexes in VRAM */
#define TILE_BG_BASE    (TILE_USER_INDEX)
#define T_SKY           (TILE_BG_BASE + 0)
#define T_GRASS         (TILE_BG_BASE + 1)
#define T_DIRT          (TILE_BG_BASE + 2)
#define T_SLOPE_UP      (TILE_BG_BASE + 3)  /* "/"  ground rises going right */
#define T_SLOPE_DOWN    (TILE_BG_BASE + 4)  /* "\"  ground falls going right */
#define BG_TILE_COUNT   5

#define TILE_SPR_BASE   (TILE_BG_BASE + BG_TILE_COUNT)
#define SPR_TILE_COUNT  4

/* ------------------------------------------------------------------ */
/* Tile data (8x8, 4bpp: one u32 per row, leftmost pixel = high nibble)*/
/* ------------------------------------------------------------------ */

static const u32 bgTiles[BG_TILE_COUNT * 8] =
{
    /* T_SKY: fully transparent -> backdrop colour shows through */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* T_GRASS: flat grass top over dirt */
    0x11111111, 0x11111111, 0x22222222, 0x33333333,
    0x33433333, 0x33333343, 0x34333333, 0x33333433,

    /* T_DIRT */
    0x33333333, 0x33343333, 0x33333333, 0x43333333,
    0x33333334, 0x33433333, 0x33333333, 0x33333433,

    /* T_SLOPE_UP "/" */
    0x00000001, 0x00000011, 0x00000112, 0x00001123,
    0x00011233, 0x00112333, 0x01123333, 0x11233333,

    /* T_SLOPE_DOWN "\" */
    0x10000000, 0x11000000, 0x21100000, 0x32110000,
    0x33211000, 0x33321100, 0x33332110, 0x33333211
};

/* Player: 16x16 = 2x2 tiles, VDP order is column-major:
   top-left, bottom-left, top-right, bottom-right */
static const u32 playerTiles[SPR_TILE_COUNT * 8] =
{
    /* top-left */
    0x00000111, 0x00011222, 0x00122222, 0x01223322,
    0x01233222, 0x12222222, 0x12244222, 0x12241222,
    /* bottom-left */
    0x12222222, 0x12222222, 0x12212222, 0x12221111,
    0x12222222, 0x01222222, 0x00122222, 0x00011111,
    /* top-right */
    0x11100000, 0x22211000, 0x22222100, 0x22332210,
    0x22233210, 0x22222221, 0x22244221, 0x22214221,
    /* bottom-right */
    0x22222221, 0x22222221, 0x22221221, 0x11112221,
    0x22222221, 0x22222210, 0x22222100, 0x11111000
};

/* Palettes (format 0x0BGR) */
static const u16 bgPalette[5] =
{
    0x0EC6,     /* 0: sky blue (backdrop) */
    0x00E4,     /* 1: bright grass        */
    0x00A0,     /* 2: dark grass          */
    0x026A,     /* 3: dirt                */
    0x0248      /* 4: dark dirt speckle   */
};

static const u16 playerPalette[5] =
{
    0x0000,     /* 0: transparent */
    0x0002,     /* 1: outline     */
    0x000E,     /* 2: red body    */
    0x068E,     /* 3: highlight   */
    0x0EEE      /* 4: white       */
};

/* ------------------------------------------------------------------ */
/* Terrain generator state                                             */
/* ------------------------------------------------------------------ */

static u16 rngState = 0xACE1;
static s16 curH     = 16;   /* surface row at the left edge of next column */
static s16 trend    = 0;    /* -1 rising, 0 flat, +1 falling               */
static u16 runLeft  = 0;    /* columns left before picking a new trend     */

static u16 rnd(void)
{
    u16 x = rngState;
    x ^= (u16)(x << 7);
    x ^= (u16)(x >> 9);
    x ^= (u16)(x << 8);
    rngState = x;
    return x;
}

/* Choose the surface height at the right edge of the column being built. */
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
        trend = -trend;     /* bounce off the limits */
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

    /* Sky above the surface */
    if (surfRow > 0)
        VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_SKY),
                            mx, 0, 1, (u16)surfRow);

    /* Surface tile */
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, surfTile),
                        mx, (u16)surfRow, 1, 1);

    /* Dirt below the surface */
    if (surfRow + 1 < MAP_ROWS)
        VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_DIRT),
                            mx, (u16)(surfRow + 1), 1, (u16)(MAP_ROWS - surfRow - 1));

    curH = next;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u32 camX   = 0;     /* scroll position in pixels                 */
    u32 genCol = 0;     /* next world column that needs generating   */
    s16 px = 64, py = 100;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    /* Palettes */
    PAL_setColors(PAL0 * 16, bgPalette, 5, CPU);
    PAL_setColors(PAL1 * 16, playerPalette, 5, CPU);

    /* Tiles */
    VDP_loadTileData(bgTiles, TILE_BG_BASE, BG_TILE_COUNT, CPU);
    VDP_loadTileData(playerTiles, TILE_SPR_BASE, SPR_TILE_COUNT, CPU);

    /* Clear both planes, full-plane horizontal scroll (default mode) */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setHorizontalScroll(BG_B, 0);

    /* Pre-fill the visible area plus the look-ahead margin */
    while (genCol <= AHEAD_COLS)
        genColumn(genCol++);

    /* Player sprite (hardware sprite 0). Sprite table coords have a +0x80 offset. */
    VDP_resetSprites();
    VDP_setSpriteFull(0, px + 0x80, py + 0x80, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_SPR_BASE), 0);
    VDP_updateSprites(1, DMA_QUEUE);

    while (TRUE)
    {
        /* ---- input ---- */
        const u16 pad = JOY_readJoypad(JOY_1);
        if (pad & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (pad & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (pad & BUTTON_UP)    py -= PLAYER_SPEED;
        if (pad & BUTTON_DOWN)  py += PLAYER_SPEED;

        if (px < 0)               px = 0;
        if (px > SCREEN_W - 16)   px = SCREEN_W - 16;
        if (py < 0)               py = 0;
        if (py > SCREEN_H - 16)   py = SCREEN_H - 16;

        /* ---- scroll left forever ---- */
        camX += SCROLL_SPEED;
        /* Positive hscroll moves the plane right, so negate to scroll left.
           Plane is 512 px wide, so only the low 9 bits matter. */
        VDP_setHorizontalScroll(BG_B, -(s16)(camX & 0x1FF));

        /* Generate columns as they come into range. The column index wraps
           into the 64-wide map, overwriting columns already off-screen. */
        while (genCol <= (camX >> 3) + AHEAD_COLS)
            genColumn(genCol++);

        /* ---- sprite ---- */
        VDP_setSpritePosition(0, px + 0x80, py + 0x80);
        VDP_updateSprites(1, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
