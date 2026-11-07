/*
 * main.c - SGDK 2.11 platformer background demo (Sega Mega Drive / Genesis)
 *
 * - Plane B: 2048x512 px level (256x64 tiles), generated procedurally.
 *   The VDP plane is only 64x64 tiles (512x512 px), so the level is
 *   streamed in one tile column at a time as the camera moves (the plane
 *   works as a circular buffer horizontally).
 * - Plane A: cloud layer with horizontal parallax per 8-line band
 *   (HSCROLL_TILE mode). Each band has its own camera-parallax factor
 *   plus its own autonomous drift speed.
 * - D-pad scrolls in both directions, accelerating up to 4 px/frame.
 * - No resource files: all tiles are generated in RAM at startup.
 */

#include <genesis.h>

#define LEVEL_W_PX      2048
#define LEVEL_H_PX      512
#define LEVEL_W_TILES   (LEVEL_W_PX / 8)    /* 256 */
#define LEVEL_H_TILES   (LEVEL_H_PX / 8)    /* 64  */

#define SCREEN_W        320
#define SCREEN_H        224
#define BANDS           (SCREEN_H / 8)      /* 28 bands of 8 lines */

#define MAX_CAM_X       (LEVEL_W_PX - SCREEN_W)   /* 1728 */
#define MAX_CAM_Y       (LEVEL_H_PX - SCREEN_H)   /* 288  */
#define MAX_SPEED       4

/* Tile offsets (relative to TILE_USER_INDEX) */
enum
{
    T_GRASS = 0,
    T_DIRT,
    T_STONE,
    T_MTN,
    T_RIDGE,
    T_BRICK,
    T_CLOUD_L,
    T_CLOUD_M,
    T_CLOUD_R,
    T_COUNT
};

#define ATTR_B(t)   TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX + (t))
#define ATTR_A(t)   TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_USER_INDEX + (t))

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */

static u32 tileData[T_COUNT][8];      /* generated 4bpp tiles */

static s16 hscrollA[BANDS];           /* per-band scroll, plane A (clouds) */
static s16 hscrollB[BANDS];           /* per-band scroll, plane B (level)  */
static u32 cloudDrift[BANDS];         /* 24.8 fixed point drift per band   */

static s16 camX, camY;
static s16 colLo, colHi;              /* range of absolute columns loaded in plane B */

/* ------------------------------------------------------------------ */
/* Tile generation                                                     */
/* ------------------------------------------------------------------ */

static void putPix(u32 *t, u16 x, u16 y, u16 c)
{
    u16 sh = (7 - x) * 4;
    t[y] = (t[y] & ~((u32)0xF << sh)) | ((u32)c << sh);
}

static void fillTile(u32 *t, u16 c)
{
    u16 x, y;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            putPix(t, x, y, c);
}

static void buildTiles(void)
{
    static const u8 capStart[8] = { 8, 5, 2, 1, 0, 1, 3, 8 };
    u16 x, y;

    memset(tileData, 0, sizeof(tileData));

    /* Grass: light green top, dark fringe, dirt below */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
        {
            u16 c;
            if (y < 3) c = 1;
            else if (y == 3) c = (x & 1) ? 2 : 1;
            else if (y == 4) c = (x & 1) ? 3 : 2;
            else c = (((x * 7 + y * 3) % 11) == 0) ? 4 : 3;
            putPix(tileData[T_GRASS], x, y, c);
        }

    /* Dirt */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            putPix(tileData[T_DIRT], x, y, (((x * 5 + y * 7) % 13) == 0 || ((x * 3 + y * 5) % 17) == 1) ? 4 : 3);

    /* Stone bricks */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
        {
            u16 c = 5;
            if (y == 3 || y == 7) c = 6;
            else if (y < 3 && x == 0) c = 6;
            else if (y > 3 && x == 4) c = 6;
            else if (y == 0 || y == 4) c = 5;
            putPix(tileData[T_STONE], x, y, c);
        }

    /* Far mountains */
    fillTile(tileData[T_MTN], 7);
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
        {
            u16 c = 7;
            if (y < 2) c = 8;
            else if (y == 2 && (x & 1)) c = 8;
            putPix(tileData[T_RIDGE], x, y, c);
        }

    /* Floating brick platform */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
        {
            u16 c = 9;
            if (y == 0) c = 11;
            else if (y == 3 || y == 7) c = 10;
            else if (y < 3 && x == 0) c = 10;
            else if (y > 3 && x == 4) c = 10;
            putPix(tileData[T_BRICK], x, y, c);
        }

    /* Cloud streak pieces (palette 1: 1 = white, 2 = shade) */
    for (y = 1; y < 7; y++)
        for (x = 0; x < 8; x++)
        {
            u16 c = (y >= 5) ? 2 : 1;

            /* middle: puffy top bumps */
            if (!(y == 1 && x >= 4))
                putPix(tileData[T_CLOUD_M], x, y, c);

            /* left cap */
            if (x >= capStart[y])
                putPix(tileData[T_CLOUD_L], x, y, c);

            /* right cap (mirrored) */
            if (x < 8 - capStart[y])
                putPix(tileData[T_CLOUD_R], x, y, c);
        }

    VDP_loadTileData(&tileData[0][0], TILE_USER_INDEX, T_COUNT, CPU);
}

static void setupPalettes(void)
{
    /* PAL0: level */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x58A8F0));   /* sky / backdrop   */
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0x68D038));   /* grass light      */
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x389020));   /* grass dark       */
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0xB87838));   /* dirt light       */
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0x805020));   /* dirt dark        */
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0x8890A0));   /* stone light      */
    PAL_setColor(6,  RGB24_TO_VDPCOLOR(0x505868));   /* stone dark       */
    PAL_setColor(7,  RGB24_TO_VDPCOLOR(0x7890C8));   /* mountain         */
    PAL_setColor(8,  RGB24_TO_VDPCOLOR(0xB8C8F0));   /* mountain ridge   */
    PAL_setColor(9,  RGB24_TO_VDPCOLOR(0xD87040));   /* brick            */
    PAL_setColor(10, RGB24_TO_VDPCOLOR(0x883818));   /* brick mortar     */
    PAL_setColor(11, RGB24_TO_VDPCOLOR(0xF0A068));   /* brick highlight  */

    /* PAL1: clouds (index 0 transparent) */
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0xB8D0F0));
}

/* ------------------------------------------------------------------ */
/* Level generation (procedural, per tile)                             */
/* ------------------------------------------------------------------ */

static u16 groundRow(u16 tx)
{
    /* One height per 8-column segment (32 segments = 256 columns) */
    static const u8 heights[32] =
    {
        36, 42, 42, 44, 44, 46, 43, 43,
        41, 41, 45, 48, 48, 46, 44, 42,
        42, 40, 40, 43, 47, 49, 49, 47,
        45, 45, 43, 41, 44, 46, 42, 36
    };
    return heights[tx >> 3];
}

static u16 levelTile(u16 tx, u16 ty, u16 g)
{
    u16 p, tri, mt;

    /* Ground and underground */
    if (ty == g)      return ATTR_B(T_GRASS);
    if (ty > g)       return (ty < g + 3) ? ATTR_B(T_DIRT) : ATTR_B(T_STONE);

    /* Floating brick platforms */
    p = tx & 31;
    if (p >= 12 && p <= 17)
    {
        u16 platRow = 34 + ((tx >> 5) % 3) * 2;
        if (ty == platRow) return ATTR_B(T_BRICK);
    }

    /* Far mountains: triangle wave silhouette */
    p = tx & 31;
    tri = (p < 16) ? p : (32 - p);
    mt = 42 - tri;
    if (ty == mt) return ATTR_B(T_RIDGE);
    if (ty > mt)  return ATTR_B(T_MTN);

    return 0;   /* sky: blank tile */
}

static void drawColumn(s16 tx)
{
    u16 ty, g;
    if (tx < 0 || tx >= LEVEL_W_TILES) return;

    g = groundRow(tx);
    for (ty = 0; ty < LEVEL_H_TILES; ty++)
        VDP_setTileMapXY(BG_B, levelTile(tx, ty, g), tx & 63, ty);
}

/* Keep the columns around the camera loaded in the 64-column ring */
static void streamColumns(void)
{
    s16 cx = camX >> 3;
    s16 wantLo = cx - 2;
    s16 wantHi = cx + 42;

    if (wantLo < 0) wantLo = 0;
    if (wantHi > LEVEL_W_TILES - 1) wantHi = LEVEL_W_TILES - 1;

    while (colHi < wantHi)
    {
        colHi++;
        drawColumn(colHi);
        if (colHi - colLo > 63) colLo = colHi - 63;
    }
    while (colLo > wantLo)
    {
        colLo--;
        drawColumn(colLo);
        if (colHi - colLo > 63) colHi = colLo + 63;
    }
}

/* ------------------------------------------------------------------ */
/* Clouds (plane A)                                                    */
/* ------------------------------------------------------------------ */

static void drawStreak(u16 x, u16 y, u16 len)
{
    u16 i;
    for (i = 0; i < len; i++)
    {
        u16 t = (i == 0) ? T_CLOUD_L : ((i == len - 1) ? T_CLOUD_R : T_CLOUD_M);
        VDP_setTileMapXY(BG_A, ATTR_A(t), (x + i) & 63, y);
    }
}

static void drawClouds(void)
{
    u16 r;
    /* One-tile-high streaks on odd rows: each row sits in its own 8-line
       band, so every streak can scroll at its own speed without tearing. */
    for (r = 1; r <= 15; r += 2)
    {
        drawStreak((r * 19) & 63,        r, 4 + (r % 5));
        drawStreak((r * 19 + 32) & 63,   r, 3 + (r % 4));
    }
}

static void updateScrollTables(void)
{
    u16 i;
    for (i = 0; i < BANDS; i++)
    {
        s32 par;
        cloudDrift[i] += (u32)(i + 1) * 8;               /* own wind speed */
        par = (((s32)camX * (s32)(i + 1)) >> 6) + (s32)(cloudDrift[i] >> 8);

        hscrollA[i] = -(s16)par;     /* clouds: slow parallax + drift */
        hscrollB[i] = -camX;         /* level: 1:1 with camera        */
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u16 holdH = 0, holdV = 0;
    s16 c;
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 64, TRUE);
    VDP_setScrollingMode(HSCROLL_TILE, VSCROLL_PLANE);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    buildTiles();
    setupPalettes();
    drawClouds();

    camX = 0;
    camY = 224;
    memset(cloudDrift, 0, sizeof(cloudDrift));

    /* Initial level columns */
    colLo = 0;
    colHi = 42;
    for (c = colLo; c <= colHi; c++) drawColumn(c);

    updateScrollTables();
    VDP_setHorizontalScrollTile(BG_A, 0, hscrollA, BANDS, CPU);
    VDP_setHorizontalScrollTile(BG_B, 0, hscrollB, BANDS, CPU);
    VDP_setVerticalScroll(BG_A, 0);
    VDP_setVerticalScroll(BG_B, camY);

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        s16 dx = 0, dy = 0;
        s16 spdH, spdV;

        /* Acceleration: 1 px/frame up to 4 px/frame while held */
        if (joy & (BUTTON_LEFT | BUTTON_RIGHT)) { if (holdH < 255) holdH++; } else holdH = 0;
        if (joy & (BUTTON_UP | BUTTON_DOWN))    { if (holdV < 255) holdV++; } else holdV = 0;

        spdH = 1 + (holdH >> 3); if (spdH > MAX_SPEED) spdH = MAX_SPEED;
        spdV = 1 + (holdV >> 3); if (spdV > MAX_SPEED) spdV = MAX_SPEED;

        if (joy & BUTTON_LEFT)  dx -= spdH;
        if (joy & BUTTON_RIGHT) dx += spdH;
        if (joy & BUTTON_UP)    dy -= spdV;
        if (joy & BUTTON_DOWN)  dy += spdV;

        camX += dx;
        camY += dy;
        if (camX < 0) camX = 0;
        if (camX > MAX_CAM_X) camX = MAX_CAM_X;
        if (camY < 0) camY = 0;
        if (camY > MAX_CAM_Y) camY = MAX_CAM_Y;

        /* Stream new level columns into plane B */
        streamColumns();

        /* Per-band horizontal scroll (8-line bands) for both planes */
        updateScrollTables();
        VDP_setHorizontalScrollTile(BG_A, 0, hscrollA, BANDS, DMA_QUEUE);
        VDP_setHorizontalScrollTile(BG_B, 0, hscrollB, BANDS, DMA_QUEUE);

        /* Vertical: level follows camera, clouds stay fixed on screen */
        VDP_setVerticalScroll(BG_B, camY);
        VDP_setVerticalScroll(BG_A, 0);

        SYS_doVBlankProcess();
    }

    return 0;
}
