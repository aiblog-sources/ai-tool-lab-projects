/*
 * main.c - Rain + umbrella for the Sega Mega Drive (SGDK 2.11)
 *
 * - 70 raindrops (8x16 sprites), 5 speed groups, random X on every restart at the top.
 * - Umbrella 48x32, moved with pad 1. The VDP's maximum sprite size is 32x32, so the
 *   umbrella is made of 2 sprites of 24x32 (3x4 tiles). The right half reuses the
 *   same tiles with the H-flip bit set, so only 12 tiles of data are needed.
 * - No resources: all tiles are const u32 arrays, loaded with VDP_loadTileData.
 *
 * Real-hardware limits (H40 / 320 px mode):
 *   - 80 sprites total            -> we use 72
 *   - 20 sprites per scanline     -> see below, we have at most 5 drops + 2 umbrella = 7
 *   - 320 sprite pixels per line  -> at most 7 * ~24 px, well below
 *
 * Why there is no dropout/flicker: the drops are split into 5 groups of 14. Inside a
 * group every drop has the same speed and the vertical spacing is 17 px (>= the 16 px
 * drop height) over a cyclic path of 240 px. Therefore a single group can never put
 * more than 1 drop on the same scanline, no matter what the random X values are.
 * That is a guaranteed maximum of 5 drops per scanline, regardless of the random X.
 */

#include <genesis.h>

#define NUM_DROPS           70
#define DROP_GROUPS         5
#define DROPS_PER_GROUP     (NUM_DROPS / DROP_GROUPS)   /* 14 */
#define DROP_SPACING        17                          /* >= 16 (drop height) */
#define DROP_CYCLE          240                         /* 224 screen + 16 sprite height */

#define SCREEN_W            320
#define SCREEN_H            224

#define UMB_W               48
#define UMB_H               32
#define UMB_SPEED           3
#define UMB_Y_MIN           160
#define UMB_Y_MAX           (SCREEN_H - UMB_H)          /* 192: flush with the bottom */

/* Sprite indexes: lowest index = highest priority, so the umbrella goes first */
#define SPR_UMB_L           0
#define SPR_UMB_R           1
#define SPR_DROP_FIRST      2
#define SPR_TOTAL           (SPR_DROP_FIRST + NUM_DROPS)   /* 72 <= 80 */

/* VRAM tile layout */
#define TILE_DROP           (TILE_USER_INDEX)              /* 2 tiles  */
#define TILE_UMB            (TILE_USER_INDEX + 2)          /* 12 tiles */

/* Palette indexes (PAL0). 0 = transparent / backdrop */
#define COL_RED     1
#define COL_WHITE   2
#define COL_DARK    3
#define COL_BROWN   4
#define COL_RAIN_HI 5
#define COL_RAIN_LO 6

/* ------------------------------------------------------------------------- */
/* Tile data                                                                 */
/* ------------------------------------------------------------------------- */

/* Raindrop 8x16 = 1 column x 2 tiles (sprite tiles are ordered column-major) */
static const u32 dropTiles[2 * 8] =
{
    /* top tile: dim tail */
    0x00000000,
    0x00000000,
    0x00060000,
    0x00060000,
    0x00066000,
    0x00066000,
    0x00066000,
    0x00066000,
    /* bottom tile: bright head */
    0x00055000,
    0x00055000,
    0x00055000,
    0x00055000,
    0x00055000,
    0x00055000,
    0x00050000,
    0x00000000
};

/*
 * Umbrella, LEFT half: 24x32 = 3 columns x 4 rows of tiles, column-major order
 * (col0: 4 tiles top->bottom, col1: 4 tiles, col2: 4 tiles).
 * The right half is drawn with the same tiles + H-flip, centered on the handle.
 * Nibbles: 0 = transparent, 1 = red, 2 = white, 3 = dark outline, 4 = brown handle.
 */
static const u32 umbrellaTiles[12 * 8] =
{
    /* ---- column 0 ---- */
    /* tile 0 (rows 0-7) */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000031, 0x00003111, 0x00031111, 0x00311111,
    /* tile 1 (rows 8-15) */
    0x03111111, 0x03111111, 0x31111111, 0x31111111,
    0x31111111, 0x31111111, 0x31111111, 0x33333333,
    /* tile 2 (rows 16-23) */
    0x33000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* tile 3 (rows 24-31) */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* ---- column 1 ---- */
    /* tile 0 */
    0x00000000, 0x00000000, 0x00033111, 0x32222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    /* tile 1 */
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x33333333,
    /* tile 2 */
    0x33000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* tile 3 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* ---- column 2 (next to the center / handle) ---- */
    /* tile 0 */
    0x00000033, 0x33331111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* tile 1 */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x33333333,
    /* tile 2: handle starts */
    0x33000004, 0x00000004, 0x00000004, 0x00000004,
    0x00000004, 0x00000004, 0x00000004, 0x00000004,
    /* tile 3: handle + foot */
    0x00000004, 0x00000004, 0x00000004, 0x00000004,
    0x00000444, 0x00000444, 0x00000000, 0x00000000
};

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

static s16 dropX[NUM_DROPS];
static s16 dropPos[NUM_DROPS];     /* 0..DROP_CYCLE-1; screen Y = dropPos - 16 */
static u8  dropSpeed[NUM_DROPS];

static s16 umbX;
static s16 umbY;

static const u8 groupSpeed[DROP_GROUPS] = { 2, 3, 4, 5, 6 };

static s16 randomDropX(void)
{
    return (s16) (random() % (SCREEN_W - 8 + 1));   /* 0..312 */
}

static void initPalette(void)
{
    PAL_setColor(0,            RGB24_TO_VDPCOLOR(0x0C1228));   /* night sky backdrop */
    PAL_setColor(COL_RED,      RGB24_TO_VDPCOLOR(0xD02828));
    PAL_setColor(COL_WHITE,    RGB24_TO_VDPCOLOR(0xF0F0F0));
    PAL_setColor(COL_DARK,     RGB24_TO_VDPCOLOR(0x280808));
    PAL_setColor(COL_BROWN,    RGB24_TO_VDPCOLOR(0x985820));
    PAL_setColor(COL_RAIN_HI,  RGB24_TO_VDPCOLOR(0x90C8FF));
    PAL_setColor(COL_RAIN_LO,  RGB24_TO_VDPCOLOR(0x3868C8));
}

static void initSprites(void)
{
    u16 i;

    VDP_resetSprites();

    /* Umbrella: two 24x32 halves (3x4 tiles). Right half is H-flipped. */
    VDP_setSpriteFull(SPR_UMB_L, umbX, umbY, SPRITE_SIZE(3, 4),
                      TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_UMB), SPR_UMB_R);
    VDP_setSpriteFull(SPR_UMB_R, umbX + 24, umbY, SPRITE_SIZE(3, 4),
                      TILE_ATTR_FULL(PAL0, FALSE, FALSE, TRUE, TILE_UMB), SPR_DROP_FIRST);

    /* Raindrops: 8x16 (1x2 tiles) */
    for (i = 0; i < NUM_DROPS; i++)
    {
        VDP_setSpriteFull(SPR_DROP_FIRST + i, dropX[i], dropPos[i] - 16,
                          SPRITE_SIZE(1, 2),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_DROP),
                          SPR_DROP_FIRST + i + 1);
    }

    /* Close the link list: all sprites linked in order, last one links to 0 */
    VDP_linkSprites(0, SPR_TOTAL);
}

static void initRain(void)
{
    u16 i;
    for (i = 0; i < NUM_DROPS; i++)
    {
        u16 g = i / DROPS_PER_GROUP;
        u16 k = i % DROPS_PER_GROUP;

        dropSpeed[i] = groupSpeed[g];
        /* Rotating the whole group by a phase keeps the cyclic spacing intact */
        dropPos[i]   = (s16) ((k * DROP_SPACING + g * 7) % DROP_CYCLE);
        dropX[i]     = randomDropX();
    }
}

static void updateRain(void)
{
    u16 i;
    for (i = 0; i < NUM_DROPS; i++)
    {
        s16 p = dropPos[i] + dropSpeed[i];
        if (p >= DROP_CYCLE)
        {
            p -= DROP_CYCLE;            /* modular wrap keeps the spacing in the group */
            dropX[i] = randomDropX();   /* restart at the top at a random X */
        }
        dropPos[i] = p;
        VDP_setSpritePosition(SPR_DROP_FIRST + i, dropX[i], p - 16);
    }
}

static void updateUmbrella(void)
{
    u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_LEFT)  umbX -= UMB_SPEED;
    if (pad & BUTTON_RIGHT) umbX += UMB_SPEED;
    if (pad & BUTTON_UP)    umbY -= UMB_SPEED;
    if (pad & BUTTON_DOWN)  umbY += UMB_SPEED;

    if (umbX < 0)                  umbX = 0;
    if (umbX > SCREEN_W - UMB_W)   umbX = SCREEN_W - UMB_W;
    if (umbY < UMB_Y_MIN)          umbY = UMB_Y_MIN;
    if (umbY > UMB_Y_MAX)          umbY = UMB_Y_MAX;

    VDP_setSpritePosition(SPR_UMB_L, umbX,      umbY);
    VDP_setSpritePosition(SPR_UMB_R, umbX + 24, umbY);
}

/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    (void) hardReset;

    VDP_setScreenWidth320();     /* H40: 320x224 */
    VDP_setScreenHeight224();

    initPalette();

    VDP_loadTileData(dropTiles,     TILE_DROP, 2,  CPU);
    VDP_loadTileData(umbrellaTiles, TILE_UMB,  12, CPU);

    setRandomSeed(0x5EED);

    umbX = (SCREEN_W - UMB_W) / 2;
    umbY = UMB_Y_MAX;

    initRain();
    initSprites();

    JOY_init();

    while (TRUE)
    {
        updateUmbrella();
        updateRain();

        /* Send the sprite attribute table to VRAM during VBlank (no tearing) */
        VDP_updateSprites(SPR_TOTAL, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
