/*
 * main.c - SGDK 2.11 - Truck convoy on a road (Mega Drive / Genesis)
 *
 * Design note (important for real hardware):
 *   14 trucks x 32 px = 448 px on a single scanline. The VDP can only draw
 *   320 sprite pixels (H40) / 20 sprites per scanline, so 14 sprite trucks
 *   on one row would make sprites drop out and flicker on a real console.
 *   The convoy is therefore drawn on plane A (tiles) and moved with the
 *   hardware horizontal scroll. The plane is 64 tiles (512 px) wide, so the
 *   convoy wraps seamlessly when a truck leaves the right edge. Only the
 *   player's car is a hardware sprite (1 sprite, no limits to worry about).
 *
 * Controls: D-pad on pad 1 moves the car.
 * No resource files: all tiles are const u32 arrays (1 nibble = 1 pixel).
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */
#define NUM_TRUCKS      14
#define TRUCK_TILES_W   4               /* 32 px */
#define TRUCK_TILES_H   2               /* 16 px */
#define TRUCK_ROW       13              /* tile row of the convoy (y = 104) */
#define CONVOY_SPEED    2               /* px per frame */
#define PLANE_PIXELS_W  512             /* 64 tiles */

#define CAR_SPEED       2
#define CAR_W           16
#define CAR_H           16

/* VRAM tile indices */
#define IDX_TRUCK       (TILE_USER_INDEX)           /* 8 tiles */
#define IDX_ROAD        (IDX_TRUCK + 8)
#define IDX_LINE        (IDX_ROAD + 1)
#define IDX_GRASS       (IDX_LINE + 1)
#define IDX_SHOULDER    (IDX_GRASS + 1)
#define IDX_CAR         (IDX_SHOULDER + 1)          /* 4 tiles */

/* ------------------------------------------------------------------ */
/* Palettes (format 0x0BGR)                                           */
/* ------------------------------------------------------------------ */
/* PAL0: background + player car */
static const u16 palBackground[16] =
{
    0x0EC8,     /* 0: sky (backdrop colour) */
    0x0444,     /* 1: asphalt */
    0x0888,     /* 2: shoulder */
    0x00EE,     /* 3: yellow */
    0x00A0,     /* 4: grass */
    0x0EEE,     /* 5: white */
    0x000E,     /* 6: red (car body) */
    0x0222      /* 7: dark outline */
};

/* PAL1..PAL3: trucks, only colour 2 (body) differs */
static const u16 palTruck1[16] =
{
    0x0000, 0x0222, 0x002E, 0x0EC8, 0x0000, 0x0AAA, 0x00EE, 0x0EEE
};
static const u16 palTruck2[16] =
{
    0x0000, 0x0222, 0x0E40, 0x0EC8, 0x0000, 0x0AAA, 0x00EE, 0x0EEE
};
static const u16 palTruck3[16] =
{
    0x0000, 0x0222, 0x00C4, 0x0EC8, 0x0000, 0x0AAA, 0x00EE, 0x0EEE
};

/* ------------------------------------------------------------------ */
/* Tile art. Each row is written as 8 hex digits = 8 pixels.          */
/* ------------------------------------------------------------------ */

/* Truck 32x16: 16 pixel rows, each made of 4 u32 (one per tile column) */
static const u32 truckArt[16][4] =
{
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x01111111, 0x11111111, 0x11111111, 0x00000000 },
    { 0x01222222, 0x22222222, 0x22222221, 0x00000000 },
    { 0x01222222, 0x22222222, 0x22222221, 0x01111100 },
    { 0x01222222, 0x22222222, 0x22222221, 0x12333310 },
    { 0x01222222, 0x22222222, 0x22222221, 0x12333310 },
    { 0x01777777, 0x77777777, 0x77777771, 0x12333310 },
    { 0x01222222, 0x22222222, 0x22222221, 0x12222221 },
    { 0x01222222, 0x22222222, 0x22222221, 0x12222226 },
    { 0x01222222, 0x22222222, 0x22222221, 0x12222221 },
    { 0x01111111, 0x11111111, 0x11111111, 0x11111111 },
    { 0x00004444, 0x00004444, 0x00000000, 0x00444400 },
    { 0x00045555, 0x40045555, 0x40000000, 0x04555540 },
    { 0x00045555, 0x40045555, 0x40000000, 0x04555540 },
    { 0x00045555, 0x40045555, 0x40000000, 0x04555540 },
    { 0x00004444, 0x00004444, 0x00000000, 0x00444400 }
};

/* Player car 16x16: 16 pixel rows of 2 u32 */
static const u32 carArt[16][2] =
{
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 },
    { 0x00770000, 0x00007700 },
    { 0x07777777, 0x77777770 },
    { 0x76666666, 0x66666667 },
    { 0x76666555, 0x56666667 },
    { 0x76666555, 0x56666667 },
    { 0x76666666, 0x66666667 },
    { 0x07777777, 0x77777770 },
    { 0x00770000, 0x00007700 },
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000 }
};

/* Background tiles (8 u32 each) */
static const u32 tileRoad[8] =
{
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};
static const u32 tileLine[8] =
{
    0x11111111, 0x11111111, 0x11111111, 0x55555111,
    0x55555111, 0x11111111, 0x11111111, 0x11111111
};
static const u32 tileGrass[8] =
{
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444
};
static const u32 tileShoulder[8] =
{
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222
};

/* Tiles re-ordered at start-up from the pixel-row art above */
static u32 truckTiles[8 * 8];   /* plane order: index = tileRow * 4 + tileCol */
static u32 carTiles[4 * 8];     /* sprite order: column-major (c * 2 + r)     */

static void buildTiles(void)
{
    u16 tr, tc, y;

    for (tr = 0; tr < TRUCK_TILES_H; tr++)
        for (tc = 0; tc < TRUCK_TILES_W; tc++)
            for (y = 0; y < 8; y++)
                truckTiles[(tr * TRUCK_TILES_W + tc) * 8 + y] = truckArt[tr * 8 + y][tc];

    for (tc = 0; tc < 2; tc++)
        for (tr = 0; tr < 2; tr++)
            for (y = 0; y < 8; y++)
                carTiles[(tc * 2 + tr) * 8 + y] = carArt[tr * 8 + y][tc];
}

/* ------------------------------------------------------------------ */
/* Scene setup                                                        */
/* ------------------------------------------------------------------ */
static void drawBackground(void)
{
    /* Plane B: grass / shoulder / road / dashed line / shoulder / grass */
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_GRASS),    0, 11, 64, 4);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_SHOULDER), 0, 15, 64, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_ROAD),     0, 16, 64, 6);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_LINE),     0, 18, 64, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_SHOULDER), 0, 22, 64, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, IDX_GRASS),    0, 23, 64, 5);
}

static void drawConvoy(void)
{
    u16 i, tr, tc;

    /* 14 trucks * 4 tiles = 56 tiles, inside the 64-tile plane.
       The remaining 8 tiles are empty: a gap before the convoy repeats. */
    for (i = 0; i < NUM_TRUCKS; i++)
    {
        u16 pal = PAL1 + (i % 3);
        for (tr = 0; tr < TRUCK_TILES_H; tr++)
            for (tc = 0; tc < TRUCK_TILES_W; tc++)
                VDP_setTileMapXY(BG_A,
                    TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE,
                                   IDX_TRUCK + tr * TRUCK_TILES_W + tc),
                    i * TRUCK_TILES_W + tc, TRUCK_ROW + tr);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    s16 carX = 152;
    s16 carY = 170;
    u16 scroll = 0;
    u16 pad;

    (void)hardReset;

    VDP_setEnable(FALSE);

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    /* Palettes */
    PAL_setPalette(PAL0, palBackground, CPU);
    PAL_setPalette(PAL1, palTruck1, CPU);
    PAL_setPalette(PAL2, palTruck2, CPU);
    PAL_setPalette(PAL3, palTruck3, CPU);
    VDP_setBackgroundColor(0);

    /* Tiles */
    buildTiles();
    VDP_loadTileData(truckTiles,   IDX_TRUCK,    8, CPU);
    VDP_loadTileData(tileRoad,     IDX_ROAD,     1, CPU);
    VDP_loadTileData(tileLine,     IDX_LINE,     1, CPU);
    VDP_loadTileData(tileGrass,    IDX_GRASS,    1, CPU);
    VDP_loadTileData(tileShoulder, IDX_SHOULDER, 1, CPU);
    VDP_loadTileData(carTiles,     IDX_CAR,      4, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    drawBackground();
    drawConvoy();

    VDP_resetSprites();
    VDP_setSpriteFull(0, carX, carY, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, IDX_CAR), 0);
    VDP_updateSprites(1, CPU);

    VDP_setEnable(TRUE);

    while (TRUE)
    {
        /* Wait for VBlank; everything below touches the VDP during VBlank
           only, so there is no mid-frame tearing of scroll or sprite. */
        SYS_doVBlankProcess();

        /* Convoy: move right, wrap at the plane width (512 px) */
        scroll = (scroll + CONVOY_SPEED) & (PLANE_PIXELS_W - 1);
        VDP_setHorizontalScroll(BG_A, scroll);

        /* Player */
        pad = JOY_readJoypad(JOY_1);
        if (pad & BUTTON_LEFT)  carX -= CAR_SPEED;
        if (pad & BUTTON_RIGHT) carX += CAR_SPEED;
        if (pad & BUTTON_UP)    carY -= CAR_SPEED;
        if (pad & BUTTON_DOWN)  carY += CAR_SPEED;

        if (carX < 0) carX = 0;
        if (carX > 320 - CAR_W) carX = 320 - CAR_W;
        if (carY < 0) carY = 0;
        if (carY > 224 - CAR_H) carY = 224 - CAR_H;

        VDP_setSpriteFull(0, carX, carY, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, IDX_CAR), 0);
        VDP_updateSprites(1, CPU);
    }

    return 0;
}
