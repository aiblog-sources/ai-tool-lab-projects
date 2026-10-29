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
static u8  bodyX[MAX_LEN];                  /* index 0 = head, snakeLen-1 = tail */
static u8  bodyY[MAX_LEN];
static u16 snakeLen;
static u8  dir;
static u8  nextDir;
static u8  foodX, foodY;
static u16 score;
static u16 moveTimer;
static u16 tileBody, tileHead, tileFood;
static u16 hudScore, hudLen;                /* last values drawn in the HUD */

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

/* Labels are drawn once per game in newGame(); only changed numbers are redrawn here */
static void drawHUD(void)
{
    if (score != hudScore)
    {
        hudScore = score;
        drawNum(score, 5, 7);
    }
    if (snakeLen != hudLen)
    {
        hudLen = snakeLen;
        drawNum(snakeLen, 4, 22);
    }
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

/* Frames per move: starts at 11, drops by 1 every 3 segments, floor of 3 */
static u16 moveInterval(void)
{
    u16 q = snakeLen / 3;
    return (q >= 9) ? 3 : (12 - q);
}

/* ---------- Game logic ---------- */
static void spawnFood(void)
{
    u8 x, y;
    u16 i;
    u8 clash;

    if (snakeLen >= MAX_LEN)
        return;

    do
    {
        x = (u8)((u16)random() % (u16)GRID_W);
        y = (u8)(FIELD_TOP + ((u16)random() % (u16)(GRID_H - FIELD_TOP)));
        clash = FALSE;
        for (i = 0; i < snakeLen; i++)
        {
            if (bodyX[i] == x && bodyY[i] == y)
            {
                clash = TRUE;
                break;
            }
        }
    } while (clash);

    foodX = x;
    foodY = y;
    putCell(tileFood, x, y);
}

static void newGame(void)
{
    VDP_clearPlane(BG_A, TRUE);

    /* Static HUD labels: drawn once here, since the plane was just cleared */
    VDP_drawText("SCORE", 1, 0);
    VDP_drawText("LENGTH", 15, 0);
    hudScore = 0xFFFF;                      /* sentinel: force the first numeric draw */
    hudLen   = 0xFFFF;

    snakeLen = 3;
    bodyX[0] = 20; bodyY[0] = 14;   /* head */
    bodyX[1] = 19; bodyY[1] = 14;
    bodyX[2] = 18; bodyY[2] = 14;   /* tail */

    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    score = 0;
    moveTimer = 0;

    putCell(tileHead, bodyX[0], bodyY[0]);
    putCell(tileBody, bodyX[1], bodyY[1]);
    putCell(tileBody, bodyX[2], bodyY[2]);

    spawnFood();
    drawHUD();
}

static u8 step(void)
{
    u8 nx = bodyX[0];
    u8 ny = bodyY[0];
    u8 grow;
    u16 i, limit, n;

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

    /* Self collision. The tail vacates its cell unless we grow. */
    limit = grow ? snakeLen : (snakeLen - 1);
    for (i = 0; i < limit; i++)
    {
        if (bodyX[i] == nx && bodyY[i] == ny)
            return RESULT_DEAD;
    }

    if (grow)
    {
        snakeLen++;
    }
    else
    {
        clearCell(bodyX[snakeLen - 1], bodyY[snakeLen - 1]);
    }

    /* Old head becomes body, then shift the array toward the tail */
    putCell(tileBody, bodyX[0], bodyY[0]);
    for (n = snakeLen - 1; n > 0; n--)
    {
        bodyX[n] = bodyX[n - 1];
        bodyY[n] = bodyY[n - 1];
    }
    bodyX[0] = nx;
    bodyY[0] = ny;
    putCell(tileHead, nx, ny);

    if (grow)
    {
        score += 10;
        drawHUD();
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

        if (++moveTimer >= moveInterval())
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
