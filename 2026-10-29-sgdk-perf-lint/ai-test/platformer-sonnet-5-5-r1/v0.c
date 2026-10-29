/*
 * main.c - One-screen platformer for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - 320x224 screen, pad 1: LEFT/RIGHT move, A jumps
 * - Level: 40x28 solid/empty tile array drawn on plane A
 * - Player: hardware sprite (2x2 tiles) with gravity and tile collision
 * - Coins on plane A add to the score (shown on plane B text)
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 */

#include <genesis.h>

#define MAP_W           40
#define MAP_H           28

/* ---------- Level definition (built from readable string rows) ---------- */
#define D1   "."
#define D2   ".."
#define D3   "..."
#define D5   D3 D2
#define D8   D5 D3
#define D10  ".........."
#define D13  D10 D3
#define D15  D10 D5
#define D16  D8 D8
#define D29  D10 D10 D8 D1

#define H4   "####"
#define H6   H4 "##"
#define H7   H4 "###"
#define H8   "########"
#define H10  "##########"
#define H12  H8 H4

#define ROW_EMPTY  D10 D10 D10 D10
#define ROW_WALL   "#" D10 D10 D10 D8 "#"
#define ROW_FLOOR  H10 H10 H10 H10

static const char levelMap[MAP_H][MAP_W + 1] =
{
    ROW_EMPTY,                                   /* 0  (score text row) */
    ROW_WALL, ROW_WALL, ROW_WALL, ROW_WALL, ROW_WALL,   /* 1-5  */
    "#" D2 H7 D29 "#",                           /* 6  */
    ROW_WALL, ROW_WALL, ROW_WALL,                /* 7-9  */
    "#" D15 H8 D15 "#",                          /* 10 */
    ROW_WALL, ROW_WALL, ROW_WALL,                /* 11-13 */
    "#" D5 H8 D16 H10,                           /* 14 */
    ROW_WALL, ROW_WALL, ROW_WALL,                /* 15-17 */
    H6 D8 H12 D13 "#",                           /* 18 */
    ROW_WALL, ROW_WALL, ROW_WALL,                /* 19-21 */
    "#" D3 H8 D16 H8 D3 "#",                     /* 22 */
    ROW_WALL, ROW_WALL, ROW_WALL,                /* 23-25 */
    ROW_FLOOR, ROW_FLOOR                         /* 26-27 */
};

/* Runtime solid/empty array: 1 = solid, 0 = empty */
static u8 level[MAP_H][MAP_W];

/* ---------- Tiles ---------- */
#define TILE_BRICK      (TILE_USER_INDEX + 0)
#define TILE_COIN       (TILE_USER_INDEX + 1)
#define TILE_PLAYER     (TILE_USER_INDEX + 2)   /* 4 tiles: TL, BL, TR, BR */
#define NUM_TILES       6

static const u32 tileData[NUM_TILES * 8] =
{
    /* 0: brick */
    0x22222221, 0x22222221, 0x22222221, 0x11111111,
    0x22122222, 0x22122222, 0x22122222, 0x11111111,

    /* 1: coin */
    0x00033000, 0x00333300, 0x03336330, 0x03333330,
    0x03333330, 0x03333330, 0x00333300, 0x00033000,

    /* 2: player top-left */
    0x00004444, 0x00044444, 0x00055555, 0x00055755,
    0x00055555, 0x00005555, 0x00077777, 0x00777777,

    /* 3: player bottom-left */
    0x05577777, 0x05507777, 0x00007777, 0x00007777,
    0x00007770, 0x00004440, 0x00004440, 0x00044440,

    /* 4: player top-right */
    0x44440000, 0x44444000, 0x55555000, 0x55755000,
    0x55555000, 0x55550000, 0x77777000, 0x77777700,

    /* 5: player bottom-right */
    0x77777550, 0x77770550, 0x77770000, 0x77770000,
    0x07770000, 0x04440000, 0x04440000, 0x04444000
};

/* 0x0BGR palette */
static const u16 palette[16] =
{
    0x0842,  /* 0: background / transparent */
    0x0248,  /* 1: brick mortar */
    0x026C,  /* 2: brick face */
    0x04EE,  /* 3: coin yellow */
    0x000E,  /* 4: red */
    0x068E,  /* 5: skin */
    0x0EEE,  /* 6: white */
    0x0E40,  /* 7: blue */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0EEE   /* 15: font color */
};

/* ---------- Coins ---------- */
#define NUM_COINS 8
static const u8 coinTx[NUM_COINS] = {  7, 31, 19,  3,  9, 34, 19,  6 };
static const u8 coinTy[NUM_COINS] = { 21, 21, 17, 17, 13, 13,  9,  5 };
static bool coinTaken[NUM_COINS];

/* ---------- Physics constants (8.8 fixed point for vertical speed) ---------- */
#define SPEED_X         2
#define GRAVITY         0x40
#define JUMP_VEL        (-0x500)
#define MAX_FALL        0x600

#define PLAYER_SIZE     16
#define HB_LEFT         2      /* hitbox left offset  */
#define HB_RIGHT        13     /* hitbox right offset */

/* ---------- State ---------- */
static s16  px, py;            /* player position in pixels (top-left of sprite) */
static s32  ypos;              /* vertical position, 8.8 fixed */
static s16  vy;                /* vertical velocity, 8.8 fixed */
static bool onGround;
static bool facingLeft;
static u32  score;
static u16  prevPad;

/* ---------- Helpers ---------- */
static bool solidAt(s16 x, s16 y)
{
    s16 tx = x >> 3;
    s16 ty = y >> 3;

    if (tx < 0 || tx >= MAP_W) return TRUE;
    if (ty < 0) return FALSE;
    if (ty >= MAP_H) return TRUE;
    return level[ty][tx] != 0;
}

/* Vertical column test at x (checks along the player's height) */
static bool columnSolid(s16 x, s16 y)
{
    return solidAt(x, y) || solidAt(x, y + 8) || solidAt(x, y + PLAYER_SIZE - 1);
}

/* Horizontal row test at y (checks along the player's hitbox width) */
static bool rowSolid(s16 x, s16 y)
{
    return solidAt(x + HB_LEFT, y) || solidAt(x + 7, y) || solidAt(x + HB_RIGHT, y);
}

static void drawCoin(u16 i)
{
    u16 tile = coinTaken[i] ? 0 : TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_COIN);
    VDP_setTileMapXY(BG_A, tile, coinTx[i], coinTy[i]);
}

static void drawScore(void)
{
    char buf[12];
    intToStr(score, buf, 5);
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText(buf, 7, 0);
}

static void resetCoins(void)
{
    for (u16 i = 0; i < NUM_COINS; i++)
    {
        coinTaken[i] = FALSE;
        drawCoin(i);
    }
}

static void drawLevel(void)
{
    const u16 brickAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BRICK);

    for (u16 y = 0; y < MAP_H; y++)
    {
        for (u16 x = 0; x < MAP_W; x++)
        {
            u8 solid = (levelMap[y][x] == '#') ? 1 : 0;
            level[y][x] = solid;
            VDP_setTileMapXY(BG_A, solid ? brickAttr : 0, x, y);
        }
    }
}

static void updatePlayer(u16 pad)
{
    s16 dx = 0;

    if (pad & BUTTON_LEFT)       { dx = -SPEED_X; facingLeft = TRUE;  }
    else if (pad & BUTTON_RIGHT) { dx =  SPEED_X; facingLeft = FALSE; }

    /* Horizontal movement + collision */
    if (dx != 0)
    {
        s16 nx = px + dx;
        if (dx > 0)
        {
            s16 edge = nx + HB_RIGHT;
            if (columnSolid(edge, py))
                nx = ((edge >> 3) << 3) - (HB_RIGHT + 1);
        }
        else
        {
            s16 edge = nx + HB_LEFT;
            if (columnSolid(edge, py))
                nx = (((edge >> 3) + 1) << 3) - HB_LEFT;
        }
        px = nx;
    }

    /* Jump (on A press, only when standing) */
    if ((pad & BUTTON_A) && !(prevPad & BUTTON_A) && onGround)
    {
        vy = JUMP_VEL;
        onGround = FALSE;
    }

    /* Gravity */
    vy += GRAVITY;
    if (vy > MAX_FALL) vy = MAX_FALL;

    ypos += vy;
    s16 ny = (s16)(ypos >> 8);

    if (vy > 0)
    {
        s16 bottom = ny + PLAYER_SIZE - 1;
        if (rowSolid(px, bottom))
        {
            ny = ((bottom >> 3) << 3) - PLAYER_SIZE;
            ypos = ((s32)ny) << 8;
            vy = 0;
        }
    }
    else if (vy < 0)
    {
        if (rowSolid(px, ny))
        {
            ny = ((ny >> 3) + 1) << 3;
            ypos = ((s32)ny) << 8;
            vy = 0;
        }
    }
    py = ny;

    /* Ground probe: one pixel below the feet */
    onGround = rowSolid(px, py + PLAYER_SIZE);
}

static void checkCoins(void)
{
    s16 cx = (px + 8) >> 3;
    s16 cy = (py + 8) >> 3;
    u16 taken = 0;

    for (u16 i = 0; i < NUM_COINS; i++)
    {
        if (!coinTaken[i] && coinTx[i] == cx && coinTy[i] == cy)
        {
            coinTaken[i] = TRUE;
            drawCoin(i);
            score += 10;
            drawScore();
        }
        if (coinTaken[i]) taken++;
    }

    /* All coins collected: bonus and respawn them */
    if (taken == NUM_COINS)
    {
        score += 50;
        drawScore();
        resetCoins();
    }
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette and tiles (no resources) */
    PAL_setColors(0, palette, 16, CPU);
    VDP_loadTileData(tileData, TILE_USER_INDEX, NUM_TILES, CPU);

    /* Text goes on plane B so it doesn't overwrite the level on plane A */
    VDP_setTextPlane(BG_B);
    VDP_setTextPalette(PAL0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    drawLevel();
    resetCoins();

    score = 0;
    drawScore();

    /* Player start on the floor */
    px = 32;
    py = 26 * 8 - PLAYER_SIZE;
    ypos = ((s32)py) << 8;
    vy = 0;
    onGround = TRUE;
    facingLeft = FALSE;
    prevPad = 0;

    VDP_resetSprites();

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        updatePlayer(pad);
        checkCoins();
        prevPad = pad;

        VDP_setSpriteFull(0, px, py, SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, facingLeft, TILE_PLAYER),
                          0);
        VDP_updateSprites(1, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
