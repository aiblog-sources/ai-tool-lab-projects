#include <genesis.h>

/* ------------------------------------------------------------------------ */
/* Layout                                                                   */
/* ------------------------------------------------------------------------ */
#define MAP_W         256                 /* level width in tiles  (2048 px) */
#define MAP_H         64                  /* level height in tiles (512 px)  */
#define MAX_CAM_X     (MAP_W * 8 - 320)   /* 1728 */
#define MAX_CAM_Y     (MAP_H * 8 - 224)   /* 288  */
#define PLANE_TILES   64                  /* plane is 64x64 tiles (512x512)  */
#define CLOUD_ROWS    14                  /* plane A rows that hold clouds   */
#define HS_BANDS      28                  /* 224 lines / 8                   */

/* tile indices inside our tile set */
enum { T_BLANK = 0, T_GRASS, T_DIRT, T_STONE, T_BRICK, T_CL, T_CM, T_CM2, T_CR, T_COUNT };

/* ------------------------------------------------------------------------ */
/* Graphics (all generated from code: 4bpp, one nibble per pixel)           */
/* ------------------------------------------------------------------------ */
static const u32 gfx[T_COUNT * 8] =
{
    /* BLANK */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* GRASS */
    0x01101101, 0x11111111, 0x12111211, 0x33333333,
    0x33433433, 0x33333333, 0x34333343, 0x33333333,
    /* DIRT */
    0x33333333, 0x33433333, 0x33333343, 0x34333333,
    0x33333433, 0x33343333, 0x33333333, 0x43334333,
    /* STONE */
    0x44444444, 0x45555544, 0x45655544, 0x45555544,
    0x44444444, 0x55544555, 0x55644555, 0x44444444,
    /* BRICK (platforms) */
    0x66666666, 0x77767777, 0x77767777, 0x77767777,
    0x66666666, 0x77777677, 0x77777677, 0x77777677,
    /* CLOUD left cap */
    0x00000000, 0x00000000, 0x00011111, 0x00111111,
    0x01111111, 0x00222222, 0x00000000, 0x00000000,
    /* CLOUD middle */
    0x00000000, 0x00000000, 0x11111111, 0x11111111,
    0x11111111, 0x22222222, 0x00000000, 0x00000000,
    /* CLOUD middle (bumpy) */
    0x00000000, 0x01111000, 0x11111111, 0x11111111,
    0x11111111, 0x22222222, 0x00222200, 0x00000000,
    /* CLOUD right cap */
    0x00000000, 0x00000000, 0x11111000, 0x11111100,
    0x11111110, 0x22222200, 0x00000000, 0x00000000,
};

/* colour format 0x0BGR */
static const u16 palGround[16] =
{
    0x0E86,  /* 0 sky (backdrop) */
    0x00A2,  /* 1 grass          */
    0x0060,  /* 2 dark green     */
    0x0048,  /* 3 dirt           */
    0x0026,  /* 4 dark dirt      */
    0x0666,  /* 5 stone          */
    0x0AAA,  /* 6 light gray     */
    0x022C,  /* 7 brick red      */
    0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 palCloud[16] =
{
    0x0000, 0x0EEE, 0x0ECA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------------ */
/* State                                                                    */
/* ------------------------------------------------------------------------ */
static u8  level[MAP_H][MAP_W];          /* 16 KB tile map generated at boot */
static u16 attrB[5];                     /* level value -> tile attribute    */
static u16 colBuf[PLANE_TILES];          /* column waiting to be pushed      */
static s16 pendSlot = -1;                /* plane column slot, -1 = none     */
static s16 hsA[HS_BANDS];
static s16 hsB[HS_BANDS];

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */
static u32 hash32(u32 v)
{
    v *= 0x9E3779B1UL;
    v ^= v >> 16;
    v *= 0x85EBCA6BUL;
    v ^= v >> 13;
    return v;
}

static u16 tri(u16 x, u16 period)
{
    u16 p = x % period;
    u16 half = period >> 1;
    return (p < half) ? p : (u16)(period - p);
}

static void buildLevel(void)
{
    u16 x, y, g, i;

    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            level[y][x] = T_BLANK;

    /* rolling ground */
    for (x = 0; x < MAP_W; x++)
    {
        s16 surf = 34 - (s16)(tri(x, 48) / 3) + (s16)(tri(x + 17, 20) / 4);
        for (y = (u16)surf; y < MAP_H; y++)
        {
            u8 t;
            if (y == (u16)surf)          t = T_GRASS;
            else if (y < (u16)surf + 4)  t = T_DIRT;
            else                         t = (((x + y) & 3) == 0) ? T_DIRT : T_STONE;
            level[y][x] = t;
        }
    }

    /* floating brick platforms, one candidate per 16-column group */
    for (g = 0; g < MAP_W / 16; g++)
    {
        u32 h = hash32(g + 77);
        if ((h & 3) == 0) continue;
        {
            u16 start = g * 16 + ((h >> 2) & 7);
            u16 len   = 3 + ((h >> 5) & 3);
            u16 row   = 8 + ((h >> 8) % 14);          /* rows 8..21 */
            for (i = 0; i < len; i++) level[row][start + i] = T_BRICK;
        }
    }
}

static void buildCloudsPlaneA(u16 base)
{
    u16 r, s, i;
    for (r = 1; r < CLOUD_ROWS; r++)
    {
        for (s = 0; s < 3; s++)
        {
            u32 h = hash32(r * 7 + s * 131 + 5);
            u16 start, len;
            if (((h >> 24) & 3) == 0) continue;
            start = s * 21 + ((h >> 4) & 7);
            len   = 4 + ((h >> 8) % 7);               /* 4..10 tiles */
            for (i = 0; i < len; i++)
            {
                u16 t;
                if (i == 0)             t = T_CL;
                else if (i == len - 1)  t = T_CR;
                else                    t = (((h >> 12) >> i) & 1) ? T_CM2 : T_CM;
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, base + t),
                                 (start + i) & (PLANE_TILES - 1), r);
            }
        }
    }
}

/* fills colBuf with level column x (0..255) */
static void buildColumn(u16 x)
{
    u16 y;
    for (y = 0; y < MAP_H; y++) colBuf[y] = attrB[level[y][x]];
}

static u16 speedFor(u16 hold)
{
    u16 s;
    if (hold == 0) return 0;
    s = 1 + (hold >> 3);          /* ramps 1,2,3,4 px/frame */
    return (s > 4) ? 4 : s;
}

/* ------------------------------------------------------------------------ */
int main(bool hardReset)
{
    s16 camX = 0, camY = 0, prevTile = 0;
    u16 hl = 0, hr = 0, hu = 0, hd = 0;
    u32 frame = 0;
    u16 base, x, y, i;

    (void)hardReset;

    VDP_setEnable(FALSE);
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(PLANE_TILES, PLANE_TILES, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);

    base = TILE_USER_INDEX;
    VDP_loadTileData(gfx, base, T_COUNT, CPU);
    PAL_setPalette(PAL0, palGround, CPU);
    PAL_setPalette(PAL1, palCloud, CPU);

    attrB[0] = TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, base + T_BLANK);
    for (i = 1; i < 5; i++)   /* ground/platforms use priority so they cover clouds */
        attrB[i] = TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, base + i);

    buildLevel();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    /* initial 64 columns of the level into plane B */
    for (x = 0; x < PLANE_TILES; x++)
    {
        buildColumn(x);
        for (y = 0; y < MAP_H; y++)
            VDP_setTileMapXY(BG_B, colBuf[y], x, y);
    }
    buildCloudsPlaneA(base);

    for (i = 0; i < HS_BANDS; i++) { hsA[i] = 0; hsB[i] = 0; }
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, 0);
    VDP_setEnable(TRUE);

    while (TRUE)
    {
        u16 j = JOY_readJoypad(JOY_1);
        s16 nt;

        /* --- input with acceleration (max 4 px/frame per axis) --- */
        hl = (j & BUTTON_LEFT)  ? (hl < 255 ? hl + 1 : hl) : 0;
        hr = (j & BUTTON_RIGHT) ? (hr < 255 ? hr + 1 : hr) : 0;
        hu = (j & BUTTON_UP)    ? (hu < 255 ? hu + 1 : hu) : 0;
        hd = (j & BUTTON_DOWN)  ? (hd < 255 ? hd + 1 : hd) : 0;

        camX += (s16)speedFor(hr) - (s16)speedFor(hl);
        camY += (s16)speedFor(hd) - (s16)speedFor(hu);
        if (camX < 0) camX = 0;
        if (camX > MAX_CAM_X) camX = MAX_CAM_X;
        if (camY < 0) camY = 0;
        if (camY > MAX_CAM_Y) camY = MAX_CAM_Y;

        /* --- column streaming: keep window [tile-8, tile+55] resident --- */
        nt = camX >> 3;
        if (nt != prevTile)                   /* at most one tile per frame */
        {
            s16 col = (nt > prevTile) ? (nt + 55) : (nt - 8);
            prevTile = nt;
            if (col >= 0 && col < MAP_W)
            {
                buildColumn((u16)col);
                pendSlot = col & (PLANE_TILES - 1);
            }
        }

        /* --- per-band horizontal scroll tables (computed outside vblank) --- */
        for (i = 0; i < HS_BANDS; i++)
        {
            hsB[i] = -camX;
            if (i < CLOUD_ROWS)
                hsA[i] = (s16)(-(((s32)camX * (i + 2)) >> 5)
                               - (s32)((frame * (i + 1)) >> 5));
            else
                hsA[i] = 0;
        }
        frame++;

        /* --- wait for vblank, then do the small VDP update right away --- */
        SYS_doVBlankProcess();

        VDP_setHorizontalScrollTile(BG_A, 0, hsA, HS_BANDS, CPU);
        VDP_setHorizontalScrollTile(BG_B, 0, hsB, HS_BANDS, CPU);
        VDP_setVerticalScroll(BG_B, camY);

        if (pendSlot >= 0)
        {
            for (y = 0; y < MAP_H; y++)
                VDP_setTileMapXY(BG_B, colBuf[y], pendSlot, y);
            pendSlot = -1;
        }
    }
    return 0;
}
