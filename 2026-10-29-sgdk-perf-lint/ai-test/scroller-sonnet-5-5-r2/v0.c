// main.c - Procedural scrolling hills on plane B + d-pad controlled sprite
// SGDK 2.09, no resource files. All tiles are const u32 arrays.

#include <genesis.h>

// ---------------------------------------------------------------------------
// Tile indices in VRAM
// ---------------------------------------------------------------------------
#define TILE_SKY        0                        // blank system tile
#define TILE_GRASS      (TILE_USER_INDEX + 0)
#define TILE_DIRT_A     (TILE_USER_INDEX + 1)
#define TILE_DIRT_B     (TILE_USER_INDEX + 2)
#define TILE_PLAYER     (TILE_USER_INDEX + 3)    // 4 tiles (2x2 sprite)

#define MAP_W           64      // plane width in tiles (must match plane size)
#define MAP_H           32      // plane height in tiles
#define VISIBLE_COLS    40      // 320 / 8

#define HILL_MIN_TOP    12      // highest hill top (tile row)
#define HILL_MAX_TOP    24      // lowest hill top (tile row)

#define PLAYER_W        16
#define PLAYER_H        16
#define PLAYER_SPEED    2

// ---------------------------------------------------------------------------
// Background tile data (8x8, 4bpp: one nibble per pixel)
// Palette 0: 0=sky 1=light green 2=dark green 3=brown 4=dark brown
// ---------------------------------------------------------------------------
static const u32 bgTiles[3 * 8] =
{
    // Grass top
    0x11111111,
    0x11211121,
    0x22222222,
    0x33323332,
    0x33333333,
    0x33433333,
    0x33333343,
    0x33333333,

    // Dirt A
    0x33333333,
    0x33343333,
    0x33333333,
    0x43333333,
    0x33333343,
    0x33333333,
    0x33433333,
    0x33333333,

    // Dirt B
    0x33333333,
    0x33333333,
    0x34333333,
    0x33333333,
    0x33334333,
    0x33333333,
    0x33333334,
    0x33343333
};

// ---------------------------------------------------------------------------
// Player sprite tile data (16x16 = 2x2 tiles, hardware order is column-major:
// top-left, bottom-left, top-right, bottom-right)
// Palette 1: 0=transparent 1=black 2=red 3=white 4=yellow
// ---------------------------------------------------------------------------
static const u32 playerTiles[4 * 8] =
{
    // Top-left
    0x00000111,
    0x00011222,
    0x00122222,
    0x01222222,
    0x01223322,
    0x12233332,
    0x12233132,
    0x12222222,

    // Bottom-left
    0x12222222,
    0x12222444,
    0x01222224,
    0x01222222,
    0x00122222,
    0x00011222,
    0x00001111,
    0x00000000,

    // Top-right
    0x11100000,
    0x22211000,
    0x22222100,
    0x22222210,
    0x22332210,
    0x23333221,
    0x23133221,
    0x22222221,

    // Bottom-right
    0x22222221,
    0x44422221,
    0x42222210,
    0x22222210,
    0x22222100,
    0x22211000,
    0x11110000,
    0x00000000
};

// ---------------------------------------------------------------------------
// Palettes (format 0x0BGR)
// ---------------------------------------------------------------------------
static const u16 pal0Colors[16] =
{
    0x0EC8,     // 0: sky blue (backdrop)
    0x04E4,     // 1: light green
    0x0080,     // 2: dark green
    0x0246,     // 3: brown
    0x0024,     // 4: dark brown
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const u16 pal1Colors[16] =
{
    0x0000,     // 0: transparent
    0x0000,     // 1: black
    0x000E,     // 2: red
    0x0EEE,     // 3: white
    0x00EE,     // 4: yellow
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// ---------------------------------------------------------------------------
// Terrain generator state
// ---------------------------------------------------------------------------
static s16 groundTop;       // current hill top row
static s16 slopeDir;        // -1, 0, +1
static u16 runLeft;         // columns left until a new slope is chosen
static u32 nextGenColumn;   // next absolute column number to generate

// Generate one new column of the world and write it to plane B.
static void generateColumn(u32 column)
{
    s16 newTop;
    u16 x;
    u16 y;

    // Pick a new slope when the current run ends
    if (runLeft == 0)
    {
        slopeDir = (s16) (random() % 3) - 1;
        runLeft = 3 + (random() % 8);
    }
    runLeft--;

    // Move the hill only every other column so slopes are gentler
    if ((runLeft & 1) == 0)
    {
        newTop = groundTop + slopeDir;
        if (newTop < HILL_MIN_TOP || newTop > HILL_MAX_TOP)
        {
            slopeDir = -slopeDir;
            newTop = groundTop + slopeDir;
        }
        groundTop = newTop;
    }

    x = (u16) (column & (MAP_W - 1));

    for (y = 0; y < MAP_H; y++)
    {
        u16 tile;

        if (y < (u16) groundTop)
            tile = TILE_SKY;
        else if (y == (u16) groundTop)
            tile = TILE_GRASS;
        else if ((((u16) column ^ (y * 5)) & 3) == 0)
            tile = TILE_DIRT_B;
        else
            tile = TILE_DIRT_A;

        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile), x, y);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(bool hardReset)
{
    u32 scrollX = 0;
    s16 px = 152;
    s16 py = 100;
    u16 i;

    (void) hardReset;

    // Video setup: 320x224, 64x32 plane, full-plane horizontal scroll
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(MAP_W, MAP_H, TRUE);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    // Palettes and tiles
    PAL_setPalette(PAL0, pal0Colors, CPU);
    PAL_setPalette(PAL1, pal1Colors, CPU);
    VDP_loadTileData(bgTiles, TILE_GRASS, 3, CPU);
    VDP_loadTileData(playerTiles, TILE_PLAYER, 4, CPU);

    // Clear both planes
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    // Terrain init
    setRandomSeed(0x2A5B);
    groundTop = 18;
    slopeDir = 0;
    runLeft = 0;
    nextGenColumn = 0;

    // Fill the whole 64-column plane initially
    for (i = 0; i < MAP_W; i++)
        generateColumn(nextGenColumn++);

    VDP_setHorizontalScroll(BG_B, 0);

    // Hardware sprite setup
    VDP_resetSprites();
    VDP_setSpriteFull(0, px, py, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER), 0);
    VDP_updateSprites(1, DMA_QUEUE);

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);

        // --- Player movement ---
        if (joy & BUTTON_LEFT)  px -= PLAYER_SPEED;
        if (joy & BUTTON_RIGHT) px += PLAYER_SPEED;
        if (joy & BUTTON_UP)    py -= PLAYER_SPEED;
        if (joy & BUTTON_DOWN)  py += PLAYER_SPEED;

        if (px < 0) px = 0;
        if (px > 320 - PLAYER_W) px = 320 - PLAYER_W;
        if (py < 0) py = 0;
        if (py > 224 - PLAYER_H) py = 224 - PLAYER_H;

        // --- Scroll the world left forever ---
        scrollX++;

        // Each time we cross a tile boundary, make sure the column that is
        // about to scroll in on the right (k + 40) has been generated.
        if ((scrollX & 7) == 0)
        {
            u32 needed = (scrollX >> 3) + VISIBLE_COLS;
            while (nextGenColumn <= needed)
                generateColumn(nextGenColumn++);
        }

        // Plane scrolls right in VDP terms (negative value) to move content left
        VDP_setHorizontalScroll(BG_B, -(s16) (scrollX & 0xFFFF));

        // --- Sprite update ---
        VDP_setSpriteFull(0, px, py, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER), 0);
        VDP_updateSprites(1, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
