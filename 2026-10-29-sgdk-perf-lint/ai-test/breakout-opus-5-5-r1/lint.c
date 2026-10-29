/*
 * Breakout for Sega Mega Drive / Genesis
 * SGDK 2.09, single file, no resources.
 *
 * Controls (pad 1):
 *   D-pad LEFT/RIGHT : move paddle
 *   A / B / C / START: launch ball
 *   START            : pause during play / restart after game over
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */

#define SCREEN_W        320
#define SCREEN_H        224

#define FP              8           /* fixed point shift (8.8)       */

/* Playfield in tiles: walls at column 4 and 35, top wall at row 2   */
#define WALL_TX_LEFT    4
#define WALL_TX_RIGHT   35
#define WALL_TY_TOP     2

/* Playfield in pixels (inside the walls)                            */
#define FIELD_L         ((WALL_TX_LEFT + 1) * 8)    /* 40  */
#define FIELD_R         (WALL_TX_RIGHT * 8)         /* 280 */
#define FIELD_T         ((WALL_TY_TOP + 1) * 8)     /* 24  */

/* Bricks: 10 x 6, each 3 tiles wide and 1 tile tall                 */
#define BRICK_COLS      10
#define BRICK_ROWS      6
#define BRICK_TW        3
#define BRICK_TX0       5
#define BRICK_TY0       5
#define BRICK_PW        (BRICK_TW * 8)              /* 24 */
#define BRICK_PH        8
#define BRICK_X0        (BRICK_TX0 * 8)             /* 40 */
#define BRICK_Y0        (BRICK_TY0 * 8)             /* 40 */
#define BRICK_MAP_W     (BRICK_COLS * BRICK_TW)     /* 30 tiles */

/* Paddle and ball                                                    */
#define PADDLE_W        32
#define PADDLE_Y        200
#define PADDLE_SPEED    5
#define BALL_SIZE       6

/* Ball speed (8.8 fixed point, pixels per frame)                     */
#define SPEED_START     0x01C0
#define SPEED_STEP      0x0010
#define SPEED_MAX       0x0340

#define MSG_ROW         17

/* ------------------------------------------------------------------ */
/* Tile data                                                           */
/* ------------------------------------------------------------------ */

#define TILE_BRICK_A    (TILE_USER_INDEX + 0)   /* 3 tiles, colors 1-3 */
#define TILE_BRICK_B    (TILE_USER_INDEX + 3)   /* 3 tiles, colors 4-6 */
#define TILE_WALL       (TILE_USER_INDEX + 6)   /* 1 tile              */
#define TILE_PADDLE     (TILE_USER_INDEX + 7)   /* 4 tiles             */
#define TILE_BALL       (TILE_USER_INDEX + 11)  /* 1 tile              */
#define NUM_TILES       12

static const u32 tileData[NUM_TILES * 8] =
{
    /* ---- Brick set A (body 1, highlight 2, shadow 3) ---- */
    /* left */
    0x22222222,
    0x21111111,
    0x21111111,
    0x21111111,
    0x21111111,
    0x21111111,
    0x23333333,
    0x00000000,
    /* middle */
    0x22222222,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x33333333,
    0x00000000,
    /* right */
    0x22222220,
    0x11111130,
    0x11111130,
    0x11111130,
    0x11111130,
    0x11111130,
    0x33333330,
    0x00000000,

    /* ---- Brick set B (body 4, highlight 5, shadow 6) ---- */
    /* left */
    0x55555555,
    0x54444444,
    0x54444444,
    0x54444444,
    0x54444444,
    0x54444444,
    0x56666666,
    0x00000000,
    /* middle */
    0x55555555,
    0x44444444,
    0x44444444,
    0x44444444,
    0x44444444,
    0x44444444,
    0x66666666,
    0x00000000,
    /* right */
    0x55555550,
    0x44444460,
    0x44444460,
    0x44444460,
    0x44444460,
    0x44444460,
    0x66666660,
    0x00000000,

    /* ---- Wall (colors 11, 12) ---- */
    0xCCCCCCCC,
    0xCBBBBBBB,
    0xCBBBBBBB,
    0xCBBBBBBB,
    0xCCCCCCCC,
    0xBBBBCBBB,
    0xBBBBCBBB,
    0xBBBBCBBB,

    /* ---- Paddle (body 7, highlight 8, shadow 9), 4 tiles ---- */
    /* left cap */
    0x08888888,
    0x87777777,
    0x87777777,
    0x87777777,
    0x87777777,
    0x09999999,
    0x00000000,
    0x00000000,
    /* middle */
    0x88888888,
    0x77777777,
    0x77777777,
    0x77777777,
    0x77777777,
    0x99999999,
    0x00000000,
    0x00000000,
    /* middle */
    0x88888888,
    0x77777777,
    0x77777777,
    0x77777777,
    0x77777777,
    0x99999999,
    0x00000000,
    0x00000000,
    /* right cap */
    0x88888880,
    0x77777779,
    0x77777779,
    0x77777779,
    0x77777779,
    0x99999990,
    0x00000000,
    0x00000000,

    /* ---- Ball 6x6 (color 10, highlight 15) ---- */
    0x0AAAA000,
    0xAFFAAA00,
    0xAFAAAA00,
    0xAAAAAA00,
    0xAAAAAA00,
    0x0AAAA000,
    0x00000000,
    0x00000000,
};

/* Pixel column inside the brick area (in tiles) -> brick column     */
static const u8 brickColOfTile[BRICK_MAP_W] =
{
    0, 0, 0, 1, 1, 1, 2, 2, 2, 3,
    3, 3, 4, 4, 4, 5, 5, 5, 6, 6,
    6, 7, 7, 7, 8, 8, 8, 9, 9, 9
};

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

typedef enum
{
    ST_SERVE,
    ST_PLAY,
    ST_PAUSED,
    ST_GAMEOVER
} GameState;

static u8        bricks[BRICK_ROWS][BRICK_COLS];
static u16       bricksLeft;
static u16       bricksHitThisLevel;

static s16       paddleX;
static s32       ballX, ballY;      /* 8.8 fixed point */
static s16       velX, velY;        /* 8.8 fixed point */
static s16       speed;

static u32       score;
static u16       lives;
static u16       level;

static GameState state;
static bool      hudDirty;
static u16       prevPad;

/* Full brick wall tilemap, built once at startup */
static u16       brickMap[BRICK_ROWS * BRICK_MAP_W];

/* HUD cache: what is currently on screen */
static bool      hudFull;           /* labels + all values need drawing */
static u32       hudScore;
static u16       hudLevel;
static u16       hudLives;

/* Message line cache */
static const char *msgText;         /* message currently shown (NULL = none) */
static u16       msgX;
static u16       msgLen;            /* cells occupied on MSG_ROW */

/* Serve message cache */
static char      serveMsg[24];
static u16       serveMsgLevel;     /* 0 = not built yet */

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

static void setColor24(u16 pal, u16 idx, u32 rgb)
{
    PAL_setColor((pal * 16) + idx, RGB24_TO_VDPCOLOR(rgb));
}

static void setupPalettes(void)
{
    /* PAL0: background, paddle, ball, walls, text */
    setColor24(0, 0,  0x000000);
    setColor24(0, 7,  0x4060E0);   /* paddle body      */
    setColor24(0, 8,  0xA0C0FF);   /* paddle highlight */
    setColor24(0, 9,  0x203080);   /* paddle shadow    */
    setColor24(0, 10, 0xE0E0E0);   /* ball             */
    setColor24(0, 11, 0x606060);   /* wall             */
    setColor24(0, 12, 0xA0A0A0);   /* wall mortar      */
    setColor24(0, 15, 0xFFFFFF);   /* text / ball hi   */

    /* PAL1: red / orange bricks */
    setColor24(1, 1, 0xE02020); setColor24(1, 2, 0xFF8080); setColor24(1, 3, 0x800000);
    setColor24(1, 4, 0xE08020); setColor24(1, 5, 0xFFC060); setColor24(1, 6, 0x804000);

    /* PAL2: yellow / green bricks */
    setColor24(2, 1, 0xE0E020); setColor24(2, 2, 0xFFFF80); setColor24(2, 3, 0x808000);
    setColor24(2, 4, 0x20C020); setColor24(2, 5, 0x80FF80); setColor24(2, 6, 0x006000);

    /* PAL3: cyan / purple bricks */
    setColor24(3, 1, 0x20C0E0); setColor24(3, 2, 0x80FFFF); setColor24(3, 3, 0x006080);
    setColor24(3, 4, 0xA040E0); setColor24(3, 5, 0xE0A0FF); setColor24(3, 6, 0x500080);
}

/* ------------------------------------------------------------------ */
/* Drawing helpers                                                     */
/* ------------------------------------------------------------------ */

static void drawWalls(void)
{
    u16 attr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_WALL);

    /* top wall, columns 4..35 */
    VDP_fillTileMapRect(BG_B, attr, WALL_TX_LEFT, WALL_TY_TOP,
                        WALL_TX_RIGHT - WALL_TX_LEFT + 1, 1);
    /* side walls, rows 3..27 */
    VDP_fillTileMapRect(BG_B, attr, WALL_TX_LEFT, WALL_TY_TOP + 1,
                        1, (SCREEN_H / 8) - (WALL_TY_TOP + 1));
    VDP_fillTileMapRect(BG_B, attr, WALL_TX_RIGHT, WALL_TY_TOP + 1,
                        1, (SCREEN_H / 8) - (WALL_TY_TOP + 1));
}

/* Build the tilemap for a full brick wall once */
static void initBrickMap(void)
{
    u16 r, c, i;
    u16 *p = brickMap;

    for (r = 0; r < BRICK_ROWS; r++)
    {
        u16 pal  = PAL1 + (r >> 1);
        u16 base = (r & 1) ? TILE_BRICK_B : TILE_BRICK_A;

        for (c = 0; c < BRICK_COLS; c++)
            for (i = 0; i < BRICK_TW; i++)
                *p++ = TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE, base + i);
    }
}

static void fillBricks(void)
{
    memset(bricks, 1, sizeof(bricks));

    VDP_setTileMapDataRect(BG_A, brickMap, BRICK_TX0, BRICK_TY0,
                           BRICK_MAP_W, BRICK_ROWS, BRICK_MAP_W, CPU);

    bricksLeft = BRICK_ROWS * BRICK_COLS;
    bricksHitThisLevel = 0;
}

/* Redraw only the HUD fields whose value changed */
static void drawHud(void)
{
    char buf[12];

    if (hudFull)
    {
        VDP_drawText("SCORE", 1, 0);
        VDP_drawText("LEVEL", 16, 0);
        VDP_drawText("LIVES", 30, 0);
    }

    if (hudFull || (score != hudScore))
    {
        uintToStr(score, buf, 6);
        VDP_drawText(buf, 7, 0);
        hudScore = score;
    }

    if (hudFull || (level != hudLevel))
    {
        uintToStr(level, buf, 2);
        VDP_drawText(buf, 22, 0);
        hudLevel = level;
    }

    if (hudFull || (lives != hudLives))
    {
        uintToStr(lives, buf, 1);
        VDP_clearText(36, 0, 3);
        VDP_drawText(buf, 36, 0);
        hudLives = lives;
    }

    hudFull  = FALSE;
    hudDirty = FALSE;
}

/* Clear only the cells used by the message currently shown */
static void clearMessage(void)
{
    if (msgLen)
    {
        VDP_clearText(msgX, MSG_ROW, msgLen);
        msgLen = 0;
    }
    msgText = NULL;
}

static void showMessage(const char *msg)
{
    u16 len;

    if (msg == msgText) return;     /* already on screen */

    clearMessage();
    len = strlen(msg);
    msgX = 20 - (len >> 1);
    VDP_drawText(msg, msgX, MSG_ROW);
    msgLen  = len;
    msgText = msg;
}

/* Invalidate the screen caches after the text plane was cleared */
static void textPlaneCleared(void)
{
    msgText = NULL;
    msgLen  = 0;
    hudFull = TRUE;
}

static void updateSprites(void)
{
    s16 bx = (s16)(ballX >> FP);
    s16 by = (s16)(ballY >> FP);

    VDP_setSpriteFull(0, paddleX, PADDLE_Y, SPRITE_SIZE(4, 1),
                      TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_PADDLE), 1);
    VDP_setSpriteFull(1, bx, by, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_BALL), 0);
    VDP_updateSprites(2, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
/* Game logic                                                          */
/* ------------------------------------------------------------------ */

static s16 levelSpeed(u16 lvl)
{
    s16 s = SPEED_START + (s16)((lvl - 1) * 0x20);
    if (s > SPEED_MAX) s = SPEED_MAX;
    return s;
}

static void stickBallToPaddle(void)
{
    ballX = (s32)(paddleX + (PADDLE_W / 2) - (BALL_SIZE / 2)) << FP;
    ballY = (s32)(PADDLE_Y - BALL_SIZE) << FP;
    velX = 0;
    velY = 0;
}

static char *appendStr(char *dst, const char *src)
{
    while (*src) *dst++ = *src++;
    *dst = 0;
    return dst;
}

static void startServe(void)
{
    state = ST_SERVE;
    stickBallToPaddle();

    /* Rebuild the text only when the level changed */
    if (serveMsgLevel != level)
    {
        char *p = appendStr(serveMsg, "LEVEL ");
        uintToStr(level, p, 1);
        while (*p) p++;
        appendStr(p, " - PRESS A");

        serveMsgLevel = level;
        if (msgText == serveMsg)
            msgText = NULL;         /* content changed: force redraw */
    }

    showMessage(serveMsg);
}

static void newGame(void)
{
    score = 0;
    lives = 3;
    level = 1;
    speed = levelSpeed(level);
    paddleX = (FIELD_L + FIELD_R - PADDLE_W) / 2;

    VDP_clearPlane(BG_A, TRUE);
    textPlaneCleared();
    fillBricks();
    hudDirty = TRUE;
    startServe();
}

static void nextLevel(void)
{
    level++;
    speed = levelSpeed(level);
    fillBricks();
    hudDirty = TRUE;
    startServe();
}

static void loseLife(void)
{
    if (lives > 0) lives--;
    hudDirty = TRUE;

    if (lives == 0)
    {
        state = ST_GAMEOVER;
        ballY = (s32)(SCREEN_H + 16) << FP;     /* keep it off screen */
        velX = velY = 0;
        showMessage("GAME OVER - PRESS START");
    }
    else
    {
        speed = levelSpeed(level);
        startServe();
    }
}

static void removeBrick(u16 row, u16 col)
{
    bricks[row][col] = 0;
    VDP_clearTileMapRect(BG_A, BRICK_TX0 + (col * BRICK_TW), BRICK_TY0 + row, BRICK_TW, 1);

    bricksLeft--;
    bricksHitThisLevel++;
    score += (BRICK_ROWS - row) * 10;
    hudDirty = TRUE;

    /* Speed up a bit every 8 bricks */
    if (((bricksHitThisLevel & 7) == 0) && (speed < SPEED_MAX))
        speed += SPEED_STEP;
}

/* Check one point against the brick grid, remove brick if hit */
static bool checkBrickPoint(s16 px, s16 py)
{
    u16 dx, dy, col, row;

    if (px < BRICK_X0 || px >= BRICK_X0 + (BRICK_COLS * BRICK_PW)) return FALSE;
    if (py < BRICK_Y0 || py >= BRICK_Y0 + (BRICK_ROWS * BRICK_PH)) return FALSE;

    dx  = (u16)(px - BRICK_X0);             /* 0..239 */
    dy  = (u16)(py - BRICK_Y0);             /* 0..47  */
    col = brickColOfTile[dx >> 3];          /* dx / 24 */
    row = dy >> 3;                          /* dy / 8  */

    if (bricks[row][col])
    {
        removeBrick(row, col);
        return TRUE;
    }
    return FALSE;
}

/* Test the four corners of the ball box */
static bool hitBricksAt(s16 x, s16 y)
{
    bool hit = FALSE;

    hit |= checkBrickPoint(x, y);
    hit |= checkBrickPoint(x + BALL_SIZE - 1, y);
    hit |= checkBrickPoint(x, y + BALL_SIZE - 1);
    hit |= checkBrickPoint(x + BALL_SIZE - 1, y + BALL_SIZE - 1);

    return hit;
}

static void movePaddle(u16 pad)
{
    if (pad & BUTTON_LEFT)  paddleX -= PADDLE_SPEED;
    if (pad & BUTTON_RIGHT) paddleX += PADDLE_SPEED;

    if (paddleX < FIELD_L) paddleX = FIELD_L;
    if (paddleX > FIELD_R - PADDLE_W) paddleX = FIELD_R - PADDLE_W;
}

static void launchBall(void)
{
    s16 half = speed >> 1;          /* speed is always positive */

    velY = -speed;
    velX = (random() & 1) ? half : -half;
    state = ST_PLAY;
    clearMessage();
}

static void bounceOnPaddle(s16 bx)
{
    /* offset is within -18..18, speed within 0x1C0..0x340:
       |offset * speed| <= 14976, fits in 16 bits */
    s16 offset = (bx + (BALL_SIZE / 2)) - (paddleX + (PADDLE_W / 2));
    s16 prod   = offset * speed;
    u16 mag    = (prod < 0) ? (u16)(-prod) : (u16)prod;
    /* mag / 12, exact for any u16 (43691 = ceil(2^19 / 12)) */
    s16 q      = (s16)(((u32)mag * 43691u) >> 19);
    s16 vx     = (prod < 0) ? -q : q;   /* truncate toward zero, like '/' */
    s16 maxVx  = speed + (speed >> 1);

    if (vx >  maxVx) vx =  maxVx;
    if (vx < -maxVx) vx = -maxVx;

    velX  = vx;
    velY  = -speed;
    ballY = (s32)(PADDLE_Y - BALL_SIZE) << FP;
}

static void updateBall(void)
{
    s16 bx, by;
    s16 oldBottom = (s16)(ballY >> FP) + BALL_SIZE;

    /* ---- Horizontal movement ---- */
    ballX += velX;
    bx = (s16)(ballX >> FP);
    by = (s16)(ballY >> FP);

    if (bx < FIELD_L)
    {
        ballX = (s32)FIELD_L << FP;
        if (velX < 0) velX = -velX;
    }
    else if (bx + BALL_SIZE > FIELD_R)
    {
        ballX = (s32)(FIELD_R - BALL_SIZE) << FP;
        if (velX > 0) velX = -velX;
    }
    else if (hitBricksAt(bx, by))
    {
        ballX -= velX;
        velX = -velX;
    }

    /* ---- Vertical movement ---- */
    ballY += velY;
    bx = (s16)(ballX >> FP);
    by = (s16)(ballY >> FP);

    if (by < FIELD_T)
    {
        ballY = (s32)FIELD_T << FP;
        if (velY < 0) velY = -velY;
    }
    else if (hitBricksAt(bx, by))
    {
        ballY -= velY;
        velY = -velY;
    }

    /* ---- Paddle ---- */
    by = (s16)(ballY >> FP);
    if (velY > 0)
    {
        s16 newBottom = by + BALL_SIZE;

        if ((oldBottom <= PADDLE_Y) && (newBottom >= PADDLE_Y) &&
            (bx + BALL_SIZE > paddleX) && (bx < paddleX + PADDLE_W))
        {
            bounceOnPaddle(bx);
        }
    }

    /* ---- Lost ball ---- */
    if (by > SCREEN_H)
    {
        loseLife();
        return;
    }

    /* ---- Level cleared ---- */
    if (bricksLeft == 0)
        nextLevel();
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setBackgroundColor(0);

    setupPalettes();
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);

    VDP_loadTileData(tileData, TILE_USER_INDEX, NUM_TILES, DMA);

    initBrickMap();
    serveMsgLevel = 0;

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    textPlaneCleared();
    drawWalls();

    prevPad = 0;
    newGame();

    while (TRUE)
    {
        u16 pad     = JOY_readJoypad(JOY_1);
        u16 pressed = pad & ~prevPad;
        prevPad = pad;

        switch (state)
        {
            case ST_SERVE:
                movePaddle(pad);
                stickBallToPaddle();
                if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
                    launchBall();
                break;

            case ST_PLAY:
                if (pressed & BUTTON_START)
                {
                    state = ST_PAUSED;
                    showMessage("PAUSED");
                }
                else
                {
                    movePaddle(pad);
                    updateBall();
                }
                break;

            case ST_PAUSED:
                if (pressed & BUTTON_START)
                {
                    state = ST_PLAY;
                    clearMessage();
                }
                break;

            case ST_GAMEOVER:
                if (pressed & BUTTON_START)
                    newGame();
                break;
        }

        if (hudDirty) drawHud();
        updateSprites();

        SYS_doVBlankProcess();
    }

    return 0;
}
