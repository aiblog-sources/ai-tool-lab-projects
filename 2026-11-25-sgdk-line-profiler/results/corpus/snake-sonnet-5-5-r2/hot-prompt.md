Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 4,321 cycles per frame on average (3.4%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    2,124 /frame  49.2% of busy  src/main.c:197  SYS_doVBlankProcess();
                     2,107 in SYS_doVBlankProcessEx.constprop.0.isra.0: 0.62 calls/frame, 3,390 cycles each
    1,293 /frame  29.9% of busy  src/main.c:329  SYS_doVBlankProcess();
                     1,286 in SYS_doVBlankProcessEx.constprop.0.isra.0: 0.38 calls/frame, 3,400 cycles each
      318 /frame   7.4% of busy  [no game line]:0  
                       126 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      252 /frame   5.8% of busy  src/main.c:147  u16 reduce = (snakeLen - START_LEN) / 3;
                       220 in __divsi3: 0.38 calls/frame, 580 cycles each
                 lint [div] frame: '/' on 32-bit / constant: about 623 cycles (measured: s32 / 10 is 623, (u16 - 240) / 30 is 640; u16 / 10 is 77
       78 /frame   1.8% of busy  src/main.c:124  VDP_setTileMapXY(BG_A, ATTR(tileIdx), x, y);
                 lint [tile-loop] hot: VDP_setTileMapXY in a loop: 324 cycles per tile; VDP_fillTileMapRect is 18 per tile, VDP_setTileMapDataRect 37
       75 /frame   1.7% of busy  src/main.c:196  random();                       /* keep the RNG state moving
       24 /frame   0.6% of busy  src/main.c:181  else if ((pad & BUTTON_RIGHT) && curDir != DIR_LEFT) nextDir
       17 /frame   0.4% of busy  src/main.c:205  VDP_clearPlane(BG_A, TRUE);
       11 /frame   0.3% of busy  src/main.c:209  grid[y][x] = 0;
       11 /frame   0.2% of busy  src/main.c:180  else if ((pad & BUTTON_LEFT) && curDir != DIR_RIGHT) nextDir
       10 /frame   0.2% of busy  src/main.c:194  if ((pad & BUTTON_START) && !(old & BUTTON_START)) break;
        9 /frame   0.2% of busy  src/main.c:178  if ((pad & BUTTON_UP) && curDir != DIR_DOWN)         nextDir
        8 /frame   0.2% of busy  src/main.c:129  VDP_setTileMapXY(BG_A, 0, x, y);
        8 /frame   0.2% of busy  src/main.c:179  else if ((pad & BUTTON_DOWN) && curDir != DIR_UP)    nextDir
        7 /frame   0.2% of busy  src/main.c:150  if (reduce > (START_DELAY - MIN_DELAY)) reduce = START_DELAY

== Lint findings, by measured cost ==
negligible       252 /frame (0.20% of a frame)  src/main.c:147 [div] frame
negligible        78 /frame (0.06% of a frame)  src/main.c:124 [tile-loop] hot
negligible         4 /frame (0.00% of a frame)  src/main.c:377 [text] frame
negligible         4 /frame (0.00% of a frame)  src/main.c:378 [text] frame
negligible         3 /frame (0.00% of a frame)  src/main.c:136 [text] frame
negligible         3 /frame (0.00% of a frame)  src/main.c:138 [text] frame
negligible         3 /frame (0.00% of a frame)  src/main.c:140 [text] frame
negligible         3 /frame (0.00% of a frame)  src/main.c:142 [text] frame
negligible         1 /frame (0.00% of a frame)  src/main.c:137 [text] frame
negligible         1 /frame (0.00% of a frame)  src/main.c:141 [text] frame
negligible         0 /frame (0.00% of a frame)  src/main.c:161 [div] frame
negligible         0 /frame (0.00% of a frame)  src/main.c:162 [div] frame
   not run         0 /frame (0.00% of a frame)  src/main.c:324 [text] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
  (none)

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Snake for Sega Mega Drive / Genesis (SGDK 2.09)                    */
/* 40x28 tile grid on plane A. Row 0 = score line, border walls on    */
/* rows 1 and 27 and columns 0 and 39. No external resources.         */
/* ------------------------------------------------------------------ */

#define GRID_W          40
#define GRID_H          28

#define PLAY_X_MIN      1
#define PLAY_X_MAX      38
#define PLAY_Y_MIN      2
#define PLAY_Y_MAX      26

#define MAX_LEN         ((PLAY_X_MAX - PLAY_X_MIN + 1) * (PLAY_Y_MAX - PLAY_Y_MIN + 1))
#define RING_SIZE       1024
#define RING_MASK       (RING_SIZE - 1)

#define START_LEN       3
#define START_DELAY     12
#define MIN_DELAY       3

#define DIR_UP          0
#define DIR_RIGHT       1
#define DIR_DOWN        2
#define DIR_LEFT        3

/* Tile indexes (loaded right after the system/font tiles) */
#define TILE_WALL_IDX   (TILE_USER_INDEX + 0)
#define TILE_HEAD_IDX   (TILE_USER_INDEX + 1)
#define TILE_BODY_IDX   (TILE_USER_INDEX + 2)
#define TILE_FOOD_IDX   (TILE_USER_INDEX + 3)
#define NUM_USER_TILES  4

#define ATTR(idx)       TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, (idx))

/* Palette colour indexes used by our tiles (font uses index 15) */
#define COL_WALL_DARK   1
#define COL_WALL_LIGHT  2
#define COL_HEAD        3
#define COL_BODY        4
#define COL_BODY_DARK   5
#define COL_FOOD        6
#define COL_FOOD_LEAF   7
#define COL_EYE         8

/* Each u32 is one 8-pixel row, one nibble per pixel (palette index) */
static const u32 tileWall[8] =
{
    0x11111111,
    0x12222221,
    0x12111121,
    0x12122121,
    0x12122121,
    0x12111121,
    0x12222221,
    0x11111111
};

static const u32 tileHead[8] =
{
    0x03333330,
    0x33333333,
    0x33833833,
    0x33833833,
    0x33333333,
    0x33333333,
    0x33333333,
    0x03333330
};

static const u32 tileBody[8] =
{
    0x00000000,
    0x04444440,
    0x44444445,
    0x44444445,
    0x44444445,
    0x44444445,
    0x04555550,
    0x00000000
};

static const u32 tileFood[8] =
{
    0x00000700,
    0x00007000,
    0x00666600,
    0x06666660,
    0x06666660,
    0x06666660,
    0x00666600,
    0x00000000
};

/* ------------------------------------------------------------------ */
/* Game state                                                         */
/* ------------------------------------------------------------------ */

static u8  grid[GRID_H][GRID_W];       /* 0 = free, 1 = wall/snake */
static u8  snakeX[RING_SIZE];
static u8  snakeY[RING_SIZE];
static u16 headIdx;
static u16 tailIdx;
static u16 snakeLen;

static u8  curDir;                     /* direction used for the last move */
static u8  nextDir;                    /* direction requested by the player */

static u8  foodX;
static u8  foodY;

static u32 score;
static u16 prevPad;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void putTile(u16 x, u16 y, u16 tileIdx)
{
    VDP_setTileMapXY(BG_A, ATTR(tileIdx), x, y);
}

static void clearCell(u16 x, u16 y)
{
    VDP_setTileMapXY(BG_A, 0, x, y);
}

static void drawScore(void)
{
    char buf[12];

    VDP_drawText("SCORE", 1, 0);
    uintToStr(score, buf, 5);
    VDP_drawText(buf, 7, 0);

    VDP_drawText("LEN", 15, 0);
    uintToStr(snakeLen, buf, 3);
    VDP_drawText(buf, 19, 0);
}

static u16 moveDelay(void)
{
    u16 reduce = (snakeLen - START_LEN) / 3;
    u16 delay;

    if (reduce > (START_DELAY - MIN_DELAY)) reduce = START_DELAY - MIN_DELAY;
    delay = START_DELAY - reduce;
    return delay;
}

static void placeFood(void)
{
    if (snakeLen >= MAX_LEN) return;   /* board full, nothing to place */

    for (;;)
    {
        u16 x = PLAY_X_MIN + (random() % (PLAY_X_MAX - PLAY_X_MIN + 1));
        u16 y = PLAY_Y_MIN + (random() % (PLAY_Y_MAX - PLAY_Y_MIN + 1));

        if (grid[y][x] == 0)
        {
            foodX = (u8) x;
            foodY = (u8) y;
            putTile(x, y, TILE_FOOD_IDX);
            return;
        }
    }
}

static void readInput(void)
{
    u16 pad = JOY_readJoypad(JOY_1);

    if ((pad & BUTTON_UP) && curDir != DIR_DOWN)         nextDir = DIR_UP;
    else if ((pad & BUTTON_DOWN) && curDir != DIR_UP)    nextDir = DIR_DOWN;
    else if ((pad & BUTTON_LEFT) && curDir != DIR_RIGHT) nextDir = DIR_LEFT;
    else if ((pad & BUTTON_RIGHT) && curDir != DIR_LEFT) nextDir = DIR_RIGHT;

    prevPad = pad;
}

static void waitStart(void)
{
    u16 pad;
    u16 old = JOY_readJoypad(JOY_1);

    for (;;)
    {
        pad = JOY_readJoypad(JOY_1);
        if ((pad & BUTTON_START) && !(old & BUTTON_START)) break;
        old = pad;
        random();                       /* keep the RNG state moving */
        SYS_doVBlankProcess();
    }
}

static void initBoard(void)
{
    u16 x, y;

    VDP_clearPlane(BG_A, TRUE);

    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            grid[y][x] = 0;

    /* Border walls (rows 1 and 27, columns 0 and 39); row 0 is the HUD */
    for (x = 0; x < GRID_W; x++)
    {
        grid[1][x] = 1;
        grid[GRID_H - 1][x] = 1;
        putTile(x, 1, TILE_WALL_IDX);
        putTile(x, GRID_H - 1, TILE_WALL_IDX);
    }
    for (y = 1; y < GRID_H; y++)
    {
        grid[y][0] = 1;
        grid[y][GRID_W - 1] = 1;
        putTile(0, y, TILE_WALL_IDX);
        putTile(GRID_W - 1, y, TILE_WALL_IDX);
    }

    /* Snake: tail on the left, head on the right, moving right */
    tailIdx = 0;
    headIdx = START_LEN - 1;
    snakeLen = START_LEN;

    for (x = 0; x < START_LEN; x++)
    {
        u8 sx = (u8) (18 + x);
        u8 sy = 14;

        snakeX[x] = sx;
        snakeY[x] = sy;
        grid[sy][sx] = 1;
        putTile(sx, sy, (x == START_LEN - 1) ? TILE_HEAD_IDX : TILE_BODY_IDX);
    }

    curDir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    score = 0;

    placeFood();
    drawScore();
}

/* Advance the snake one cell. Returns TRUE if the snake survived. */
static bool stepSnake(void)
{
    s16 nx = snakeX[headIdx];
    s16 ny = snakeY[headIdx];
    bool ate;

    curDir = nextDir;

    switch (curDir)
    {
        case DIR_UP:    ny--; break;
        case DIR_DOWN:  ny++; break;
        case DIR_LEFT:  nx--; break;
        default:        nx++; break;
    }

    ate = (nx == foodX && ny == foodY);

    /* If not growing, the tail cell is freed before the collision test */
    if (!ate)
    {
        u8 tx = snakeX[tailIdx];
        u8 ty = snakeY[tailIdx];

        grid[ty][tx] = 0;
        clearCell(tx, ty);
        tailIdx = (tailIdx + 1) & RING_MASK;
        snakeLen--;
    }

    if (grid[ny][nx] != 0)
        return FALSE;                   /* hit wall or itself */

    /* Old head becomes body */
    putTile(snakeX[headIdx], snakeY[headIdx], TILE_BODY_IDX);

    /* New head */
    headIdx = (headIdx + 1) & RING_MASK;
    snakeX[headIdx] = (u8) nx;
    snakeY[headIdx] = (u8) ny;
    grid[ny][nx] = 1;
    putTile(nx, ny, TILE_HEAD_IDX);
    snakeLen++;

    if (ate)
    {
        score += 10;
        placeFood();
        drawScore();
    }

    return TRUE;
}

static void playGame(void)
{
    u16 frameCnt = 0;

    initBoard();

    for (;;)
    {
        readInput();

        frameCnt++;
        if (frameCnt >= moveDelay())
        {
            frameCnt = 0;
            if (!stepSnake()) return;

            if (snakeLen >= MAX_LEN)
            {
                VDP_drawText("YOU WIN!", 16, 13);
                return;
            }
        }

        SYS_doVBlankProcess();
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void) hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(64, 32, TRUE);

    VDP_setTextPlane(BG_A);
    VDP_setTextPalette(PAL0);

    /* Palette */
    PAL_setColor(0,              RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(COL_WALL_DARK,  RGB24_TO_VDPCOLOR(0x2020A0));
    PAL_setColor(COL_WALL_LIGHT, RGB24_TO_VDPCOLOR(0x6060E0));
    PAL_setColor(COL_HEAD,       RGB24_TO_VDPCOLOR(0x60FF60));
    PAL_setColor(COL_BODY,       RGB24_TO_VDPCOLOR(0x00C000));
    PAL_setColor(COL_BODY_DARK,  RGB24_TO_VDPCOLOR(0x006000));
    PAL_setColor(COL_FOOD,       RGB24_TO_VDPCOLOR(0xE02020));
    PAL_setColor(COL_FOOD_LEAF,  RGB24_TO_VDPCOLOR(0x80E040));
    PAL_setColor(COL_EYE,        RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15,             RGB24_TO_VDPCOLOR(0xFFFFFF));   /* font colour */

    /* Tiles */
    VDP_loadTileData(tileWall, TILE_WALL_IDX, 1, CPU);
    VDP_loadTileData(tileHead, TILE_HEAD_IDX, 1, CPU);
    VDP_loadTileData(tileBody, TILE_BODY_IDX, 1, CPU);
    VDP_loadTileData(tileFood, TILE_FOOD_IDX, 1, CPU);

    /* Title screen */
    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("S N A K E", 15, 10);
    VDP_drawText("D-PAD: MOVE", 14, 14);
    VDP_drawText("PRESS START", 14, 17);
    waitStart();

    for (;;)
    {
        playGame();

        VDP_drawText(" GAME OVER ", 14, 12);
        VDP_drawText("PRESS START", 14, 14);
        waitStart();
    }

    return 0;
}
