/*
 * SNAKE for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - 40x28 tile grid on plane A (full 320x224 screen)
 * - Background grid, score/HUD and messages on plane B (text in high
 *   priority, so it stays readable over the snake)
 * - Tiles defined in code and loaded with VDP_loadTileData (no rescomp)
 * - Pad 1: D-pad to steer, START to begin / pause / restart
 * - Speed increases as the snake grows
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

#define GRID_W          40
#define GRID_H          28
#define MAX_CELLS       (GRID_W * GRID_H)
#define INIT_LEN        4

#define START_DELAY     12      /* frames per step at start (5 steps/s)  */
#define MIN_DELAY       3       /* fastest: 20 steps/s                   */
#define GROW_PER_SPEED  3       /* every N segments gained -> 1 frame faster */

#define POINTS_PER_FOOD 10

/* Tile indices in VRAM */
#define TILE_BODY       (TILE_USER_INDEX + 0)
#define TILE_HEAD       (TILE_USER_INDEX + 1)
#define TILE_FOOD       (TILE_USER_INDEX + 2)
#define TILE_GRID       (TILE_USER_INDEX + 3)
#define NUM_TILES       4

#define ATTR_BODY       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_BODY)
#define ATTR_HEAD       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_HEAD)
#define ATTR_FOOD       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_FOOD)
#define ATTR_GRID       TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_GRID)
#define ATTR_EMPTY      0

/* Palette entries */
#define COL_BACKDROP    0       /* PAL0 */
#define COL_GRID        8       /* PAL0 */
#define COL_TEXT        15      /* PAL0 (default font uses index 15) */

#define PAL1_BASE       16
#define COL_BODY        1
#define COL_BODY_HI     2
#define COL_HEAD        3
#define COL_FOOD        4
#define COL_EYE         6
#define COL_STEM        7

/* ------------------------------------------------------------------ */
/* Tile data (4bpp, 8 rows of 8 pixels per tile, one nibble per pixel) */
/* ------------------------------------------------------------------ */

static const u32 gameTiles[NUM_TILES * 8] =
{
    /* TILE_BODY */
    0x01111110,
    0x12222211,
    0x12111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x01111110,

    /* TILE_HEAD */
    0x03333330,
    0x33333333,
    0x33633633,
    0x33633633,
    0x33333333,
    0x33333333,
    0x33333333,
    0x03333330,

    /* TILE_FOOD */
    0x00000700,
    0x00007000,
    0x04447440,
    0x44444444,
    0x44444444,
    0x44444444,
    0x04444440,
    0x00444400,

    /* TILE_GRID (background dot) */
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000008,
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER
} GameState;

enum { DIR_UP = 0, DIR_RIGHT, DIR_DOWN, DIR_LEFT };

static const s8 DX[4] = {  0, 1, 0, -1 };
static const s8 DY[4] = { -1, 0, 1,  0 };

/* Snake body as ring buffer, head at headIdx, tail at tailIdx */
static u8  bodyX[MAX_CELLS];
static u8  bodyY[MAX_CELLS];
static u16 headIdx;
static u16 tailIdx;
static u16 length;

/* Occupancy map for O(1) collision checks */
static u8  occ[GRID_H][GRID_W];

static u8  dir;
static u8  dirQueue[2];
static u8  dirQueueCount;

static u8  foodX, foodY;

static u16 score;
static u16 hiScore;
static u16 stepTimer;
static u16 stepDelay;          /* cached currentDelay(), updated on length change */

static GameState gameState;
static volatile bool startPressed;
static u16 frameCounter;

/* HUD template: "SCORE:%05u  LEN:%04u  HI:%05u" with digits patched in */
static char hudBuf[] = "SCORE:00000  LEN:0000  HI:00000";
#define HUD_SCORE_POS   6
#define HUD_LEN_POS     17
#define HUD_HI_POS      26

static const u16 pow10Tab[5] = { 10000, 1000, 100, 10, 1 };

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void setupPalette(void)
{
    /* PAL0: backdrop, grid, text */
    PAL_setColor(COL_BACKDROP, RGB24_TO_VDPCOLOR(0x101828));
    PAL_setColor(COL_GRID,     RGB24_TO_VDPCOLOR(0x384868));
    PAL_setColor(COL_TEXT,     RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* PAL1: game objects */
    PAL_setColor(PAL1_BASE + COL_BODY,    RGB24_TO_VDPCOLOR(0x20A040));
    PAL_setColor(PAL1_BASE + COL_BODY_HI, RGB24_TO_VDPCOLOR(0x80FF80));
    PAL_setColor(PAL1_BASE + COL_HEAD,    RGB24_TO_VDPCOLOR(0xC0FF40));
    PAL_setColor(PAL1_BASE + COL_FOOD,    RGB24_TO_VDPCOLOR(0xE02020));
    PAL_setColor(PAL1_BASE + COL_EYE,     RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(PAL1_BASE + COL_STEM,    RGB24_TO_VDPCOLOR(0x806020));
}

static void setHeadColorAlive(bool alive)
{
    PAL_setColor(PAL1_BASE + COL_HEAD,
                 alive ? RGB24_TO_VDPCOLOR(0xC0FF40) : RGB24_TO_VDPCOLOR(0xFF6000));
}

/* Restore a plane B row to background grid (used to erase messages) */
static void clearRow(u16 y)
{
    VDP_fillTileMapRect(BG_B, ATTR_GRID, 0, y, GRID_W, 1);
}

static void drawCentered(const char *str, u16 y)
{
    u16 len = strlen(str);
    u16 x = (len < GRID_W) ? (u16)(GRID_W - len) >> 1 : 0;
    VDP_drawText(str, x, y);
}

/* Write v as zero-padded decimal of 'width' digits (4 or 5), no division */
static void putDigits(char *dst, u16 v, u16 width)
{
    const u16 *p = &pow10Tab[5 - width];
    u16 n;

    for (n = 0; n < width; n++)
    {
        u16 pw = p[n];
        char d = '0';
        while (v >= pw)
        {
            v -= pw;
            d++;
        }
        dst[n] = d;
    }
}

static void drawHud(void)
{
    /* fixed width, so no clearing needed */
    putDigits(&hudBuf[HUD_SCORE_POS], score, 5);
    putDigits(&hudBuf[HUD_LEN_POS], length, 4);
    putDigits(&hudBuf[HUD_HI_POS], hiScore, 5);
    VDP_drawText(hudBuf, 1, 0);
}

static u16 currentDelay(void)
{
    s16 d = START_DELAY - (s16)((u16)(length - INIT_LEN) / (u16)GROW_PER_SPEED);
    if (d < MIN_DELAY) d = MIN_DELAY;
    return (u16)d;
}

static inline void setCell(u8 x, u8 y, u16 attr)
{
    VDP_setTileMapXY(BG_A, attr, x, y);
}

/* Place food on a free cell. Row 0 is avoided (HUD text lives there)
 * unless no other cell is free. Returns FALSE if the board is full. */
static bool spawnFood(void)
{
    u16 tries;

    for (tries = 0; tries < 64; tries++)
    {
        u8 x = (u8)((u16)random() % (u16)GRID_W);
        u8 y = (u8)(1 + ((u16)random() % (u16)(GRID_H - 1)));
        if (!occ[y][x])
        {
            foodX = x;
            foodY = y;
            setCell(x, y, ATTR_FOOD);
            return TRUE;
        }
    }

    /* Fallback: scan from a random starting point */
    {
        u16 start = (u16)random() % (u16)MAX_CELLS;
        u16 i;
        u8 x = (u8)(start % (u16)GRID_W);
        u8 y = (u8)(start / (u16)GRID_W);

        for (i = 0; i < MAX_CELLS; i++)
        {
            if (y != 0 && !occ[y][x])
            {
                foodX = x; foodY = y;
                setCell(x, y, ATTR_FOOD);
                return TRUE;
            }
            /* advance to next cell, wrapping like (start + i) % MAX_CELLS */
            if (++x >= GRID_W)
            {
                x = 0;
                if (++y >= GRID_H) y = 0;
            }
        }
        /* last resort: row 0 */
        for (i = 0; i < GRID_W; i++)
        {
            if (!occ[0][i])
            {
                foodX = i; foodY = 0;
                setCell(i, 0, ATTR_FOOD);
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void resetGame(void)
{
    u16 i;
    const u8 x0 = (GRID_W / 2 - INIT_LEN / 2);
    const u8 y0 = GRID_H / 2;

    VDP_clearPlane(BG_A, TRUE);
    memset(occ, 0, sizeof(occ));

    length  = INIT_LEN;
    tailIdx = 0;
    headIdx = INIT_LEN - 1;

    for (i = 0; i < INIT_LEN; i++)
    {
        u8 x = x0 + i;
        bodyX[i] = x;
        bodyY[i] = y0;
        occ[y0][x] = 1;
    }
    VDP_fillTileMapRect(BG_A, ATTR_BODY, x0, y0, INIT_LEN - 1, 1);
    setCell(x0 + INIT_LEN - 1, y0, ATTR_HEAD);

    dir = DIR_RIGHT;
    dirQueueCount = 0;
    score = 0;
    stepTimer = 0;
    stepDelay = currentDelay();

    setHeadColorAlive(TRUE);
    spawnFood();
    drawHud();
}

/* Queue a direction change (up to 2 buffered turns per step) */
static void queueDir(u8 d)
{
    u8 last;

    if (dirQueueCount >= 2) return;
    last = dirQueueCount ? dirQueue[dirQueueCount - 1] : dir;
    if (d == last || d == ((last + 2) & 3)) return;   /* same or reverse */
    dirQueue[dirQueueCount++] = d;
}

/* Advance snake one cell.
 * Returns 0 = ok, 1 = died, 2 = won (board full) */
static u8 stepSnake(void)
{
    s16 nx, ny;
    bool eat;
    u8 tx, ty;

    if (dirQueueCount)
    {
        dir = dirQueue[0];
        dirQueue[0] = dirQueue[1];
        dirQueueCount--;
    }

    nx = (s16)bodyX[headIdx] + DX[dir];
    ny = (s16)bodyY[headIdx] + DY[dir];

    /* Walls */
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H)
        return 1;

    eat = (nx == foodX && ny == foodY);
    tx = bodyX[tailIdx];
    ty = bodyY[tailIdx];

    /* Self collision (moving into the current tail cell is OK when the
     * tail is about to move away, i.e. when not eating) */
    if (occ[ny][nx])
    {
        if (eat || nx != tx || ny != ty)
            return 1;
    }

    /* Remove tail if not growing */
    if (!eat)
    {
        occ[ty][tx] = 0;
        setCell(tx, ty, ATTR_EMPTY);
        if (++tailIdx >= MAX_CELLS) tailIdx = 0;
        length--;
    }

    /* Old head becomes body */
    setCell(bodyX[headIdx], bodyY[headIdx], ATTR_BODY);

    /* New head */
    if (++headIdx >= MAX_CELLS) headIdx = 0;
    bodyX[headIdx] = (u8)nx;
    bodyY[headIdx] = (u8)ny;
    occ[ny][nx] = 1;
    length++;
    setCell((u8)nx, (u8)ny, ATTR_HEAD);

    if (eat)
    {
        stepDelay = currentDelay();

        score += POINTS_PER_FOOD;
        if (score > hiScore) hiScore = score;
        drawHud();

        if (length >= MAX_CELLS || !spawnFood())
            return 2;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Screens                                                             */
/* ------------------------------------------------------------------ */

static void showTitle(void)
{
    VDP_clearPlane(BG_A, TRUE);
    VDP_fillTileMapRect(BG_B, ATTR_GRID, 0, 0, GRID_W, GRID_H);

    drawCentered("S N A K E", 9);
    drawCentered("D-PAD : STEER", 13);
    drawCentered("START : PLAY / PAUSE", 15);
    drawCentered("PRESS START", 19);
}

static void clearTitle(void)
{
    clearRow(9);
    clearRow(13);
    clearRow(15);
    clearRow(19);
}

static void showGameOver(bool won)
{
    setHeadColorAlive(won);
    drawCentered(won ? " YOU WIN! " : " GAME OVER ", 12);
    drawCentered(" PRESS START ", 15);
}

static void clearGameOver(void)
{
    clearRow(12);
    clearRow(15);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

static void joyHandler(u16 joy, u16 changed, u16 state)
{
    u16 pressed;

    if (joy != JOY_1) return;
    pressed = changed & state;

    if (pressed & BUTTON_START) startPressed = TRUE;

    if (gameState == STATE_PLAYING)
    {
        if (pressed & BUTTON_UP)         queueDir(DIR_UP);
        else if (pressed & BUTTON_DOWN)  queueDir(DIR_DOWN);
        else if (pressed & BUTTON_LEFT)  queueDir(DIR_LEFT);
        else if (pressed & BUTTON_RIGHT) queueDir(DIR_RIGHT);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(COL_BACKDROP);

    setupPalette();
    VDP_loadTileData(gameTiles, TILE_USER_INDEX, NUM_TILES, CPU);

    VDP_setTextPlane(BG_B);
    VDP_setTextPalette(PAL0);
    VDP_setTextPriority(TRUE);   /* text above the (low priority) snake */

    JOY_setEventHandler(&joyHandler);

    hiScore = 0;
    score = 0;
    length = INIT_LEN;
    stepDelay = START_DELAY;
    frameCounter = 0;
    startPressed = FALSE;

    gameState = STATE_TITLE;
    showTitle();

    while (TRUE)
    {
        frameCounter++;

        switch (gameState)
        {
            case STATE_TITLE:
                if (startPressed)
                {
                    startPressed = FALSE;
                    setRandomSeed(frameCounter ^ 0xA5C3);
                    clearTitle();
                    resetGame();
                    gameState = STATE_PLAYING;
                }
                break;

            case STATE_PLAYING:
                if (startPressed)
                {
                    startPressed = FALSE;
                    drawCentered(" PAUSED ", 13);
                    gameState = STATE_PAUSED;
                    break;
                }

                stepTimer++;
                if (stepTimer >= stepDelay)
                {
                    u8 res;
                    stepTimer = 0;
                    res = stepSnake();
                    if (res != 0)
                    {
                        showGameOver(res == 2);
                        gameState = STATE_GAMEOVER;
                    }
                }
                break;

            case STATE_PAUSED:
                if (startPressed)
                {
                    startPressed = FALSE;
                    clearRow(13);
                    gameState = STATE_PLAYING;
                }
                break;

            case STATE_GAMEOVER:
                if (startPressed)
                {
                    startPressed = FALSE;
                    clearGameOver();
                    resetGame();
                    gameState = STATE_PLAYING;
                }
                break;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
