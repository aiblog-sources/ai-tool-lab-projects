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

static u16 curDelay;                   /* cached moveDelay() result */
static u32 shownScore;                 /* values currently drawn in the HUD */
static u16 shownLen;
static bool hudValid;

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

/* Static labels: drawn once per board */
static void drawScoreLabels(void)
{
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("LEN", 15, 0);
    hudValid = FALSE;
}

/* Values: redrawn only when they changed */
static void drawScore(void)
{
    char buf[12];

    if (hudValid && shownScore == score && shownLen == snakeLen) return;

    if (!hudValid || shownScore != score)
    {
        uintToStr(score, buf, 5);
        VDP_drawText(buf, 7, 0);
    }
    if (!hudValid || shownLen != snakeLen)
    {
        uintToStr(snakeLen, buf, 3);
        VDP_drawText(buf, 19, 0);
    }

    shownScore = score;
    shownLen = snakeLen;
    hudValid = TRUE;
}

static u16 moveDelay(void)
{
    u16 reduce = (u16) (snakeLen - START_LEN) / (u16) 3;
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
        u16 x = PLAY_X_MIN + ((u16) random() % (u16) (PLAY_X_MAX - PLAY_X_MIN + 1));
        u16 y = PLAY_Y_MIN + ((u16) random() % (u16) (PLAY_Y_MAX - PLAY_Y_MIN + 1));

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
    }
    for (y = 1; y < GRID_H; y++)
    {
        grid[y][0] = 1;
        grid[y][GRID_W - 1] = 1;
    }

    VDP_fillTileMapRect(BG_A, ATTR(TILE_WALL_IDX), 0, 1, GRID_W, 1);
    VDP_fillTileMapRect(BG_A, ATTR(TILE_WALL_IDX), 0, GRID_H - 1, GRID_W, 1);
    VDP_fillTileMapRect(BG_A, ATTR(TILE_WALL_IDX), 0, 1, 1, GRID_H - 1);
    VDP_fillTileMapRect(BG_A, ATTR(TILE_WALL_IDX), GRID_W - 1, 1, 1, GRID_H - 1);

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
    curDelay = moveDelay();

    placeFood();
    drawScoreLabels();
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
        curDelay = moveDelay();
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
        if (frameCnt >= curDelay)
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
