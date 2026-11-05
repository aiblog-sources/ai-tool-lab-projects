#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */
#define SPR_OFFSET      0x80          /* VDP sprite coordinate origin */

#define NUM_MISSILES    20
#define BOSS_COLS       3             /* 3 x 32 = 96 px */
#define BOSS_ROWS       2             /* 2 x 32 = 64 px */
#define BOSS_PIECES     (BOSS_COLS * BOSS_ROWS)

/* Sprite table slots: player, missiles, boss pieces */
#define SLOT_PLAYER     0
#define SLOT_MISSILE    1
#define SLOT_BOSS       (SLOT_MISSILE + NUM_MISSILES)
#define SPRITE_TOTAL    (SLOT_BOSS + BOSS_PIECES)   /* 27 */

/* VRAM tile layout */
#define TILE_BOSS       (TILE_USER_INDEX)
#define TILE_MISSILE    (TILE_BOSS + BOSS_PIECES * 16)
#define TILE_PLAYER     (TILE_MISSILE + 4)

/* Boss / orbit geometry */
#define BOSS_W          96
#define BOSS_H          64
#define BOSS_CENTER_Y   76
#define SWAY_AMPLITUDE  60
#define ORBIT_RADIUS    66            /* clears the boss' corners (~58 px) */

/* Player */
#define PLAYER_SIZE     16
#define PLAYER_SPEED    3
#define PLAYER_MIN_Y    176
#define PLAYER_MAX_Y    (224 - PLAYER_SIZE - 4)

/* ------------------------------------------------------------------ */
/* Palettes (format 0x0BGR)                                            */
/* ------------------------------------------------------------------ */
static const u16 palBoss[16] =
{
    0x0200, 0x0002, 0x0448, 0x066A, 0x000E, 0x0EEE, 0x00EE, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palMissile[16] =
{
    0x0000, 0x0004, 0x024E, 0x08EE, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palPlayer[16] =
{
    0x0000, 0x0820, 0x0EE4, 0x0EEE, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/* ------------------------------------------------------------------ */
/* Tile data (8x8, 4bpp, one nibble per pixel)                         */
/* ------------------------------------------------------------------ */
static const u32 tileBossBorder[8] =
{
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

static const u32 tileBossBody[8] =
{
    0x22222222, 0x22222222, 0x22232222, 0x22222222,
    0x22222222, 0x22222322, 0x22222222, 0x22222222
};

static const u32 tileBossEye[8] =
{
    0x22555522, 0x25555552, 0x55544555, 0x55444455,
    0x55444455, 0x55544555, 0x25555552, 0x22555522
};

static const u32 tileBossMouth[8] =
{
    0x22222222, 0x11111111, 0x16161616, 0x16161616,
    0x11111111, 0x22222222, 0x22222222, 0x22222222
};

/* Top-left quadrant of the 16x16 missile orb (other quadrants mirrored) */
static const u32 tileMissileTL[8] =
{
    0x00000111, 0x00011222, 0x00122222, 0x01223332,
    0x12233322, 0x12232222, 0x12222222, 0x12222222
};

/* Left half of the 16x16 ship (right half mirrored) */
static const u32 tilePlayerTL[8] =
{
    0x00000013, 0x00000123, 0x00001223, 0x00012223,
    0x00122223, 0x01222223, 0x12222223, 0x12222223
};

static const u32 tilePlayerBL[8] =
{
    0x12222223, 0x12222223, 0x01222223, 0x01122223,
    0x00112223, 0x00011223, 0x00001123, 0x00000013
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static u32 flipRowH(u32 row)
{
    u32 out = 0;
    for (u16 i = 0; i < 8; i++)
    {
        out = (out << 4) | (row & 0xF);
        row >>= 4;
    }
    return out;
}

/* Load one tile, optionally mirrored horizontally and/or vertically */
static void loadTile(const u32 *src, u16 index, bool flipH, bool flipV)
{
    u32 t[8];
    for (u16 y = 0; y < 8; y++)
    {
        u32 row = src[flipV ? (7 - y) : y];
        t[y] = flipH ? flipRowH(row) : row;
    }
    VDP_loadTileData(t, index, 1, CPU);
}

static void loadBossTiles(void)
{
    for (u16 row = 0; row < BOSS_ROWS; row++)
    {
        for (u16 col = 0; col < BOSS_COLS; col++)
        {
            u16 base = TILE_BOSS + (row * BOSS_COLS + col) * 16;

            for (u16 tx = 0; tx < 4; tx++)
            {
                for (u16 ty = 0; ty < 4; ty++)
                {
                    u16 gx = col * 4 + tx;      /* 0..11 */
                    u16 gy = row * 4 + ty;      /* 0..7  */
                    const u32 *src = tileBossBody;

                    if (gx == 0 || gx == 11 || gy == 0 || gy == 7) src = tileBossBorder;
                    else if (gy == 3 && (gx == 3 || gx == 8))      src = tileBossEye;
                    else if (gy == 5 && gx >= 4 && gx <= 7)        src = tileBossMouth;

                    /* Sprite tiles are ordered column-major */
                    loadTile(src, base + tx * 4 + ty, FALSE, FALSE);
                }
            }
        }
    }
}

static void loadMissileTiles(void)
{
    /* Column-major: TL, BL, TR, BR */
    loadTile(tileMissileTL, TILE_MISSILE + 0, FALSE, FALSE);
    loadTile(tileMissileTL, TILE_MISSILE + 1, FALSE, TRUE);
    loadTile(tileMissileTL, TILE_MISSILE + 2, TRUE,  FALSE);
    loadTile(tileMissileTL, TILE_MISSILE + 3, TRUE,  TRUE);
}

static void loadPlayerTiles(void)
{
    loadTile(tilePlayerTL, TILE_PLAYER + 0, FALSE, FALSE);
    loadTile(tilePlayerBL, TILE_PLAYER + 1, FALSE, FALSE);
    loadTile(tilePlayerTL, TILE_PLAYER + 2, TRUE,  FALSE);
    loadTile(tilePlayerBL, TILE_PLAYER + 3, TRUE,  FALSE);
}

/* Write one hardware sprite into the cache; link chains to the next slot */
static void putSprite(u16 slot, s16 x, s16 y, u8 size, u16 attr)
{
    u16 link = (slot + 1 < SPRITE_TOTAL) ? (slot + 1) : 0;
    VDP_setSpriteFull(slot, x + SPR_OFFSET, y + SPR_OFFSET, size, attr, link);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    s16 playerX = 160 - PLAYER_SIZE / 2;
    s16 playerY = PLAYER_MAX_Y;
    u16 swayAngle = 0;
    u16 orbitAngle = 0;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setColors(0,  palBoss,    16, CPU);
    PAL_setColors(16, palMissile, 16, CPU);
    PAL_setColors(32, palPlayer,  16, CPU);

    loadBossTiles();
    loadMissileTiles();
    loadPlayerTiles();

    JOY_init();

    VDP_drawText("BOSS FIGHT", 15, 26);

    while (TRUE)
    {
        /* ---------------- Input ---------------- */
        u16 pad = JOY_readJoypad(JOY_1);

        if (pad & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
        if (pad & BUTTON_RIGHT) playerX += PLAYER_SPEED;
        if (pad & BUTTON_UP)    playerY -= PLAYER_SPEED;
        if (pad & BUTTON_DOWN)  playerY += PLAYER_SPEED;

        if (playerX < 0) playerX = 0;
        if (playerX > 320 - PLAYER_SIZE) playerX = 320 - PLAYER_SIZE;
        if (playerY < PLAYER_MIN_Y) playerY = PLAYER_MIN_Y;
        if (playerY > PLAYER_MAX_Y) playerY = PLAYER_MAX_Y;

        /* ---------------- Boss motion ---------------- */
        swayAngle = (swayAngle + 3) & 1023;
        orbitAngle = (orbitAngle + 6) & 1023;

        s16 bossCX = 160 + ((sinFix16(swayAngle) * SWAY_AMPLITUDE) >> FIX16_FRAC_BITS);
        s16 bossCY = BOSS_CENTER_Y;
        s16 bossX = bossCX - BOSS_W / 2;
        s16 bossY = bossCY - BOSS_H / 2;

        /* ---------------- Build sprite list ---------------- */
        putSprite(SLOT_PLAYER, playerX, playerY, SPRITE_SIZE(2, 2),
                  TILE_ATTR_FULL(PAL2, TRUE, FALSE, FALSE, TILE_PLAYER));

        for (u16 i = 0; i < NUM_MISSILES; i++)
        {
            u16 a = (orbitAngle + (i * 1024) / NUM_MISSILES) & 1023;
            s16 mx = bossCX + ((cosFix16(a) * ORBIT_RADIUS) >> FIX16_FRAC_BITS) - 8;
            s16 my = bossCY + ((sinFix16(a) * ORBIT_RADIUS) >> FIX16_FRAC_BITS) - 8;

            putSprite(SLOT_MISSILE + i, mx, my, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_MISSILE));
        }

        for (u16 row = 0; row < BOSS_ROWS; row++)
        {
            for (u16 col = 0; col < BOSS_COLS; col++)
            {
                u16 piece = row * BOSS_COLS + col;
                putSprite(SLOT_BOSS + piece,
                          bossX + col * 32, bossY + row * 32,
                          SPRITE_SIZE(4, 4),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BOSS + piece * 16));
            }
        }

        VDP_updateSprites(SPRITE_TOTAL, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
