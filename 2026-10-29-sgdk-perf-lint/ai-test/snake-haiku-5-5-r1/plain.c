#include <genesis.h>

/* ---------- Configuration ---------- */
#define GRID_W      40
#define GRID_H      28
#define FIELD_TOP   1                       /* row 0 is the HUD */
#define MAX_LEN     (GRID_W * (GRID_H - FIELD_TOP))

#define DIR_UP      0
#define DIR_DOWN    1                       /* DIR ^ 1 gives the opposite direction */
#define DIR_LEFT    2
#define DIR_RIGHT   3

#define RESULT_ALIVE 0
#define RESULT_DEAD  1
#define RESULT_WON   2

/* Occupancy grid index = (y << 6) | x, so no multiply is needed */
#define OCC_SIZE    (GRID_H << 6)

/* ---------- Tile graphics (4bpp, 8 rows per tile, one u32 per row) ----------
 * Palette 1 indices: 1 = body, 2 = head, 3 = food. Index 0 is transparent.
 */
static const u32 gameTiles[24] =
{
    /* body: inset square */
    0x00000000, 0x01111110, 0x01111110, 0x01111110,
    0x01111110, 0x01111110, 0x01111110, 0x00000000,
    /* head: brighter inset square */
    0x00000000, 0x02222220, 0x02222220, 0x02222220,
    0x02222220, 0x02222220, 0x02222220, 0x00000000,
    /* food: small round dot */
    0x00333300, 0x03333330, 0x03333330, 0x03333330,
    0x03333330, 0x03333330, 0x00333300, 0x00000000
};

/* ---------- Game state ---------- */
/* Ring buffer: segment k (0 = head) lives at index (head + k) mod MAX_LEN */
static u8  bodyX[MAX_LEN];
static u8  bodyY[MAX_LEN];
static u16 head;
static u16 snakeLen;
static u8  dir;
static u8  nextDir;
static u8  foodX, foodY;
static u16 score;
static u16 moveTimer;
static u16 frameInterval;                   /* cached moveInterval() */
static u16 tileBody, tileHead, tileFood;
static u8  occ[OCC_SIZE];                   /* 1 = cell occupied by snake */

/* ---------- Drawing helpers ---------- */
static void putCell(u16 tile, u8 x, u8 y)
{
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, tile), x, y);
}

static void clearCell(u8 x, u8 y)
{
    VDP_setTileMapXY(BG_A, 0, x, y);
}

static void drawNum(u16 v, u8 digits, u16 x)
{
    char buf[6];
    u8 i;

    for (i = digits; i > 0; i--)
    {
        buf[i - 1] = '0' + (v % 10);
        v /= 10;
    }
    buf[digits] = 0;
    VDP_drawText(buf, x, 0);
}

/* Full HUD, used once per game. Labels never change during play. */
static void drawHUD(void)
{
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("LENGTH", 15, 0);
    drawNum(score, 5, 7);
    drawNum(snakeLen, 4, 22);
}

/* Only the numbers change while playing; the labels stay on screen */
static void updateScore(void)
{
    drawNum(score, 5, 7);
    drawNum(snakeLen, 4, 22);
}

/* ---------- Input and speed ---------- */
static void readDirection(u16 joy)
{
    u8 want;

    if (joy & BUTTON_UP)         want = DIR_UP;
    else if (joy & BUTTON_DOWN)  want = DIR_DOWN;
    else if (joy & BUTTON_LEFT)  want = DIR_LEFT;
    else if (joy & BUTTON_RIGHT) want = DIR_RIGHT;
    else return;

    /* Reject reversing into the neck (checked against the current direction) */
    if (want != (dir ^ 1))
        nextDir = want;
}

/* Frames per move: starts at 11, drops by 1 every 3 segments, floor of 3.
 * Only depends on snakeLen, so it is recomputed when the snake grows. */
static u16 calcInterval(void)
{
    u16 q = snakeLen / 3;
    return (q >= 9) ? 3 : (12 - q);
}

/* ---------- Game logic ---------- */
static void spawnFood(void)
{
    u8 x, y;

    if (snakeLen >= MAX_LEN)
        return;

    do
    {
        x = (u8)(random() % GRID_W);
        y = (u8)(FIELD_TOP + (random() % (GRID_H - FIELD_TOP)));
    } while (occ[(y << 6) | x]);

    foodX = x;
    foodY = y;
    putCell(tileFood, x, y);
}

static void newGame(void)
{
    VDP_clearPlane(BG_A, TRUE);

    memset(occ, 0, sizeof(occ));

    head = 0;
    snakeLen = 3;
    bodyX[0] = 20; bodyY[0] = 14;   /* head */
    bodyX[1] = 19; bodyY[1] = 14;
    bodyX[2] = 18; bodyY[2] = 14;   /* tail */

    occ[(14 << 6) | 20] = 1;
    occ[(14 << 6) | 19] = 1;
    occ[(14 << 6) | 18] = 1;

    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    score = 0;
    moveTimer = 0;
    frameInterval = calcInterval();

    putCell(tileHead, bodyX[0], bodyY[0]);
    putCell(tileBody, bodyX[1], bodyY[1]);
    putCell(tileBody, bodyX[2], bodyY[2]);

    spawnFood();
    drawHUD();
}

static u8 step(void)
{
    u8 nx = bodyX[head];
    u8 ny = bodyY[head];
    u8 grow;
    u8 tx, ty;
    u16 tailIdx, newHead;
    u8 cell;

    switch (dir)
    {
        case DIR_UP:   ny--; break;
        case DIR_DOWN: ny++; break;
        case DIR_LEFT: nx--; break;
        default:       nx++; break;
    }

    /* Wrapped u8 values are >= GRID_W, so underflow is caught too */
    if (nx >= GRID_W || ny < FIELD_TOP || ny >= GRID_H)
        return RESULT_DEAD;

    grow = (nx == foodX && ny == foodY && snakeLen < MAX_LEN);

    /* Tail segment: it vacates its cell unless we grow */
    tailIdx = head + snakeLen - 1;
    if (tailIdx >= MAX_LEN)
        tailIdx -= MAX_LEN;
    tx = bodyX[tailIdx];
    ty = bodyY[tailIdx];

    /* Self collision. Body segments never overlap, so the only cell that
     * can match the tail is the tail itself, which is allowed when not growing. */
    cell = (u8)((ny << 6) | nx);
    if (occ[cell] && (grow || nx != tx || ny != ty))
        return RESULT_DEAD;

    if (grow)
    {
        snakeLen++;
    }
    else
    {
        clearCell(tx, ty);
        occ[(ty << 6) | tx] = 0;
    }

    /* Old head becomes body; new head goes one slot back in the ring */
    putCell(tileBody, bodyX[head], bodyY[head]);

    newHead = head ? (u16)(head - 1) : (u16)(MAX_LEN - 1);
    bodyX[newHead] = nx;
    bodyY[newHead] = ny;
    head = newHead;

    occ[cell] = 1;
    putCell(tileHead, nx, ny);

    if (grow)
    {
        score += 10;
        updateScore();
        frameInterval = calcInterval();
        if (snakeLen >= MAX_LEN)
            return RESULT_WON;
        spawnFood();
    }

    return RESULT_ALIVE;
}

static u8 playGame(void)
{
    u16 joy;
    u8 result;

    for (;;)
    {
        joy = JOY_readJoypad(JOY_1);
        readDirection(joy);

        if (++moveTimer >= frameInterval)
        {
            moveTimer = 0;
            dir = nextDir;
            result = step();
            if (result != RESULT_ALIVE)
                return result;
        }

        SYS_doVBlankProcess();
    }
}

/* Waits for a START press; the frame count seeds the RNG so food placement varies */
static void waitStart(void)
{
    u16 prev = JOY_readJoypad(JOY_1);
    u16 joy;
    u32 seed = 1;

    for (;;)
    {
        joy = JOY_readJoypad(JOY_1);
        if ((joy & BUTTON_START) && !(prev & BUTTON_START))
            break;
        prev = joy;
        seed++;
        SYS_doVBlankProcess();
    }

    setRandomSeed(seed);
}

static void setupPalettes(void)
{
    u16 i;

    /* Palette 0: backdrop (index 0) and white text (indices 1-15) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x101830));
    for (i = 1; i < 16; i++)
        PAL_setColor(i, RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* Palette 1: snake and food */
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x30B040));  /* body */
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0xA0FFA0));  /* head */
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xFF5050));  /* food */
}

/* ---------- Entry point ---------- */
int main(bool hardReset)
{
    u8 result;

    JOY_init();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    setupPalettes();

    VDP_loadTileData(gameTiles, TILE_USER_INDEX, 3, FALSE);
    tileBody = TILE_USER_INDEX;
    tileHead = TILE_USER_INDEX + 1;
    tileFood = TILE_USER_INDEX + 2;

    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("SNAKE", 16, 10);
    VDP_drawText("PRESS START", 13, 14);
    waitStart();

    for (;;)
    {
        newGame();
        result = playGame();

        if (result == RESULT_WON)
            VDP_drawText("YOU WIN!", 15, 12);
        else
            VDP_drawText("GAME OVER", 15, 12);

        VDP_drawText("PRESS START", 13, 14);
        waitStart();
    }

    return 0;
}
