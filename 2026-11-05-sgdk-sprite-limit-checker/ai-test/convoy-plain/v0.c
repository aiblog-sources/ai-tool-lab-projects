/*
 * main.c - SGDK 2.11 - Mega Drive / Genesis
 *
 * A road with a convoy of 14 trucks (each 32x16 px = one 4x2-tile hardware
 * sprite) driving left to right on the same row. A truck that leaves the
 * right edge wraps to the back of the convoy. Player 1 moves a small car
 * with the d-pad.
 *
 * No resource files: all tiles are const u32 arrays loaded with
 * VDP_loadTileData.
 *
 * Sprite limits (why the convoy is spaced out):
 *   - Mega Drive: max 80 sprites, 20 per scanline, 320 sprite pixels per line.
 *   - 14 trucks x 32 px = 448 px, which would exceed 320 px per line if they
 *     were all on screen at once. The convoy is therefore spaced 40 px apart
 *     (loop length 560 px), so at most 9 trucks are on screen at any time
 *     (9 x 32 = 288 px) plus the 16 px player = 304 px <= 320.
 *   - Trucks outside the screen are hidden (moved off vertically), so they
 *     use no scanline budget.
 *
 * Sprites are driven directly through the VDP sprite table (no rescomp
 * SpriteDefinition needed).
 */

#include <genesis.h>

#define NUM_TRUCKS      14
#define TRUCK_W         32
#define TRUCK_SPACING   40                              /* 32 px truck + 8 px gap */
#define CONVOY_LENGTH   (NUM_TRUCKS * TRUCK_SPACING)    /* 560 px loop            */
#define TRUCK_SPEED     2
#define TRUCK_Y         104                             /* same row for all       */

#define PLAYER_SPEED    2
#define PLAYER_SIZE     16

#define SCREEN_W        320
#define SCREEN_H        224

#define HIDDEN_Y        (-64)

/* Sprite table layout: player first so it is drawn on top of the trucks */
#define SPR_PLAYER      0
#define SPR_TRUCK0      1
#define SPR_COUNT       (1 + NUM_TRUCKS)

/* Tile slots in VRAM */
#define TILE_GRASS      (TILE_USER_INDEX + 0)
#define TILE_ROAD       (TILE_USER_INDEX + 1)
#define TILE_LINE       (TILE_USER_INDEX + 2)
#define TILE_TRUCK      (TILE_USER_INDEX + 3)   /* 8 tiles  */
#define TILE_PLAYER     (TILE_USER_INDEX + 11)  /* 4 tiles  */

/* ---------------------------------------------------------------------- */
/* Tile data (4bpp, one nibble per pixel, 8 u32 per 8x8 tile)              */
/* ---------------------------------------------------------------------- */

/* Background palette (PAL0): 1 = asphalt, 2 = grass, 3 = white line */
static const u32 bgTiles[3 * 8] =
{
    /* grass */
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    /* road */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* road with dashed line */
    0x11111111, 0x11111111, 0x11111111, 0x33333333,
    0x33333333, 0x11111111, 0x11111111, 0x11111111
};

/*
 * Truck palette (PAL1):
 * 1 = trailer white, 2 = trailer edge gray, 3 = cab red,
 * 4 = window light blue, 5 = wheel black, 6 = chassis dark gray
 *
 * Sprite tiles are ordered column-major: for each 8px column, top tile then
 * bottom tile. 4 columns x 2 rows = 8 tiles.
 */
static const u32 truckTiles[8 * 8] =
{
    /* col 0 top (trailer) */
    0x00000000, 0x22222222, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* col 0 bottom (trailer + wheel) */
    0x11111111, 0x11111111, 0x11111111, 0x22222222,
    0x66666666, 0x00555500, 0x05555550, 0x00555500,

    /* col 1 top (trailer) */
    0x00000000, 0x22222222, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* col 1 bottom (trailer, no wheel) */
    0x11111111, 0x11111111, 0x11111111, 0x22222222,
    0x66666666, 0x00000000, 0x00000000, 0x00000000,

    /* col 2 top (trailer) */
    0x00000000, 0x22222222, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* col 2 bottom (trailer + wheel) */
    0x11111111, 0x11111111, 0x11111111, 0x22222222,
    0x66666666, 0x00555500, 0x05555550, 0x00555500,

    /* col 3 top (cab with window, front on the right) */
    0x00000000, 0x00000000, 0x33333333, 0x34444433,
    0x34444433, 0x34444433, 0x33333333, 0x33333333,
    /* col 3 bottom (cab + wheel) */
    0x33333333, 0x33333333, 0x33333333, 0x22222222,
    0x66666666, 0x00555500, 0x05555550, 0x00555500
};

/*
 * Player car palette (PAL2):
 * 1 = yellow body, 3 = cyan window, 5 = wheel black
 * 2x2 tiles, column-major.
 */
static const u32 playerTiles[4 * 8] =
{
    /* col 0 top */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00001111, 0x00013331, 0x00133331, 0x01111111,
    /* col 0 bottom */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11555511, 0x00555500, 0x00555500, 0x00000000,

    /* col 1 top */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x11110000, 0x13331000, 0x13333100, 0x11111110,
    /* col 1 bottom */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11555511, 0x00555500, 0x00555500, 0x00000000
};

/* ---------------------------------------------------------------------- */
/* State                                                                   */
/* ---------------------------------------------------------------------- */

static s16 truckX[NUM_TRUCKS];
static s16 playerX;
static s16 playerY;

/* ---------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ---------------------------------------------------------------------- */

static void setupPalettes(void)
{
    /* PAL0: background */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x101820));
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0x585858));
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x20a040));
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0xe8e8e8));

    /* PAL1: trucks (color 0 transparent) */
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xf0f0f0));
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x909090));
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xd02020));
    PAL_setColor(16 + 4, RGB24_TO_VDPCOLOR(0x80c0ff));
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0x101010));
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0x404040));

    /* PAL2: player */
    PAL_setColor(32 + 1, RGB24_TO_VDPCOLOR(0xf0d020));
    PAL_setColor(32 + 3, RGB24_TO_VDPCOLOR(0x40e0f0));
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0x101010));
}

static void drawBackground(void)
{
    u16 x, y;
    u16 grassAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_GRASS);
    u16 roadAttr  = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_ROAD);
    u16 lineAttr  = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_LINE);

    /* Grass everywhere */
    VDP_fillTileMapRect(BG_B, grassAttr, 0, 0, 40, 28);

    /* Road: tile rows 11..16 (pixels 88..135), trucks run on rows 13-14 */
    VDP_fillTileMapRect(BG_B, roadAttr, 0, 11, 40, 6);

    /* Dashed centre line on tile row 15 (every other tile) */
    y = 15;
    for (x = 0; x < 40; x += 2)
        VDP_setTileMapXY(BG_B, lineAttr, x, y);
}

static void setupSprites(void)
{
    u16 i;
    u16 truckAttr  = TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_TRUCK);
    u16 playerAttr = TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, TILE_PLAYER);

    VDP_resetSprites();

    /* Player: 2x2 tiles (16x16) */
    VDP_setSpriteFull(SPR_PLAYER, playerX, playerY,
                      SPRITE_SIZE(2, 2), playerAttr, SPR_TRUCK0);

    /* Trucks: 4x2 tiles (32x16), linked in a chain; last link = 0 */
    for (i = 0; i < NUM_TRUCKS; i++)
    {
        u8 link = (i == NUM_TRUCKS - 1) ? 0 : (SPR_TRUCK0 + i + 1);
        VDP_setSpriteFull(SPR_TRUCK0 + i, truckX[i], TRUCK_Y,
                          SPRITE_SIZE(4, 2), truckAttr, link);
    }
}

/* ---------------------------------------------------------------------- */
/* Main                                                                    */
/* ---------------------------------------------------------------------- */

int main(bool hardReset)
{
    u16 i;
    u16 pad;

    (void) hardReset;

    VDP_setScreenWidth320();
    setupPalettes();

    /* Load all tiles from the const arrays in code */
    VDP_loadTileData(bgTiles,      TILE_GRASS,  3, CPU);
    VDP_loadTileData(truckTiles,   TILE_TRUCK,  8, CPU);
    VDP_loadTileData(playerTiles,  TILE_PLAYER, 4, CPU);

    drawBackground();

    /* Convoy: truck 0 leads on the right, the others queue behind it */
    for (i = 0; i < NUM_TRUCKS; i++)
        truckX[i] = (SCREEN_W - TRUCK_W) - (s16) (i * TRUCK_SPACING);

    playerX = (SCREEN_W - PLAYER_SIZE) / 2;
    playerY = 168;

    setupSprites();

    while (TRUE)
    {
        /* ---- Input (pad 1) ---- */
        pad = JOY_readJoypad(JOY_1);

        if (pad & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (pad & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (pad & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (pad & BUTTON_DOWN)  playerY += PLAYER_SPEED;

        if (playerX < 0)                         playerX = 0;
        if (playerX > SCREEN_W - PLAYER_SIZE)    playerX = SCREEN_W - PLAYER_SIZE;
        if (playerY < 0)                         playerY = 0;
        if (playerY > SCREEN_H - PLAYER_SIZE)    playerY = SCREEN_H - PLAYER_SIZE;

        VDP_setSpritePosition(SPR_PLAYER, playerX, playerY);

        /* ---- Convoy ---- */
        for (i = 0; i < NUM_TRUCKS; i++)
        {
            truckX[i] += TRUCK_SPEED;

            /* Left the screen on the right: wrap to the back of the convoy */
            if (truckX[i] >= SCREEN_W)
                truckX[i] -= CONVOY_LENGTH;

            /* Only trucks overlapping the screen are shown; the rest are
               parked off-screen vertically so they cost no scanline budget */
            if (truckX[i] > -TRUCK_W)
                VDP_setSpritePosition(SPR_TRUCK0 + i, truckX[i], TRUCK_Y);
            else
                VDP_setSpritePosition(SPR_TRUCK0 + i, 0, HIDDEN_Y);
        }

        /* ---- Present ---- */
        VDP_updateSprites(SPR_COUNT, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
