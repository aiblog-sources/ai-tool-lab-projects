/*
 * main.c - Procedural hill scroller for the Sega Mega Drive (SGDK 2.09)
 *
 * Plane B shows procedurally generated hills that scroll left forever.
 * Each new tile column is generated as it is about to enter the screen.
 * A 16x16 player sprite moves with the D-pad on pad 1.
 *
 * No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData.
 *
 * Performance notes:
 *  - A tile column is written straight to the VDP data port with the
 *    auto-increment set to one plane row (128 bytes): 1 address set +
 *    16 long writes, instead of three VDP_fillTileMapRect calls.
 *  - Column contents come from a precomputed 60-entry strip
 *    (28 sky, 1 grass, 31 dirt); a column is a 32-entry window into it.
 *  - The sprite table is only re-uploaded when the player moves.
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Tile data (4bpp, 8 rows per tile, leftmost pixel in the high nibble)       */
/* ------------------------------------------------------------------------- */

/* Background palette (PAL0):
 * 0 = sky, 1 = light grass, 2 = dark grass, 3 = dirt, 4 = dark dirt */

static const u32 tileSky[8] =
{
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

static const u32 tileGrass[8] =
{
    0x11111111,
    0x11111111,
    0x21121112,
    0x22222222,
    0x32232223,
    0x33333333,
    0x33433343,
    0x33333333
};

static const u32 tileDirt[8] =
{
    0x33333333,
    0x33433333,
    0x33333343,
    0x34333333,
    0x33333333,
    0x33334333,
    0x43333333,
    0x33333433
};

/* Player sprite palette (PAL1):
 * 0 = transparent, 1 = yellow, 2 = red, 3 = white, 4 = black
 * 2x2 sprite: tiles are stored column-major (TL, BL, TR, BR). */
static const u32 tilePlayer[4 * 8] =
{
    /* top-left */
    0x00000444, 0x00044111, 0x00411111, 0x04113311,
    0x04133111, 0x41131111, 0x41111111, 0x41111111,
    /* bottom-left */
    0x41111111, 0x41121111, 0x41112111, 0x04111222,
    0x04111111, 0x00411111, 0x00044111, 0x00000444,
    /* top-right */
    0x44400000, 0x11144000, 0x11111400, 0x14114140,
    0x14114140, 0x14114114, 0x11111114, 0x11111114,
    /* bottom-right */
    0x11111114, 0x11111214, 0x11112114, 0x22221140,
    0x11111140, 0x11111400, 0x11144000, 0x44400000
};

/* ------------------------------------------------------------------------- */
/* VRAM tile indexes                                                          */
/* ------------------------------------------------------------------------- */

#define TILE_SKY        (TILE_USER_INDEX + 0)
#define TILE_GRASS      (TILE_USER_INDEX + 1)
#define TILE_DIRT       (TILE_USER_INDEX + 2)
#define TILE_PLAYER     (TILE_USER_INDEX + 3)   /* 4 tiles */

#define ATTR_SKY        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_SKY)
#define ATTR_GRASS      TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_GRASS)
#define ATTR_DIRT       TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_DIRT)

/* ------------------------------------------------------------------------- */
/* Constants                                                                  */
/* ------------------------------------------------------------------------- */

#define SCREEN_W        320
#define SCREEN_H        224
#define SCREEN_ROWS     28          /* 224 / 8 */
#define PLANE_W         64          /* plane width in tiles  */
#define PLANE_H         32          /* plane height in tiles */
#define PLANE_W_MASK    (PLANE_W - 1)
#define PLANE_PIX_MASK  ((PLANE_W * 8) - 1)
#define PLANE_ROW_BYTES (PLANE_W * 2)

#define SCROLL_SPEED    1           /* pixels per frame */
#define COLS_AHEAD      41          /* generate up to this many columns past scroll col */

#define MIN_HEIGHT      3           /* hill height in tiles (from bottom of screen) */
#define MAX_HEIGHT      16

#define PLAYER_W        16
#define PLAYER_H        16
#define PLAYER_SPEED    2

/* column strip: SCREEN_ROWS sky, 1 grass, PLANE_H - 1 dirt */
#define STRIP_GRASS     SCREEN_ROWS
#define STRIP_LEN       (SCREEN_ROWS + 1 + (PLANE_H - 1))

/* raw VDP ports */
#define VDP_DATA_L      (*((volatile u32*) 0xC00000))
#define VDP_CTRL_L      (*((volatile u32*) 0xC00004))
#define VDP_DATA_W      (*((volatile u16*) 0xC00000))

#define VRAM_WRITE_CMD(adr) \
    (0x40000000UL | (((u32)(adr) & 0x3FFF) << 16) | ((u32)(adr) >> 14))

/* ------------------------------------------------------------------------- */
/* State                                                                      */
/* ------------------------------------------------------------------------- */

static u32 scrollX;        /* world scroll position in pixels */
static u32 nextColumn;     /* next world column to generate   */
static u16 nextColumnTile; /* scrollX tile column at which nextColumn must be drawn */

static s16 hillHeight;
static s16 hillSlope;
static u16 slopeTimer;

static s16 playerX;
static s16 playerY;
static s16 lastPlayerX;
static s16 lastPlayerY;
static bool spriteDirty;

static u16 planeBAddr;
static u16 hscrollAddr;

/* word-aligned strip of tilemap entries (long reads need only even address) */
static u16 columnStrip[STRIP_LEN] __attribute__((aligned(2)));

/* ------------------------------------------------------------------------- */
/* Hill generation                                                            */
/* ------------------------------------------------------------------------- */

static inline s16 nextHillHeight(void)
{
    if (slopeTimer == 0)
    {
        hillSlope  = (s16)(random() % 3) - 1;   /* -1, 0 or +1 */
        slopeTimer = 2 + (random() & 7);        /* keep slope for 2..9 columns */
    }
    slopeTimer--;

    hillHeight += hillSlope;

    if (hillHeight < MIN_HEIGHT)
    {
        hillHeight = MIN_HEIGHT;
        hillSlope  = 1;
    }
    else if (hillHeight > MAX_HEIGHT)
    {
        hillHeight = MAX_HEIGHT;
        hillSlope  = -1;
    }

    return hillHeight;
}

/* Generate world column 'worldCol' and write it into plane B. */
static void drawColumn(u32 worldCol)
{
    const u16 x   = (u16)(worldCol & PLANE_W_MASK);
    const s16 h   = nextHillHeight();
    /* top = SCREEN_ROWS - h ; window starts at STRIP_GRASS - top = h */
    const u32 *src = (const u32*) &columnStrip[h];
    const u32 cmd  = VRAM_WRITE_CMD(planeBAddr + (x << 1));
    volatile u32 *data = &VDP_DATA_L;

    SYS_disableInts();

    VDP_setAutoInc(PLANE_ROW_BYTES);
    VDP_CTRL_L = cmd;

    /* 32 tilemap entries = 16 long writes, fully unrolled */
    *data = src[0];  *data = src[1];  *data = src[2];  *data = src[3];
    *data = src[4];  *data = src[5];  *data = src[6];  *data = src[7];
    *data = src[8];  *data = src[9];  *data = src[10]; *data = src[11];
    *data = src[12]; *data = src[13]; *data = src[14]; *data = src[15];

    VDP_setAutoInc(2);

    SYS_enableInts();
}

/* Make sure every column that is (or is about to be) visible exists. */
static inline void generateColumns(void)
{
    const u32 target = (scrollX >> 3) + COLS_AHEAD;

    while (nextColumn <= target)
    {
        drawColumn(nextColumn);
        nextColumn++;
    }
}

/* ------------------------------------------------------------------------- */
/* Player                                                                     */
/* ------------------------------------------------------------------------- */

static inline void updatePlayer(void)
{
    const u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
    if (pad & BUTTON_RIGHT) playerX += PLAYER_SPEED;
    if (pad & BUTTON_UP)    playerY -= PLAYER_SPEED;
    if (pad & BUTTON_DOWN)  playerY += PLAYER_SPEED;

    if (playerX < 0) playerX = 0;
    if (playerX > SCREEN_W - PLAYER_W) playerX = SCREEN_W - PLAYER_W;
    if (playerY < 0) playerY = 0;
    if (playerY > SCREEN_H - PLAYER_H) playerY = SCREEN_H - PLAYER_H;

    /* only touch the sprite table when something changed */
    if (spriteDirty || (playerX != lastPlayerX) || (playerY != lastPlayerY))
    {
        /* single hardware sprite, link 0 terminates the sprite list */
        VDP_setSpriteFull(0, playerX, playerY, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER), 0);
        VDP_updateSprites(1, DMA_QUEUE);

        lastPlayerX = playerX;
        lastPlayerY = playerY;
        spriteDirty = FALSE;
    }
}

/* ------------------------------------------------------------------------- */
/* Main                                                                       */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    u16 i;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setPlaneSize(PLANE_W, PLANE_H, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
    VDP_setBackgroundColor(0);

    /* palettes */
    PAL_setColor(0,  0x0EA6);   /* sky (light blue)  */
    PAL_setColor(1,  0x02C2);   /* light grass       */
    PAL_setColor(2,  0x0080);   /* dark grass        */
    PAL_setColor(3,  0x0248);   /* dirt (brown)      */
    PAL_setColor(4,  0x0124);   /* dark dirt         */

    PAL_setColor(16 + 1, 0x00EE);   /* yellow */
    PAL_setColor(16 + 2, 0x000E);   /* red    */
    PAL_setColor(16 + 3, 0x0EEE);   /* white  */
    PAL_setColor(16 + 4, 0x0000);   /* black  */

    /* tiles */
    VDP_loadTileData(tileSky,    TILE_SKY,    1, CPU);
    VDP_loadTileData(tileGrass,  TILE_GRASS,  1, CPU);
    VDP_loadTileData(tileDirt,   TILE_DIRT,   1, CPU);
    VDP_loadTileData(tilePlayer, TILE_PLAYER, 4, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    /* sprites */
    VDP_resetSprites();

    /* cached VRAM addresses */
    planeBAddr  = VDP_getBGBAddress();
    hscrollAddr = VDP_getHScrollTableAddress();

    /* precomputed column strip */
    for (i = 0; i < STRIP_GRASS; i++) columnStrip[i] = ATTR_SKY;
    columnStrip[STRIP_GRASS] = ATTR_GRASS;
    for (i = STRIP_GRASS + 1; i < STRIP_LEN; i++) columnStrip[i] = ATTR_DIRT;

    /* generator init */
    setRandomSeed(0x1D2B);
    hillHeight = 8;
    hillSlope  = 0;
    slopeTimer = 0;

    scrollX    = 0;
    nextColumn = 0;
    nextColumnTile = 0;
    generateColumns();

    VDP_setHorizontalScroll(BG_B, 0);
    VDP_setVerticalScroll(BG_B, 0);

    playerX = 48;
    playerY = (SCREEN_H - PLAYER_H) / 2;
    lastPlayerX = playerX;
    lastPlayerY = playerY;
    spriteDirty = TRUE;

    {
        /* hscroll entry for plane B (HSCROLL_PLANE): word at hscroll + 2 */
        const u32 hsCmd = VRAM_WRITE_CMD(hscrollAddr + 2);

        while (TRUE)
        {
            /* advance scroll and push it to the VDP (plane wraps every 512 px) */
            scrollX += SCROLL_SPEED;

            SYS_disableInts();
            VDP_CTRL_L = hsCmd;
            VDP_DATA_W = (u16)(-(s16)(scrollX & PLANE_PIX_MASK));
            SYS_enableInts();

            /* create any column about to scroll in on the right */
            generateColumns();

            /* player movement + sprite update */
            updatePlayer();

            SYS_doVBlankProcess();
        }
    }

    return 0;
}
