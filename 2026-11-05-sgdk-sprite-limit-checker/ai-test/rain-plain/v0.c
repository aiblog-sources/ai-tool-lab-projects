#include <genesis.h>

#define NUM_DROPS       70
#define DROP_W          8
#define DROP_H          16
#define UMB_W           48
#define UMB_H           32
#define UMB_SPEED       3
#define SCREEN_W        320
#define SCREEN_H        224

/* Sprite slots: 0 = umbrella left part (32x32), 1 = umbrella right part (16x32), 2.. = drops */
#define SPR_UMB_A       0
#define SPR_UMB_B       1
#define SPR_DROP_FIRST  2
#define SPR_TOTAL       (SPR_DROP_FIRST + NUM_DROPS)

/* VRAM layout (tile indices) */
#define TILE_DROP       (TILE_USER_INDEX)            /* 2 tiles  */
#define TILE_UMB_A      (TILE_DROP + 2)              /* 16 tiles */
#define TILE_UMB_B      (TILE_UMB_A + 16)            /* 8 tiles  */

/* Palette 0 color indexes: 0 = transparent, 1 = drop, 2/3 = umbrella, 4 = handle */

/* ---- Raindrop: 8x16 = 1 tile wide, 2 tall (column-major: top, bottom) ---- */
static const u32 dropTiles[2 * 8] =
{
    /* top */
    0x00011000, 0x00011000, 0x00011000, 0x00011000,
    0x00011000, 0x00011000, 0x00111100, 0x00111100,
    /* bottom */
    0x00111100, 0x00111100, 0x00111100, 0x00111100,
    0x00111100, 0x00111100, 0x00011000, 0x00000000
};

/* ---- Umbrella building-block tiles ---- */
enum { U_E = 0, U_DL, U_DR, U_A, U_B, U_EA, U_EB, U_PL, U_PR, U_COUNT };

static const u32 umbTiles[U_COUNT][8] =
{
    /* U_E: empty */
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    /* U_DL: diagonal, filled bottom-right */
    { 0x00000002, 0x00000022, 0x00000222, 0x00002222,
      0x00022222, 0x00222222, 0x02222222, 0x22222222 },
    /* U_DR: diagonal, filled bottom-left */
    { 0x20000000, 0x22000000, 0x22200000, 0x22220000,
      0x22222000, 0x22222200, 0x22222220, 0x22222222 },
    /* U_A: solid panel, color 2 */
    { 0x22222222, 0x22222222, 0x22222222, 0x22222222,
      0x22222222, 0x22222222, 0x22222222, 0x22222222 },
    /* U_B: solid panel, color 3 */
    { 0x33333333, 0x33333333, 0x33333333, 0x33333333,
      0x33333333, 0x33333333, 0x33333333, 0x33333333 },
    /* U_EA: scalloped bottom edge, color 2 */
    { 0x22222222, 0x22222222, 0x22222222, 0x22222222,
      0x22222222, 0x22222222, 0x22000022, 0x20000002 },
    /* U_EB: scalloped bottom edge, color 3 */
    { 0x33333333, 0x33333333, 0x33333333, 0x33333333,
      0x33333333, 0x33333333, 0x33000033, 0x30000003 },
    /* U_PL: handle pole, left half */
    { 0x00000044, 0x00000044, 0x00000044, 0x00000044,
      0x00000044, 0x00000044, 0x00000044, 0x00000044 },
    /* U_PR: handle pole, right half */
    { 0x44000000, 0x44000000, 0x44000000, 0x44000000,
      0x44000000, 0x44000000, 0x44000000, 0x44000000 }
};

/* Umbrella layout: 4 rows x 6 columns (48x32 px) */
static const u8 umbMap[4][6] =
{
    { U_E,  U_DL, U_A,  U_B,  U_DR, U_E  },
    { U_DL, U_B,  U_A,  U_B,  U_A,  U_DR },
    { U_EA, U_EB, U_EA, U_EB, U_EA, U_EB },
    { U_E,  U_E,  U_PL, U_PR, U_E,  U_E  }
};

/* RAM buffers for the assembled hardware-sprite tile data */
static u32 umbBufA[16 * 8];   /* 4x4 tiles */
static u32 umbBufB[8 * 8];    /* 2x4 tiles */

static s16 dropX[NUM_DROPS];
static s16 dropY[NUM_DROPS];
static u8  dropSpeed[NUM_DROPS];

static s16 umbX, umbY;

/* Sprite tiles are stored column-major: for each column, top to bottom */
static void buildUmbrellaTiles(void)
{
    u16 c, r, i;
    for (c = 0; c < 6; c++)
    {
        for (r = 0; r < 4; r++)
        {
            const u32 *src = umbTiles[umbMap[r][c]];
            u32 *dst;
            if (c < 4) dst = &umbBufA[(c * 4 + r) * 8];
            else       dst = &umbBufB[((c - 4) * 4 + r) * 8];
            for (i = 0; i < 8; i++) dst[i] = src[i];
        }
    }
}

static void respawnDrop(u16 i, bool initial)
{
    dropX[i] = random() % (SCREEN_W - DROP_W);
    if (initial) dropY[i] = (s16)(random() % (SCREEN_H + DROP_H)) - DROP_H;
    else         dropY[i] = -DROP_H - (s16)(random() % 48);
    dropSpeed[i] = 2 + (random() % 6);   /* 2..7 px per frame */
}

int main(bool hardReset)
{
    u16 i;
    (void)hardReset;

    SYS_disableInts();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette 0 */
    PAL_setColor(0, 0x0420);   /* backdrop: dark blue-gray */
    PAL_setColor(1, 0x0EC6);   /* raindrop: light blue */
    PAL_setColor(2, 0x000E);   /* umbrella red */
    PAL_setColor(3, 0x0EEE);   /* umbrella white */
    PAL_setColor(4, 0x0248);   /* handle brown */

    /* Load tiles from const arrays */
    buildUmbrellaTiles();
    VDP_loadTileData(dropTiles, TILE_DROP, 2, CPU);
    VDP_loadTileData(umbBufA, TILE_UMB_A, 16, CPU);
    VDP_loadTileData(umbBufB, TILE_UMB_B, 8, CPU);

    JOY_init();
    setRandomSeed(0x1234);

    for (i = 0; i < NUM_DROPS; i++) respawnDrop(i, TRUE);

    umbX = (SCREEN_W - UMB_W) / 2;
    umbY = SCREEN_H - UMB_H - 4;

    VDP_resetSprites();

    SYS_enableInts();

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        /* Umbrella movement */
        if (pad & BUTTON_LEFT)  umbX -= UMB_SPEED;
        if (pad & BUTTON_RIGHT) umbX += UMB_SPEED;
        if (pad & BUTTON_UP)    umbY -= UMB_SPEED;
        if (pad & BUTTON_DOWN)  umbY += UMB_SPEED;

        if (umbX < 0) umbX = 0;
        if (umbX > SCREEN_W - UMB_W) umbX = SCREEN_W - UMB_W;
        if (umbY < 96) umbY = 96;
        if (umbY > SCREEN_H - UMB_H) umbY = SCREEN_H - UMB_H;

        /* Rain update */
        for (i = 0; i < NUM_DROPS; i++)
        {
            dropY[i] += dropSpeed[i];

            /* Hit the ground */
            if (dropY[i] >= SCREEN_H)
            {
                respawnDrop(i, FALSE);
                continue;
            }

            /* Hit the umbrella canopy */
            if ((dropX[i] + DROP_W > umbX) && (dropX[i] < umbX + UMB_W) &&
                (dropY[i] + DROP_H >= umbY + 2) && (dropY[i] < umbY + 24))
            {
                respawnDrop(i, FALSE);
            }
        }

        /* Sprites (0x80 = VDP coordinate offset). Umbrella first = drawn in front. */
        VDP_setSpriteFull(SPR_UMB_A, 0x80 + umbX, 0x80 + umbY,
                          SPRITE_SIZE(4, 4),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_UMB_A),
                          SPR_UMB_B);
        VDP_setSpriteFull(SPR_UMB_B, 0x80 + umbX + 32, 0x80 + umbY,
                          SPRITE_SIZE(2, 4),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_UMB_B),
                          SPR_DROP_FIRST);

        for (i = 0; i < NUM_DROPS; i++)
        {
            u16 idx = SPR_DROP_FIRST + i;
            VDP_setSpriteFull(idx, 0x80 + dropX[i], 0x80 + dropY[i],
                              SPRITE_SIZE(1, 2),
                              TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_DROP),
                              (i == NUM_DROPS - 1) ? 0 : (idx + 1));
        }

        VDP_updateSprites(SPR_TOTAL, DMA);
        SYS_doVBlankProcess();
    }

    return 0;
}
