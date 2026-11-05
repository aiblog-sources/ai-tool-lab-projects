/*
 * main.c - SGDK 2.11 shoot-em-up scene (no resource files)
 *
 *  - 3 rows x 12 enemy ships, each one 24x24 px (a single 3x3-tile hardware
 *    sprite), marching left/right together as a formation
 *  - Player ship (16x16, 2x2 tiles) moves along the bottom with the d-pad (pad 1)
 *  - All tile art is stored as const u32 arrays and loaded with VDP_loadTileData
 *
 * Sprite budget: 37 sprites total (limit 80). At most 12 enemy sprites
 * (288 px) share a scanline, within the 20 sprites / 320 px per-line limit
 * of H40 mode. Rows are 32 px apart, so enemy rows never share a scanline.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define ENEMY_COLS      12
#define ENEMY_ROWS      3
#define ENEMY_COUNT     (ENEMY_COLS * ENEMY_ROWS)
#define ENEMY_W         24
#define ENEMY_PITCH_X   24      /* 12 * 24 = 288 px wide formation */
#define ENEMY_PITCH_Y   32
#define FORMATION_Y     24
#define FORMATION_MAX_X 32      /* 320 - 288 px of travel */

#define PLAYER_W        16
#define PLAYER_H        16
#define PLAYER_MIN_Y    150
#define PLAYER_MAX_Y    (224 - PLAYER_H - 8)
#define PLAYER_SPEED    3

#define SPRITE_TOTAL    (ENEMY_COUNT + 1)

/* VRAM tile slots */
#define ENEMY_TILE      (TILE_USER_INDEX)
#define PLAYER_TILE     (ENEMY_TILE + 9)
#define STAR_TILE       (PLAYER_TILE + 4)

/* ------------------------------------------------------------------ */
/* Tile data (4bpp, one nibble per pixel, row-major bitmaps)           */
/* Enemy: 24 rows x 3 u32 (8 px each). Player: 16 rows x 2 u32.        */
/* Converted at load time into the column-major order sprites need.    */
/* ------------------------------------------------------------------ */
static const u32 enemyBitmap[24 * 3] =
{
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00111100, 0x00000000,
    0x00000000, 0x01122110, 0x00000000,
    0x00000000, 0x12322321, 0x00000000,
    0x00000001, 0x22322322, 0x10000000,
    0x00000012, 0x22444422, 0x21000000,
    0x00001122, 0x22444422, 0x22110000,
    0x00122222, 0x23322332, 0x22222100,
    0x11222222, 0x23322332, 0x22222211,
    0x12222222, 0x22222222, 0x22222221,
    0x12322222, 0x22222222, 0x22222321,
    0x11222222, 0x22222222, 0x22222211,
    0x01122222, 0x22222222, 0x22222110,
    0x00112221, 0x12222221, 0x12221100,
    0x00011221, 0x01122110, 0x12211000,
    0x00001110, 0x00111100, 0x01110000,
    0x00000110, 0x00011000, 0x01100000,
    0x00000130, 0x00011000, 0x03100000,
    0x00000030, 0x00000000, 0x03000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000
};

static const u32 playerBitmap[16 * 2] =
{
    0x00000001, 0x10000000,
    0x00000012, 0x21000000,
    0x00000013, 0x31000000,
    0x00000124, 0x42100000,
    0x00000122, 0x22100000,
    0x00001222, 0x22210000,
    0x00011223, 0x32211000,
    0x00122223, 0x32222100,
    0x01222222, 0x22222210,
    0x11222222, 0x22222211,
    0x12221122, 0x22112221,
    0x12110112, 0x21101121,
    0x11000101, 0x10100011,
    0x00000005, 0x50000000,
    0x00000006, 0x60000000,
    0x00000000, 0x00000000
};

/* One 8x8 tile with a single star pixel (colour 1) */
static const u32 starTile[8] =
{
    0x00000000, 0x00000000, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* Palettes (format 0x0BGR) */
static const u16 enemyPalette[5] =
{
    0x0000,     /* 0: transparent */
    0x0200,     /* 1: dark blue outline */
    0x008E,     /* 2: orange body */
    0x00EE,     /* 3: yellow highlight */
    0x0EE0      /* 4: cyan cockpit */
};

static const u16 playerPalette[7] =
{
    0x0000,     /* 0: transparent */
    0x0400,     /* 1: dark outline */
    0x0CCC,     /* 2: light grey hull */
    0x0EEE,     /* 3: white highlight */
    0x0E80,     /* 4: blue cockpit */
    0x004E,     /* 5: red flame */
    0x00EE      /* 6: yellow flame */
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Convert a row-major bitmap (wT x hT tiles) to sprite tile order
 * (column-major) and upload it to VRAM. */
static void loadSpriteBitmap(const u32 *bitmap, u16 wT, u16 hT, u16 tileIndex)
{
    u32 buf[9 * 8];     /* big enough for up to 3x3 tiles */
    u16 tx, ty, y;

    for (tx = 0; tx < wT; tx++)
    {
        for (ty = 0; ty < hT; ty++)
        {
            for (y = 0; y < 8; y++)
            {
                buf[(tx * hT + ty) * 8 + y] = bitmap[(ty * 8 + y) * wT + tx];
            }
        }
    }
    VDP_loadTileData(buf, tileIndex, wT * hT, CPU);
}

static u32 rngState = 0x1234ABCD;
static u16 nextRandom(void)
{
    rngState = rngState * 1103515245UL + 12345UL;
    return (u16)(rngState >> 16);
}

static void drawStarfield(void)
{
    u16 i;
    for (i = 0; i < 60; i++)
    {
        u16 x = nextRandom() % 40;
        u16 y = nextRandom() % 32;
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, STAR_TILE), x, y);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    s16 formationX = FORMATION_MAX_X / 2;
    s16 formationDir = 1;
    u16 frame = 0;
    s16 playerX = (320 - PLAYER_W) / 2;
    s16 playerY = PLAYER_MAX_Y;
    s16 starScroll = 0;
    u16 i;

    (void)hardReset;

    JOY_init();
    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    /* Palettes */
    PAL_setColors(PAL1 * 16, enemyPalette, 5, CPU);
    PAL_setColors(PAL2 * 16, playerPalette, 7, CPU);
    PAL_setColor(1, 0x0888);            /* PAL0 colour 1: star colour */

    /* Tiles */
    loadSpriteBitmap(enemyBitmap, 3, 3, ENEMY_TILE);
    loadSpriteBitmap(playerBitmap, 2, 2, PLAYER_TILE);
    VDP_loadTileData(starTile, STAR_TILE, 1, CPU);

    /* Background */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    drawStarfield();

    /* Sprite list */
    VDP_resetSprites();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 row, col, n = 0;

        /* ---- Player input ---- */
        if (joy & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (joy & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  playerY += PLAYER_SPEED;

        if (playerX < 0) playerX = 0;
        if (playerX > 320 - PLAYER_W) playerX = 320 - PLAYER_W;
        if (playerY < PLAYER_MIN_Y) playerY = PLAYER_MIN_Y;
        if (playerY > PLAYER_MAX_Y) playerY = PLAYER_MAX_Y;

        /* ---- Formation march: 1 px every 2 frames, bounce at the edges ---- */
        frame++;
        if ((frame & 1) == 0)
        {
            formationX += formationDir;
            if (formationX >= FORMATION_MAX_X)
            {
                formationX = FORMATION_MAX_X;
                formationDir = -1;
            }
            else if (formationX <= 0)
            {
                formationX = 0;
                formationDir = 1;
            }
        }

        /* ---- Build the sprite list ---- */
        for (row = 0; row < ENEMY_ROWS; row++)
        {
            for (col = 0; col < ENEMY_COLS; col++)
            {
                s16 x = formationX + col * ENEMY_PITCH_X;
                s16 y = FORMATION_Y + row * ENEMY_PITCH_Y;

                VDP_setSpriteFull(n, x, y,
                                  SPRITE_SIZE(3, 3),
                                  TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, ENEMY_TILE),
                                  n + 1);
                n++;
            }
        }

        /* Player is the last sprite: link 0 terminates the list */
        VDP_setSpriteFull(n, playerX, playerY,
                          SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, PLAYER_TILE),
                          0);
        n++;

        VDP_updateSprites(n, DMA_QUEUE);

        /* ---- Scrolling starfield ---- */
        if ((frame & 3) == 0) starScroll++;
        VDP_setVerticalScroll(BG_B, starScroll);

        (void)i;
        SYS_doVBlankProcess();
    }

    return 0;
}
