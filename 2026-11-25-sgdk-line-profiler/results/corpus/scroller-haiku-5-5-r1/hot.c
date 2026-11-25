#include <genesis.h>

/* ---------------- Screen / plane constants ---------------- */
#define SCREEN_W          320
#define SCREEN_H          224
#define SCREEN_TILE_H     (SCREEN_H / 8)   /* 28 tile rows */
#define PLANE_W_TILES     64               /* plane B: 64 x 32 tiles = 512 x 256 px */
#define PLANE_W_PIXELS    (PLANE_W_TILES * 8)

#define SCROLL_SPEED      1                /* background scroll, px per frame */

/* VRAM tile indices */
#define BG_TILE_BASE      1                /* hill tiles 0..9 -> VRAM 1..10 */
#define PLAYER_TILE       20               /* 2x2 player tiles -> VRAM 20..23 */

/* ---------------- Direct VDP access for plane B column writes ---------------- */
#define VDP_DATA_PORT     (*(vu16 *)0xC00000)
#define VDP_CTRL_PORT     (*(vu16 *)0xC00004)
#define VDP_REG_AUTOINC   0x8F00           /* VDP register 15: auto-increment */
#define PLANE_B_VRAM      0xE000UL         /* SGDK default plane B base address */
#define PLANE_ROW_BYTES   (PLANE_W_TILES * 2)

/* ---------------- Background tiles ----------------
 * 4bpp 8x8 tiles: 8 x u32, one u32 per pixel row, leftmost pixel in the
 * high nibble. Colour indices: 0 = sky, 1 = grass, 2 = ground body.
 *
 * Tile k (0..9):
 *   k = 0      : all sky
 *   k = 1..8   : bottom k pixel rows are ground; the top row of that
 *                ground part is grass, the rest is body
 *   k = 9      : solid ground
 */
#define C_SKY     0x00000000UL
#define C_GRASS   0x11111111UL
#define C_BODY    0x22222222UL

const u32 hillTiles[10 * 8] =
{
    /* 0 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY,
    /* 1 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS,
    /* 2 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY,
    /* 3 */ C_SKY, C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY,
    /* 4 */ C_SKY, C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY,
    /* 5 */ C_SKY, C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 6 */ C_SKY, C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 7 */ C_SKY, C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 8 */ C_GRASS, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY,
    /* 9 */ C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY, C_BODY
};

/* ---------------- Player sprite tiles (16x16 = 2x2 tiles) ----------------
 * Hardware sprite tile order is column-major: TL, BL, TR, BR.
 * Colour indices: 0 = transparent, 1 = body, 2 = highlight, 3 = outline.
 */
#define P_OUT      0x33333333UL
#define P_HL_L     0x32222222UL
#define P_BODY_L   0x31111111UL
#define P_HL_R     0x22222223UL
#define P_BODY_R   0x11111113UL

const u32 playerTiles[4 * 8] =
{
    /* TL */ P_OUT,   P_HL_L,   P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L,
    /* BL */ P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_BODY_L, P_OUT,
    /* TR */ P_OUT,   P_HL_R,   P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R,
    /* BR */ P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_BODY_R, P_OUT
};

/* ---------------- Palettes (0x0BGR, 3 bits per channel) ---------------- */
const u16 bgPalette[16] =
{
    0x0EA4,   /* 0: sky blue   */
    0x04E6,   /* 1: grass      */
    0x0282,   /* 2: ground     */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const u16 spritePalette[16] =
{
    0x0000,   /* 0: transparent */
    0x00EE,   /* 1: yellow body */
    0x0EEE,   /* 2: white highlight */
    0x0004,   /* 3: dark red outline */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ---------------- Procedural hill generator ---------------- */
static u16 rngState = 0xACE1;

static u16 rnd(void)
{
    rngState = (u16)(rngState * 25173u + 13849u);
    return (u16)(rngState >> 8);
}

static s16 surfY    = 160;   /* current surface height (px from top of screen) */
static s16 targetY  = 160;   /* surface height we are heading towards */
static u16 holdCols = 0;     /* columns to stay flat before moving again */
static u32 genCol   = 0;     /* next world column (8px) to generate */

static void stepSurface(void)
{
    s16 diff;

    if (holdCols)
    {
        holdCols--;
        return;
    }

    diff = targetY - surfY;
    if (diff == 0)
    {
        /* reached target: choose a new one, maybe hold flat for a while */
        targetY  = (s16)(96 + (rnd() % 96));   /* 96..191 */
        holdCols = rnd() % 24;
        return;
    }

    if (diff > 0)
        surfY += (diff >= 2) ? 2 : 1;
    else
        surfY -= (diff <= -2) ? 2 : 1;
}

/* Writes one column of plane B. A column is 28 tiles strided one map row
 * (128 bytes) apart, so the VDP auto-increment is set to 128 for the burst
 * and restored to 2 afterwards. Interrupts are masked so a VBlank DMA
 * cannot change the VDP write address mid-column. */
static void generateColumn(u32 col)
{
    u16 mapX = (u16)(col & (PLANE_W_TILES - 1));
    u32 addr = PLANE_B_VRAM + (u32)mapX * 2;
    s16 surf = surfY;
    s16 below = (s16)(8 - surf);   /* row 0: row*8+8 - surf */
    u16 row;

    SYS_disableInts();

    VDP_CTRL_PORT = VDP_REG_AUTOINC | PLANE_ROW_BYTES;
    VDP_CTRL_PORT = (u16)(0x4000 | (addr & 0x3FFF));
    VDP_CTRL_PORT = (u16)((addr >> 14) & 3);

    for (row = 0; row < SCREEN_TILE_H; row++)
    {
        /* number of ground pixels inside this tile (<=0 sky, >=9 solid) */
        u16 idx;

        if (below <= 0)
            idx = 0;
        else if (below >= 9)
            idx = 9;
        else
            idx = (u16)below;

        VDP_DATA_PORT = (u16)(BG_TILE_BASE + idx);   /* PAL0, no flips, no priority */
        below += 8;
    }

    VDP_CTRL_PORT = VDP_REG_AUTOINC | 2;

    SYS_enableInts();

    stepSurface();
}

/* Make sure every column up to the right edge of the view (plus a margin) exists */
static void fillAhead(u32 camX)
{
    while ((genCol << 3) < camX + SCREEN_W + 8)
    {
        generateColumn(genCol);
        genCol++;
    }
}

/* ---------------- Main ---------------- */
int main(void)
{
    u32 camX = 0;
    s16 px = 152;
    s16 py = 104;
    s16 oldPx, oldPy;
    u16 joy;
    u16 spriteDirty = TRUE;   /* upload the sprite once at start */

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    VDP_loadTileData(hillTiles,   BG_TILE_BASE, 10, CPU);
    VDP_loadTileData(playerTiles, PLAYER_TILE,   4, CPU);

    PAL_setPalette(PAL0, bgPalette,     CPU);
    PAL_setPalette(PAL1, spritePalette, CPU);

    fillAhead(camX);

    while (TRUE)
    {
        /* player movement */
        joy = JOY_readJoypad(JOY_1);

        oldPx = px;
        oldPy = py;

        if (joy & BUTTON_LEFT)  px -= 2;
        if (joy & BUTTON_RIGHT) px += 2;
        if (joy & BUTTON_UP)    py -= 2;
        if (joy & BUTTON_DOWN)  py += 2;

        if (px < 0)   px = 0;
        if (px > 304) px = 304;   /* 320 - 16 */
        if (py < 0)   py = 0;
        if (py > 208) py = 208;   /* 224 - 16 */

        if (px != oldPx || py != oldPy)
            spriteDirty = TRUE;

        /* sprite table only changes when the player moves */
        if (spriteDirty)
        {
            VDP_setSprite(0, px, py, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, PLAYER_TILE));
            VDP_updateSprites(1, DMA);
            spriteDirty = FALSE;
        }

        /* scroll: camera moves right, so the hills move left.
         * Plane B wraps every 512 px, so only the low bits matter. */
        camX += SCROLL_SPEED;
        fillAhead(camX);
        VDP_setHorizontalScroll(BG_B, -(s16)(camX & (PLANE_W_PIXELS - 1)));

        SYS_doVBlankProcess();
    }

    return 0;
}
