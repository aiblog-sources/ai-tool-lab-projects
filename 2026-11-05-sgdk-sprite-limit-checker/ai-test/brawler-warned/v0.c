/*
 * main.c - SGDK 2.11 beat-em-up scene (Sega Mega Drive / Genesis)
 *
 * - 10 enemies + 1 player, each 32x48 px (4x6 tiles), no resource files.
 * - Every tile is a const u32 array, loaded with VDP_loadTileData.
 *
 * Sprite hardware budget (H40, 320x224):
 *   - A 32x48 character needs 2 hardware sprites (4x4 + 4x2 tiles).
 *   - Limits: 80 sprites, 20 sprites/scanline, 320 sprite pixels/scanline.
 *   - 11 characters x 32 px = 352 px, which does not fit on one scanline.
 *     So the scene is a 1280 px wide scrolling street. Each enemy patrols its
 *     own 128 px lane (96 px walk + 32 px body), so at most 4 enemies + the
 *     player (5 x 32 = 160 px/line) are ever on screen. Characters outside
 *     the screen are left out of the sprite list instead of being parked.
 *   - The list is rebuilt compactly every frame, sorted by feet Y, so there
 *     is no flicker, no dropout and no multiplexing.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
#define SCREEN_W        320
#define WORLD_W         1280
#define NUM_ENEMIES     10
#define NUM_CHARS       (NUM_ENEMIES + 1)

#define LANE_W          128                 /* world slice owned by one enemy   */
#define PATROL_LEN      96                  /* LANE_W - 32 px sprite width      */
#define ENEMY_Y         136                 /* sprite top; feet at y = 184      */

#define PLAYER_Y_MIN    100
#define PLAYER_Y_MAX    168
#define PLAYER_SPEED_X  2
#define PLAYER_SPEED_Y  1

/* VRAM layout (tile indices) */
#define BG_TILE_BASE    (TILE_USER_INDEX)           /* 6 background tiles       */
#define TOP_TILE_BASE   (BG_TILE_BASE + 6)          /* 16 tiles: 4x4 upper body */
#define LEGS_TILE_BASE  (TOP_TILE_BASE + 16)        /* 2 frames x 8 tiles: 4x2  */

#define T_SKY1   0
#define T_SKY2   1
#define T_WALL   2
#define T_WIN    3
#define T_FTOP   4
#define T_FLOOR  5

/* ------------------------------------------------------------------ */
/* Palettes (0x0BGR, 3 bits per channel, even values)                  */
/* ------------------------------------------------------------------ */
static const u16 palBackground[16] =
{
    0x0000, 0x0E86, 0x0EC8, 0x0248, 0x0224, 0x0468, 0x0246, 0x0222,
    0x04EE, 0x0222, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/* idx: 0 transp, 1 outline, 2 skin, 3 hair, 4 shirt, 5 pants, 6 shoes, 7 belt, 8 buckle */
static const u16 palPlayer[16] =
{
    0x0000, 0x0222, 0x068E, 0x02CE, 0x022E, 0x0A22, 0x0224, 0x0246,
    0x00EE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palEnemyA[16] =
{
    0x0000, 0x0222, 0x048A, 0x0444, 0x02C2, 0x0446, 0x0222, 0x0224,
    0x00CE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palEnemyB[16] =
{
    0x0000, 0x0222, 0x068E, 0x028E, 0x0A2A, 0x0644, 0x0222, 0x0246,
    0x00EE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

/* ------------------------------------------------------------------ */
/* Background tiles (8x8, 4bpp, one u32 per row)                       */
/* ------------------------------------------------------------------ */
static const u32 tilesBackground[6][8] =
{
    /* SKY1 */
    { 0x11111111, 0x11111111, 0x11111111, 0x11111111,
      0x11111111, 0x11111111, 0x11111111, 0x11111111 },
    /* SKY2 (lighter, near the horizon) */
    { 0x22222222, 0x22222222, 0x22222222, 0x22222222,
      0x22222222, 0x22222222, 0x22222222, 0x22222222 },
    /* WALL (bricks) */
    { 0x44444444, 0x33343333, 0x33343333, 0x33343333,
      0x44444444, 0x33333433, 0x33333433, 0x33333433 },
    /* WINDOW */
    { 0x99999999, 0x98888889, 0x98888889, 0x98888889,
      0x98888889, 0x98888889, 0x98888889, 0x99999999 },
    /* FLOOR TOP (curb line) */
    { 0x77777777, 0x77777777, 0x55555556, 0x55555556,
      0x55555556, 0x55555556, 0x55555556, 0x66666666 },
    /* FLOOR */
    { 0x55555556, 0x55555556, 0x55555556, 0x55555556,
      0x55555556, 0x55555556, 0x55555556, 0x66666666 }
};

/* ------------------------------------------------------------------ */
/* Character tiles. Stored in hardware sprite order: column-major      */
/* (each column top to bottom, columns left to right).                 */
/* Art faces right; facing left uses the sprite H-flip bit.            */
/* ------------------------------------------------------------------ */

/* Upper body: 4 columns x 4 tile rows = 32x32 px (hardware sprite 4x4) */
static const u32 tilesCharTop[16][8] =
{
    /* column 0 (x 0..7) */
    { 0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0 },
    { 0x00000000, 0x00011111, 0x00014444, 0x00014444,
      0x00014444, 0x00014444, 0x00014444, 0x00014444 },
    { 0x00014444, 0x00014444, 0x00014444, 0x00014444,
      0x00012222, 0x00012222, 0x00012221, 0x00001110 },

    /* column 1 (x 8..15) */
    { 0x00000111, 0x00013333, 0x00133333, 0x01333333,
      0x01333333, 0x01333333, 0x01333333, 0x01333333 },
    { 0x01332222, 0x01332222, 0x01332222, 0x01222222,
      0x01222222, 0x00122222, 0x00012222, 0x00012222 },
    { 0x11112222, 0x14444444, 0x14444444, 0x14444444,
      0x14444444, 0x14444444, 0x14444444, 0x14444444 },
    { 0x14444444, 0x14444444, 0x14444444, 0x14444444,
      0x14444444, 0x14444444, 0x17777788, 0x17777788 },

    /* column 2 (x 16..23) */
    { 0x11100000, 0x33310000, 0x33333100, 0x33333310,
      0x33222210, 0x22222221, 0x22222221, 0x22222221 },
    { 0x22222221, 0x22221121, 0x22221121, 0x22222221,
      0x22222221, 0x22222211, 0x22222110, 0x22221000 },
    { 0x22221111, 0x44444441, 0x44444441, 0x44444441,
      0x44444441, 0x44444441, 0x44444441, 0x44444441 },
    { 0x44444441, 0x44444441, 0x44444441, 0x44444441,
      0x44444441, 0x44444441, 0x88777771, 0x88777771 },

    /* column 3 (x 24..31) */
    { 0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0 },
    { 0x00000000, 0x11110000, 0x44441000, 0x44441000,
      0x44441000, 0x44441000, 0x44441000, 0x44441000 },
    { 0x44441000, 0x44441000, 0x44441000, 0x44441000,
      0x22221000, 0x22221000, 0x22210000, 0x11100000 }
};

/* Legs: 4 columns x 2 tile rows = 32x16 px (hardware sprite 4x2) */
static const u32 tilesLegs[2][8][8] =
{
    /* frame 0: standing */
    {
        /* column 0 */
        { 0,0,0,0,0,0,0,0 },
        { 0,0,0,0, 0x00000001, 0x00000001, 0x00000001, 0x00000001 },
        /* column 1 */
        { 0x15555555, 0x15555555, 0x15555551, 0x15555551,
          0x15555551, 0x15555551, 0x15555551, 0x15555551 },
        { 0x15555551, 0x15555551, 0x15555551, 0x15555551,
          0x66666661, 0x66666661, 0x66666661, 0x11111111 },
        /* column 2 */
        { 0x55555551, 0x55555551, 0x15555551, 0x15555551,
          0x15555551, 0x15555551, 0x15555551, 0x15555551 },
        { 0x15555551, 0x15555551, 0x15555551, 0x15555551,
          0x16666666, 0x16666666, 0x16666666, 0x11111111 },
        /* column 3 */
        { 0,0,0,0,0,0,0,0 },
        { 0,0,0,0, 0x66100000, 0x66100000, 0x66100000, 0x11100000 }
    },
    /* frame 1: stride (legs apart) */
    {
        /* column 0 */
        { 0,0,0,0, 0x00000015, 0x00000015, 0x00000015, 0x00000015 },
        { 0x00001555, 0x00001555, 0x00001555, 0x00001555,
          0x00166666, 0x00166666, 0x00166666, 0x00111111 },
        /* column 1 */
        { 0x15555555, 0x15555555, 0x15555555, 0x15555555,
          0x55555100, 0x55555100, 0x55555100, 0x55555100 },
        { 0x55510000, 0x55510000, 0x55510000, 0x55510000,
          0x66661000, 0x66661000, 0x66661000, 0x11111000 },
        /* column 2 */
        { 0x55555551, 0x55555551, 0x55555551, 0x55555551,
          0x00155555, 0x00155555, 0x00155555, 0x00155555 },
        { 0x00001555, 0x00001555, 0x00001555, 0x00001555,
          0x00001666, 0x00001666, 0x00001666, 0x00001111 },
        /* column 3 */
        { 0,0,0,0, 0x51000000, 0x51000000, 0x51000000, 0x51000000 },
        { 0x55510000, 0x55510000, 0x55510000, 0x55510000,
          0x66666610, 0x66666610, 0x66666610, 0x11111110 }
    }
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */
typedef struct
{
    s16 pos;        /* x in 1/16 px                         */
    s16 minPos;     /* patrol limits in 1/16 px             */
    s16 maxPos;
    s16 spd;        /* 1/16 px per frame (all different)    */
    s16 dir;        /* +1 right, -1 left                    */
    u16 anim;       /* walk-cycle accumulator               */
    u8  pal;
} Enemy;

typedef struct
{
    s16 x;
    s16 y;
    u8  pal;
    u8  flip;
    u8  legs;
} DrawItem;

static Enemy    enemies[NUM_ENEMIES];
static DrawItem items[NUM_CHARS];
static u8       order[NUM_CHARS];

static s16 playerX;
static s16 playerY;
static u8  playerFlip;
static u16 playerAnim;
static s16 camX;

/* Different speed for every enemy (1/16 px per frame) */
static const s16 enemySpeeds[NUM_ENEMIES] = { 10, 18, 7, 26, 14, 22, 9, 28, 16, 12 };

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */
static void initGraphics(void)
{
    u16 x, y;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setPalette(PAL0, palBackground, CPU);
    PAL_setPalette(PAL1, palPlayer, CPU);
    PAL_setPalette(PAL2, palEnemyA, CPU);
    PAL_setPalette(PAL3, palEnemyB, CPU);

    VDP_loadTileData((const u32*) tilesBackground,   BG_TILE_BASE,   6,  CPU);
    VDP_loadTileData((const u32*) tilesCharTop,      TOP_TILE_BASE,  16, CPU);
    VDP_loadTileData((const u32*) tilesLegs,         LEGS_TILE_BASE, 16, CPU);

    VDP_clearPlane(BG_A, TRUE);

    /* Background on plane B: 64x32 tile map; the pattern repeats every
       4 tiles so the 512 px plane wraps seamlessly while scrolling. */
    for (y = 0; y < 28; y++)
    {
        for (x = 0; x < 64; x++)
        {
            u16 t;

            if (y < 5)            t = T_SKY1;
            else if (y < 10)      t = T_SKY2;
            else if (y < 17)      t = ((y == 12 || y == 13) && ((x & 3) == 1)) ? T_WIN : T_WALL;
            else if (y == 17)     t = T_FTOP;
            else                  t = T_FLOOR;

            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, BG_TILE_BASE + t), x, y);
        }
    }

    VDP_resetSprites();
}

static void initActors(void)
{
    u16 i;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        Enemy *e = &enemies[i];
        s16 laneX = i * LANE_W;

        e->minPos = laneX * 16;
        e->maxPos = (laneX + PATROL_LEN) * 16;
        e->spd    = enemySpeeds[i];
        e->dir    = (i & 1) ? -1 : 1;
        e->pos    = e->minPos + (s16) (((i * 37) % 97) * 16);
        e->anim   = i * 29;
        e->pal    = (i & 1) ? PAL3 : PAL2;
    }

    playerX    = 64;
    playerY    = ENEMY_Y;
    playerFlip = FALSE;
    playerAnim = 0;
    camX       = 0;
}

/* ------------------------------------------------------------------ */
/* Per-frame logic                                                     */
/* ------------------------------------------------------------------ */
static void updatePlayer(void)
{
    u16 pad = JOY_readJoypad(JOY_1);
    bool moving = FALSE;

    if (pad & BUTTON_LEFT)
    {
        playerX -= PLAYER_SPEED_X;
        playerFlip = TRUE;
        moving = TRUE;
    }
    else if (pad & BUTTON_RIGHT)
    {
        playerX += PLAYER_SPEED_X;
        playerFlip = FALSE;
        moving = TRUE;
    }

    if (pad & BUTTON_UP)
    {
        playerY -= PLAYER_SPEED_Y;
        moving = TRUE;
    }
    else if (pad & BUTTON_DOWN)
    {
        playerY += PLAYER_SPEED_Y;
        moving = TRUE;
    }

    if (playerX < 0) playerX = 0;
    if (playerX > WORLD_W - 32) playerX = WORLD_W - 32;
    if (playerY < PLAYER_Y_MIN) playerY = PLAYER_Y_MIN;
    if (playerY > PLAYER_Y_MAX) playerY = PLAYER_Y_MAX;

    if (moving) playerAnim += 32;
    else        playerAnim = 0;
}

static void updateEnemies(void)
{
    u16 i;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        Enemy *e = &enemies[i];

        e->pos += e->dir * e->spd;

        if (e->pos >= e->maxPos)
        {
            e->pos = e->maxPos;
            e->dir = -1;
        }
        else if (e->pos <= e->minPos)
        {
            e->pos = e->minPos;
            e->dir = 1;
        }

        e->anim += e->spd;
    }
}

static void updateCamera(void)
{
    s16 target = playerX + 16 - (SCREEN_W / 2);

    if (target < 0) target = 0;
    if (target > WORLD_W - SCREEN_W) target = WORLD_W - SCREEN_W;
    camX = target;
}

/* ------------------------------------------------------------------ */
/* Sprite list construction                                            */
/* ------------------------------------------------------------------ */
static void buildSprites(void)
{
    s16 i, j;
    u16 nVis = 0;
    u16 total;
    u16 spr = 0;

    /* item 0 = player, 1..10 = enemies */
    items[0].x    = playerX;
    items[0].y    = playerY;
    items[0].pal  = PAL1;
    items[0].flip = playerFlip;
    items[0].legs = (playerAnim >> 7) & 1;

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        const Enemy *e = &enemies[i];
        DrawItem *d = &items[i + 1];

        d->x    = e->pos >> 4;
        d->y    = ENEMY_Y;
        d->pal  = e->pal;
        d->flip = (e->dir < 0) ? TRUE : FALSE;
        d->legs = (e->anim >> 7) & 1;
    }

    /* Depth sort: larger feet Y (closer to the camera) first in the list,
       so it is drawn on top. Insertion sort, stable (player wins ties). */
    for (i = 0; i < NUM_CHARS; i++) order[i] = i;

    for (i = 1; i < NUM_CHARS; i++)
    {
        u8 key = order[i];

        j = i - 1;
        while (j >= 0 && items[order[j]].y < items[key].y)
        {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    /* Count on-screen characters; off-screen ones are simply not emitted. */
    for (i = 0; i < NUM_CHARS; i++)
    {
        s16 sx = items[order[i]].x - camX;
        if (sx > -32 && sx < SCREEN_W) nVis++;
    }

    total = nVis * 2;   /* player is always on screen, so total >= 2 */

    for (i = 0; i < NUM_CHARS; i++)
    {
        const DrawItem *d = &items[order[i]];
        s16 sx = d->x - camX;
        u16 attrTop, attrLegs;

        if (sx <= -32 || sx >= SCREEN_W) continue;

        attrTop  = TILE_ATTR_FULL(d->pal, FALSE, FALSE, d->flip, TOP_TILE_BASE);
        attrLegs = TILE_ATTR_FULL(d->pal, FALSE, FALSE, d->flip, LEGS_TILE_BASE + (d->legs * 8));

        /* upper body 32x32 */
        VDP_setSpriteFull(spr, sx, d->y, SPRITE_SIZE(4, 4), attrTop, spr + 1);
        spr++;

        /* legs 32x16 directly below; last sprite of the list links to 0 */
        VDP_setSpriteFull(spr, sx, d->y + 32, SPRITE_SIZE(4, 2), attrLegs,
                          (spr + 1 < total) ? (spr + 1) : 0);
        spr++;
    }

    VDP_updateSprites(total, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    (void) hardReset;

    initGraphics();
    initActors();

    VDP_setHorizontalScroll(BG_B, 0);

    while (TRUE)
    {
        updatePlayer();
        updateEnemies();
        updateCamera();
        buildSprites();

        /* Waits for VBlank and flushes the DMA queue (sprite table). */
        SYS_doVBlankProcess();

        /* Scroll register written right at the start of VBlank. */
        VDP_setHorizontalScroll(BG_B, -camX);
    }

    return 0;
}
