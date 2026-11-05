/*
 * main.c - Boss fight demo for Sega Mega Drive / Genesis (SGDK 2.11)
 *
 * - 96x64 boss built from six 32x32 hardware sprites, swaying left/right near the top
 * - 20 missiles (16x16) orbiting the boss in a circle
 * - Player ship (16x16) moves with the d-pad on pad 1 along the bottom
 * - No resource files: every tile is a const u32 array loaded with VDP_loadTileData
 *
 * Sprite budget (H40 / 320x224):
 *   27 sprites total (limit 80). Boss = 6, missiles = 20, player = 1.
 *   Boss covers at most 3 sprites (96 px) on any scanline.
 *   The 20 missiles are spaced evenly on a circle (R = 64), so at most about
 *   5 of them touch any one scanline (the top and bottom of the circle are the worst case).
 *   Worst case is roughly 8 sprites and about 180 px per line, well under the
 *   hardware limits of 20 sprites and 320 px per line, so nothing drops out or flickers.
 *   The player stays below the whole boss/orbit area, so it never shares a line with them.
 *   All sprite updates go through the DMA queue and are flushed in vblank (no tearing).
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Layout / gameplay constants                                               */
/* ------------------------------------------------------------------------- */
#define NUM_MISSILES    20
#define NUM_BOSS_SPR    6

#define SPR_PLAYER      0
#define SPR_MISSILE0    1
#define SPR_BOSS0       (SPR_MISSILE0 + NUM_MISSILES)
#define NUM_SPRITES     (SPR_BOSS0 + NUM_BOSS_SPR)      /* 27 */

#define BOSS_CX         160     /* sway centre X */
#define BOSS_CY         76      /* boss centre Y */
#define SWAY_AMP        40      /* pixels left/right */
#define ORBIT_R         64      /* missile orbit radius */

#define PLAYER_MIN_X    8
#define PLAYER_MAX_X    (320 - 8 - 16)
#define PLAYER_MIN_Y    164
#define PLAYER_MAX_Y    192
#define PLAYER_SPEED    3

/* VRAM tile layout */
#define BG_TILE_BASE    (TILE_USER_INDEX)               /* 2 tiles: star, floor */
#define BOSS_TILE_BASE  (BG_TILE_BASE + 2)              /* 96 tiles */
#define MISSILE_BASE    (BOSS_TILE_BASE + 96)           /* 4 tiles */
#define PLAYER_BASE     (MISSILE_BASE + 4)              /* 4 tiles */

/* ------------------------------------------------------------------------- */
/* Palettes (format 0x0BGR, 4 bits per channel, even values)                 */
/* ------------------------------------------------------------------------- */
static const u16 palBoss[16] =
{
    0x0200, /* 0: backdrop (dark navy) */
    0x0404, /* 1: outline */
    0x0826, /* 2: body */
    0x0C6A, /* 3: highlight */
    0x0EEE, /* 4: white */
    0x000E, /* 5: red */
    0x08CE, /* 6: teeth */
    0, 0, 0, 0, 0, 0, 0,
    0x0444, /* 14: text shade */
    0x0EEE  /* 15: text colour */
};

static const u16 palMissile[16] =
{
    0x0000,
    0x0006, /* dark red outline */
    0x004E, /* orange */
    0x00EE, /* yellow */
    0x0EEE, /* white core */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 palPlayer[16] =
{
    0x0000,
    0x0420, /* outline */
    0x0CA4, /* body */
    0x0EEA, /* highlight */
    0x00E6, /* cockpit */
    0x008E, /* flame */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 palBg[16] =
{
    0x0000,
    0x0EEE, /* bright star */
    0x0866, /* dim star */
    0x0644, /* floor */
    0x0A66, /* floor edge */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------------- */
/* Tile data (8x8, 4bpp, one u32 per row)                                    */
/* ------------------------------------------------------------------------- */

/* Background: [0] star, [1] floor */
static const u32 bgTiles[2][8] =
{
    { 0x00000000, 0x00000000, 0x00010000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x44444444, 0x33333333, 0x33333333, 0x33333333,
      0x33333333, 0x33333333, 0x33333333, 0x33333333 }
};

/* Boss building blocks */
enum
{
    BT_FILL = 0, BT_TOP, BT_BOT, BT_LEFT, BT_RIGHT,
    BT_TL, BT_TR, BT_BL, BT_BR, BT_EYE, BT_MOUTH,
    BT_COUNT
};

static const u32 bossTiles[BT_COUNT][8] =
{
    /* FILL */
    { 0x22222222, 0x22222222, 0x22232222, 0x22222222,
      0x22222222, 0x22222222, 0x22222322, 0x22222222 },
    /* TOP */
    { 0x11111111, 0x33333333, 0x22222222, 0x22222222,
      0x22222222, 0x22222222, 0x22222222, 0x22222222 },
    /* BOT */
    { 0x22222222, 0x22222222, 0x22222222, 0x22222222,
      0x22222222, 0x22222222, 0x22222222, 0x11111111 },
    /* LEFT */
    { 0x13222222, 0x13222222, 0x13222222, 0x13222222,
      0x13222222, 0x13222222, 0x13222222, 0x13222222 },
    /* RIGHT */
    { 0x22222231, 0x22222231, 0x22222231, 0x22222231,
      0x22222231, 0x22222231, 0x22222231, 0x22222231 },
    /* TL */
    { 0x00011111, 0x00133333, 0x01322222, 0x13222222,
      0x13222222, 0x13222222, 0x13222222, 0x13222222 },
    /* TR */
    { 0x11111000, 0x33333100, 0x22222310, 0x22222231,
      0x22222231, 0x22222231, 0x22222231, 0x22222231 },
    /* BL */
    { 0x13222222, 0x13222222, 0x13222222, 0x13222222,
      0x13222222, 0x01222222, 0x00122222, 0x00011111 },
    /* BR */
    { 0x22222231, 0x22222231, 0x22222231, 0x22222231,
      0x22222231, 0x22222210, 0x22222100, 0x11111000 },
    /* EYE */
    { 0x01111110, 0x14444441, 0x14455441, 0x14555541,
      0x14555541, 0x14455441, 0x14444441, 0x01111110 },
    /* MOUTH (teeth) */
    { 0x22222222, 0x11111111, 0x16661666, 0x16661666,
      0x11111111, 0x22222222, 0x22222222, 0x22222222 }
};

/* Boss layout: 12 x 8 tiles = 96 x 64 pixels */
#define F  BT_FILL
#define TP BT_TOP
#define BM BT_BOT
#define LF BT_LEFT
#define RT BT_RIGHT
#define TL BT_TL
#define TR BT_TR
#define BL BT_BL
#define BR BT_BR
#define EY BT_EYE
#define MO BT_MOUTH

static const u8 bossMap[8][12] =
{
    { TL, TP, TP, TP, TP, TP, TP, TP, TP, TP, TP, TR },
    { LF, F,  F,  F,  F,  F,  F,  F,  F,  F,  F,  RT },
    { LF, F,  F,  EY, F,  F,  F,  F,  EY, F,  F,  RT },
    { LF, F,  F,  F,  F,  F,  F,  F,  F,  F,  F,  RT },
    { LF, F,  F,  F,  F,  F,  F,  F,  F,  F,  F,  RT },
    { LF, F,  MO, MO, MO, MO, MO, MO, MO, MO, F,  RT },
    { LF, F,  F,  F,  F,  F,  F,  F,  F,  F,  F,  RT },
    { BL, BM, BM, BM, BM, BM, BM, BM, BM, BM, BM, BR }
};

#undef F
#undef TP
#undef BM
#undef LF
#undef RT
#undef TL
#undef TR
#undef BL
#undef BR
#undef EY
#undef MO

/* Missile 16x16 (tile order for a 2x2 sprite: TL, BL, TR, BR) */
static const u32 missileTiles[4 * 8] =
{
    /* TL */
    0x00000111, 0x00011222, 0x00122222, 0x01222233,
    0x01222333, 0x12223334, 0x12233344, 0x12233444,
    /* BL */
    0x12233444, 0x12233344, 0x12223334, 0x01222333,
    0x01222233, 0x00122222, 0x00011222, 0x00000111,
    /* TR */
    0x11100000, 0x22211000, 0x22222100, 0x33222210,
    0x33322210, 0x43332221, 0x44333221, 0x44433221,
    /* BR */
    0x44433221, 0x44333221, 0x43332221, 0x33322210,
    0x33222210, 0x22222100, 0x22211000, 0x11100000
};

/* Player ship 16x16 (tile order: TL, BL, TR, BR) */
static const u32 playerTiles[4 * 8] =
{
    /* TL */
    0x00000001, 0x00000012, 0x00000012, 0x00000123,
    0x00000124, 0x00000124, 0x00001222, 0x00011223,
    /* BL */
    0x00122223, 0x01222222, 0x12212222, 0x12111222,
    0x12100122, 0x11100012, 0x00000015, 0x00000005,
    /* TR */
    0x10000000, 0x21000000, 0x31000000, 0x32100000,
    0x42100000, 0x42100000, 0x22210000, 0x32211000,
    /* BR */
    0x32222100, 0x22222210, 0x22221221, 0x22211121,
    0x22100121, 0x21000111, 0x51000000, 0x50000000
};

/* ------------------------------------------------------------------------- */
/* Setup helpers                                                             */
/* ------------------------------------------------------------------------- */
static void loadGraphics(void)
{
    u16 s, c, r;

    PAL_setPalette(PAL0, palBoss, CPU);
    PAL_setPalette(PAL1, palMissile, CPU);
    PAL_setPalette(PAL2, palPlayer, CPU);
    PAL_setPalette(PAL3, palBg, CPU);

    VDP_loadTileData((const u32*) bgTiles, BG_TILE_BASE, 2, CPU);

    /*
     * Each 32x32 hardware sprite is 4x4 tiles stored column-major:
     * tile index = base + column * 4 + row.
     * Build each from the boss tile map.
     */
    for (s = 0; s < NUM_BOSS_SPR; s++)
    {
        u16 sx = s % 3;
        u16 sy = s / 3;
        for (c = 0; c < 4; c++)
        {
            for (r = 0; r < 4; r++)
            {
                u8 id = bossMap[sy * 4 + r][sx * 4 + c];
                VDP_loadTileData(bossTiles[id],
                                 BOSS_TILE_BASE + s * 16 + c * 4 + r, 1, CPU);
            }
        }
    }

    VDP_loadTileData(missileTiles, MISSILE_BASE, 4, CPU);
    VDP_loadTileData(playerTiles, PLAYER_BASE, 4, CPU);
}

static void drawBackground(void)
{
    u16 i;
    u16 seed = 0x1234;
    u16 starAttr = TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, BG_TILE_BASE + 0);
    u16 floorAttr = TILE_ATTR_FULL(PAL3, FALSE, FALSE, FALSE, BG_TILE_BASE + 1);

    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(BG_A, TRUE);

    for (i = 0; i < 40; i++)
    {
        u16 x, y;
        seed = seed * 25173 + 13849;
        x = (seed >> 5) % 40;
        seed = seed * 25173 + 13849;
        y = (seed >> 5) % 25;
        VDP_setTileMapXY(BG_B, starAttr, x, y);
    }

    VDP_fillTileMapRect(BG_B, floorAttr, 0, 26, 40, 2);
    VDP_drawText("D-PAD: MOVE", 1, 27);
}

/* Link every sprite to the next one; the last one ends the list. */
static void initSpriteChain(void)
{
    u16 i;
    VDP_resetSprites();
    for (i = 0; i < NUM_SPRITES; i++)
    {
        VDP_setSpriteFull(i, 0, -32, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, MISSILE_BASE),
                          (i == NUM_SPRITES - 1) ? 0 : (i + 1));
    }
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */
int main(bool hardReset)
{
    s16 playerX = 160 - 8;
    s16 playerY = PLAYER_MAX_Y;
    u16 swayAngle = 0;
    u16 orbitAngle = 0;
    u16 i;

    if (!hardReset) SYS_hardReset();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    VDP_setEnable(FALSE);
    loadGraphics();
    drawBackground();
    initSpriteChain();
    VDP_setEnable(TRUE);

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        s16 bossX;
        u16 s;

        /* ---- player input ---- */
        if (joy & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (joy & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  playerY += PLAYER_SPEED;

        if (playerX < PLAYER_MIN_X) playerX = PLAYER_MIN_X;
        if (playerX > PLAYER_MAX_X) playerX = PLAYER_MAX_X;
        if (playerY < PLAYER_MIN_Y) playerY = PLAYER_MIN_Y;
        if (playerY > PLAYER_MAX_Y) playerY = PLAYER_MAX_Y;

        /* ---- boss sway & missile orbit ---- */
        swayAngle = (swayAngle + 3) & 1023;
        orbitAngle = (orbitAngle + 6) & 1023;

        bossX = BOSS_CX + F16_toRoundedInt(F16_mul(sinFix16(swayAngle), FIX16(SWAY_AMP)));

        /* ---- sprite 0: player (drawn on top of everything) ---- */
        VDP_setSpriteFull(SPR_PLAYER, playerX, playerY, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, PLAYER_BASE),
                          SPR_MISSILE0);

        /* ---- sprites 1..20: missiles evenly spaced on a circle ---- */
        for (i = 0; i < NUM_MISSILES; i++)
        {
            u16 a = (orbitAngle + (u16) ((i * 1024UL) / NUM_MISSILES)) & 1023;
            s16 mx = bossX + F16_toRoundedInt(F16_mul(cosFix16(a), FIX16(ORBIT_R))) - 8;
            s16 my = BOSS_CY + F16_toRoundedInt(F16_mul(sinFix16(a), FIX16(ORBIT_R))) - 8;

            VDP_setSpriteFull(SPR_MISSILE0 + i, mx, my, SPRITE_SIZE(2, 2),
                              TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, MISSILE_BASE),
                              SPR_MISSILE0 + i + 1);
        }

        /* ---- sprites 21..26: boss (3x2 grid of 32x32), behind missiles ---- */
        for (s = 0; s < NUM_BOSS_SPR; s++)
        {
            s16 bx = bossX - 48 + (s % 3) * 32;
            s16 by = BOSS_CY - 32 + (s / 3) * 32;

            VDP_setSpriteFull(SPR_BOSS0 + s, bx, by, SPRITE_SIZE(4, 4),
                              TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, BOSS_TILE_BASE + s * 16),
                              (s == NUM_BOSS_SPR - 1) ? 0 : (SPR_BOSS0 + s + 1));
        }

        /* ---- push the sprite table during vblank ---- */
        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
