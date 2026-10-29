/*
 * SNAKE for Sega Mega Drive / Genesis, SGDK 2.09
 * - 40x28 tile grid on plane A (full 320x224 screen)
 * - Score/HUD text on plane B with high priority, so it shows over the snake
 * - No resource files: tiles are const u32 arrays loaded with VDP_loadTileData
 * - Pad 1: D-Pad = steer, START = start / restart
 * - Speed increases with snake length
 */

#include <genesis.h>

#define GRID_W          40
#define GRID_H          28
#define MAX_LEN         (GRID_W * GRID_H)

#define INIT_LEN        3
#define START_DELAY     12      /* frames per step at start */
#define MIN_DELAY       4       /* fastest speed (frames per step) */
#define LEN_PER_SPEEDUP 3       /* grow this much to gain one speed step */
#define POINTS_PER_FOOD 10

/* Tile indices in VRAM */
#define TILE_BODY       (TILE_USER_INDEX + 0)
#define TILE_HEAD       (TILE_USER_INDEX + 1)
#define TILE_FOOD       (TILE_USER_INDEX + 2)

#define ATTR_BODY       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_BODY)
#define ATTR_HEAD       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_HEAD)
#define ATTR_FOOD       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_FOOD)

/* Palette 1 color indices used by the tiles:
 * 0 transparent, 1 dark green, 2 green, 3 light green,
 * 4 red, 5 white, 6 brown */
static const u32 tileData[3 * 8] =
{
    /* body */
    0x11111111,
    0x12222221,
    0x12222221,
    0x12222221,
    0x12222221,
    0x12222221,
    0x12222221,
    0x11111111,

    /* head (with eyes) */
    0x11111111,
    0x13333331,
    0x13533531,
    0x13533531,
    0x13333331,
    0x13333331,
    0x13333331,
    0x11111111,

    /* food (apple) */
    0x00006000,
    0x04466440,
    0x44544444,
    0x44544444,
    0x44444444,
    0x44444444,
    0x04444440,
    0x00444400
};

typedef enum { STATE_TITLE, STATE_PLAY, STATE_OVER } GameState;

/* Snake storage: ring buffer of cell coordinates + occupancy grid */
static u8 snakeX[MAX_LEN];
static u8 snakeY[MAX_LEN];
static u8 occupied[GRID_H][GRID_W];
static u16 headIdx;
static u16 tailIdx;
static u16 snakeLen;

static u8 foodX, foodY;
static u8 dir;          /* direction applied at last step: 0 up,1 right,2 down,3 left */
static u8 pendingDir;   /* direction requested by the player */
static u16 score;
static u16 hiScore;
static u16 frameCount;
static GameState state;

/* Values currently shown on the HUD (0xFFFF = not drawn yet) */
static u16 shownScore = 0xFFFF;
static u16 shownLen = 0xFFFF;
static u16 shownHi = 0xFFFF;

static const s8 dirDX[4] = { 0, 1, 0, -1 };
static const s8 dirDY[4] = { -1, 0, 1, 0 };

/* ---------- drawing helpers ---------- */

static void putCell(u8 x, u8 y, u16 attr)
{
    VDP_setTileMapXY(BG_A, attr, x, y);
}

static void clearCell(u8 x, u8 y)
{
    VDP_setTileMapXY(BG_A, 0, x, y);
}

/* Static labels: drawn once */
static void drawHudLabels(void)
{
    VDP_drawTextBG(BG_B, "SCORE", 1, 0);
    VDP_drawTextBG(BG_B, "LEN", 15, 0);
    VDP_drawTextBG(BG_B, "HI", 30, 0);
}

/* Values: redrawn only when they change */
static void updateHud(void)
{
    char buf[8];

    if (score != shownScore)
    {
        shownScore = score;
        uintToStr(score, buf, 5);
        VDP_drawTextBG(BG_B, buf, 7, 0);
    }

    if (snakeLen != shownLen)
    {
        shownLen = snakeLen;
        uintToStr(snakeLen, buf, 4);
        VDP_drawTextBG(BG_B, buf, 19, 0);
    }

    if (hiScore != shownHi)
    {
        shownHi = hiScore;
        uintToStr(hiScore, buf, 5);
        VDP_drawTextBG(BG_B, buf, 33, 0);
    }
}

/* ---------- snake logic ---------- */

static void pushHead(u8 x, u8 y)
{
    u16 h = headIdx + 1;
    if (h >= MAX_LEN) h = 0;
    headIdx = h;
    snakeX[h] = x;
    snakeY[h] = y;
    occupied[y][x] = 1;
    snakeLen++;
}

static void popTail(void)
{
    u8 x = snakeX[tailIdx];
    u8 y = snakeY[tailIdx];
    u16 t = tailIdx + 1;
    occupied[y][x] = 0;
    clearCell(x, y);
    if (t >= MAX_LEN) t = 0;
    tailIdx = t;
    snakeLen--;
}

static void placeFood(void)
{
    u16 rx, ry;
    const u16 gw = GRID_W;
    const u16 gh = GRID_H;

    if (snakeLen >= MAX_LEN) return;    /* board full, nothing to place */

    do
    {
        rx = random();
        ry = random();
        rx %= gw;
        ry %= gh;
        foodX = (u8) rx;
        foodY = (u8) ry;
    } while (occupied[foodY][foodX]);

    putCell(foodX, foodY, ATTR_FOOD);
}

static u16 currentDelay(void)
{
    u16 diff = snakeLen - INIT_LEN;
    const u16 per = LEN_PER_SPEEDUP;
    u16 speedups = diff / per;
    u16 maxSpeedups = START_DELAY - MIN_DELAY;
    if (speedups > maxSpeedups) speedups = maxSpeedups;
    return START_DELAY - speedups;
}

static void showCenterText(const char *l1, const char *l2)
{
    VDP_drawTextBG(BG_B, l1, 20 - (u16) (strlen(l1) / 2), 12);
    if (l2) VDP_drawTextBG(BG_B, l2, 20 - (u16) (strlen(l2) / 2), 14);
}

static void clearCenterText(void)
{
    VDP_clearTextAreaBG(BG_B, 8, 10, 24, 6);
}

static void newGame(void)
{
    u8 i;
    u16 row[INIT_LEN];
    const u8 x0 = (GRID_W / 2) - (INIT_LEN - 1);
    const u8 y0 = GRID_H / 2;

    VDP_clearPlane(BG_A, TRUE);
    clearCenterText();
    memset(occupied, 0, sizeof(occupied));

    headIdx = MAX_LEN - 1;  /* so the first push lands on index 0 */
    tailIdx = 0;
    snakeLen = 0;

    /* Start in the middle, heading right */
    for (i = 0; i < INIT_LEN; i++)
    {
        pushHead(x0 + i, y0);
        row[i] = (i == INIT_LEN - 1) ? ATTR_HEAD : ATTR_BODY;
    }
    /* CPU transfer: 'row' is a stack buffer, so DMA (queued) would be unsafe */
    VDP_setTileMapDataRect(BG_A, row, x0, y0, INIT_LEN, 1, INIT_LEN, CPU);

    dir = 1;
    pendingDir = 1;
    score = 0;
    frameCount = 0;

    placeFood();
    updateHud();
    state = STATE_PLAY;
}

static void gameOver(void)
{
    if (score > hiScore) hiScore = score;
    updateHud();

    if (snakeLen >= MAX_LEN) showCenterText("YOU WIN!", "PRESS START");
    else showCenterText("GAME OVER", "PRESS START");

    state = STATE_OVER;
}

static void stepSnake(void)
{
    u8 oldX = snakeX[headIdx];
    u8 oldY = snakeY[headIdx];
    s16 nx, ny;
    bool eating;

    dir = pendingDir;
    nx = oldX + dirDX[dir];
    ny = oldY + dirDY[dir];

    /* Wall collision */
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H)
    {
        gameOver();
        return;
    }

    eating = (nx == foodX && ny == foodY);

    /* Self collision (the tail cell is free if we are not growing) */
    if (occupied[ny][nx])
    {
        bool isTail = (nx == snakeX[tailIdx] && ny == snakeY[tailIdx]);
        if (eating || !isTail)
        {
            gameOver();
            return;
        }
    }

    if (!eating) popTail();

    /* Old head becomes body, new head is drawn */
    putCell(oldX, oldY, ATTR_BODY);
    pushHead((u8) nx, (u8) ny);
    putCell((u8) nx, (u8) ny, ATTR_HEAD);

    if (eating)
    {
        score += POINTS_PER_FOOD;
        if (snakeLen >= MAX_LEN)
        {
            gameOver();
            return;
        }
        placeFood();
    }

    updateHud();
}

/* ---------- main ---------- */

int main(bool hardReset)
{
    u16 joy, prevJoy = 0, pressed;

    (void) hardReset;

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);

    /* Palettes: PAL0 for text, PAL1 for the game tiles */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x006000));
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x20B020));
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0x80FF60));
    PAL_setColor(16 + 4, RGB24_TO_VDPCOLOR(0xE02020));
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0x804000));

    VDP_loadTileData(tileData, TILE_USER_INDEX, 3, CPU);

    /* Text on plane B with high priority so it stays above the snake */
    VDP_setTextPalette(PAL0);
    VDP_setTextPriority(TRUE);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    hiScore = 0;
    score = 0;
    snakeLen = INIT_LEN;
    state = STATE_TITLE;
    drawHudLabels();
    updateHud();
    showCenterText("S N A K E", "PRESS START");

    while (TRUE)
    {
        joy = JOY_readJoypad(JOY_1);
        pressed = joy & ~prevJoy;
        prevJoy = joy;

        switch (state)
        {
            case STATE_TITLE:
            case STATE_OVER:
                if (pressed & BUTTON_START)
                {
                    setRandomSeed((u16) (getTick() ^ (getTick() >> 16) ^ GET_VCOUNTER));
                    newGame();
                }
                break;

            case STATE_PLAY:
                /* Accept a direction unless it reverses the last applied one */
                if      ((joy & BUTTON_UP)    && dir != 2) pendingDir = 0;
                else if ((joy & BUTTON_RIGHT) && dir != 3) pendingDir = 1;
                else if ((joy & BUTTON_DOWN)  && dir != 0) pendingDir = 2;
                else if ((joy & BUTTON_LEFT)  && dir != 1) pendingDir = 3;

                frameCount++;
                if (frameCount >= currentDelay())
                {
                    frameCount = 0;
                    stepSnake();
                }
                break;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
