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
#define BRICK_AREA_W    (BRICK_COLS * BRICK_PW)     /* 240 */
#define BRICK_AREA_H    (BRICK_ROWS * BRICK_PH)     /* 48  */
#define BRICK_X1        (BRICK_X0 + BRICK_AREA_W)
#define BRICK_Y1        (BRICK_Y0 + BRICK_AREA_H)

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

/* HUD dirty flags */
#define HUD_SCORE       1
#define HUD_LEVEL       2
#define HUD_LIVES       4
#define HUD_LABELS      8
#define HUD_ALL         (HUD_SCORE | HUD_LEVEL | HUD_LIVES | HUD_LABELS)

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

/* pixel x offset inside brick area -> brick column (avoids divide by 24) */
static u8        colLUT[BRICK_AREA_W];

static s16       paddleX;
static s32       ballX, ballY;      /* 8.8 fixed point */
static s16       velX, velY;        /* 8.8 fixed point */
static s16       speed;

/* score kept as decimal digits, least significant first (no divisions) */
#define SCORE_DIGITS    10
static u8        scoreDig[SCORE_DIGITS];
static u16       lives;
static u16       level;

static GameState state;
static u16       hudDirty;
static u16       prevPad;

/* last values pushed to the sprite table */
static s16       lastPaddleX;
static s16       lastBallX, lastBallY;

/* ------------------------------------------------------------------ */
/* Fast math helpers                                                   */
/* ------------------------------------------------------------------ */

/* (a * b) / 12 using native 16-bit muls/divs; caller guarantees the
   product fits in 16 bits signed range so the quotient cannot overflow.
   divs truncates toward zero, same as C division. */
static inline s16 mulDiv12(s16 a, s16 b)
{
    s32 r = a;
    __asm__ ("muls.w %1,%0\n\t"
             "divs.w #12,%0"
             : "+d" (r)
             : "d" (b)
             : "cc");
    return (s16) r;
}

/* ------------------------------------------------------------------ */
/* Score (decimal digits)                                              */
/* ------------------------------------------------------------------ */

static void scoreReset(void)
{
    u16 i;
    for (i = 0; i < SCORE_DIGITS; i++) scoreDig[i] = 0;
}

/* add t * 10 to the score (t <= 9) */
static void scoreAddTens(u16 t)
{
    u16 i = 1;
    u16 d = scoreDig[1] + t;

    while (d >= 10)
    {
        scoreDig[i] = d - 10;
        i++;
        if (i >= SCORE_DIGITS) return;
        d = scoreDig[i] + 1;
    }
    scoreDig[i] = d;
}

/* same output as uintToStr(score, buf, 6) */
static void scoreToStr(char *buf)
{
    u16 n = SCORE_DIGITS;
    u16 j;

    while ((n > 6) && (scoreDig[n - 1] == 0)) n--;
    for (j = 0; j < n; j++)
        buf[j] = '0' + scoreDig[n - 1 - j];
    buf[n] = 0;
}

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
    u16 i;

    for (i = WALL_TX_LEFT; i <= WALL_TX_RIGHT; i++)
        VDP_setTileMapXY(BG_B, attr, i, WALL_TY_TOP);

    for (i = WALL_TY_TOP + 1; i < (SCREEN_H / 8); i++)
    {
        VDP_setTileMapXY(BG_B, attr, WALL_TX_LEFT, i);
        VDP_setTileMapXY(BG_B, attr, WALL_TX_RIGHT, i);
    }
}

static void drawBrick(u16 row, u16 col)
{
    u16 pal  = PAL1 + (row >> 1);
    u16 base = (row & 1) ? TILE_BRICK_B : TILE_BRICK_A;
    u16 tx   = BRICK_TX0 + (col * BRICK_TW);
    u16 ty   = BRICK_TY0 + row;
    u16 i;

    for (i = 0; i < BRICK_TW; i++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, FALSE, FALSE, FALSE, base + i), tx + i, ty);
}

static void fillBricks(void)
{
    u16 r, c;

    for (r = 0; r < BRICK_ROWS; r++)
    {
        for (c = 0; c < BRICK_COLS; c++)
        {
            bricks[r][c] = 1;
            drawBrick(r, c);
        }
    }
    bricksLeft = BRICK_ROWS * BRICK_COLS;
    bricksHitThisLevel = 0;
}

static void drawHud(void)
{
    char buf[12];

    if (hudDirty & HUD_LABELS)
    {
        VDP_drawText("SCORE", 1, 0);
        VDP_drawText("LEVEL", 16, 0);
        VDP_drawText("LIVES", 30, 0);
    }

    if (hudDirty & HUD_SCORE)
    {
        scoreToStr(buf);
        VDP_drawText(buf, 7, 0);
    }

    if (hudDirty & HUD_LEVEL)
    {
        uintToStr(level, buf, 2);
        VDP_drawText(buf, 22, 0);
    }

    if (hudDirty & HUD_LIVES)
    {
        uintToStr(lives, buf, 1);
        VDP_clearText(36, 0, 3);
        VDP_drawText(buf, 36, 0);
    }

    hudDirty = 0;
}

static void clearMessage(void)
{
    VDP_clearText(FIELD_L / 8, MSG_ROW, (FIELD_R - FIELD_L) / 8);
}

static void showMessage(const char *msg)
{
    u16 len = strlen(msg);
    clearMessage();
    VDP_drawText(msg, 20 - (len / 2), MSG_ROW);
}

static void updateSprites(void)
{
    s16 bx = (s16)(ballX >> FP);
    s16 by = (s16)(ballY >> FP);
    bool changed = FALSE;

    if (paddleX != lastPaddleX)
    {
        VDP_setSpriteFull(0, paddleX, PADDLE_Y, SPRITE_SIZE(4, 1),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_PADDLE), 1);
        lastPaddleX = paddleX;
        changed = TRUE;
    }

    if ((bx != lastBallX) || (by != lastBallY))
    {
        VDP_setSpriteFull(1, bx, by, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_BALL), 0);
        lastBallX = bx;
        lastBallY = by;
        changed = TRUE;
    }

    if (changed)
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

static void startServe(void)
{
    char buf[24];
    char num[6];

    state = ST_SERVE;
    stickBallToPaddle();

    strcpy(buf, "LEVEL ");
    uintToStr(level, num, 1);
    strcat(buf, num);
    strcat(buf, " - PRESS A");
    showMessage(buf);
}

static void newGame(void)
{
    scoreReset();
    lives = 3;
    level = 1;
    speed = levelSpeed(level);
    paddleX = (FIELD_L + FIELD_R - PADDLE_W) / 2;

    VDP_clearPlane(BG_A, TRUE);
    fillBricks();
    hudDirty = HUD_ALL;
    startServe();
}

static void nextLevel(void)
{
    level++;
    speed = levelSpeed(level);
    fillBricks();
    hudDirty |= HUD_LEVEL;
    startServe();
}

static void loseLife(void)
{
    if (lives > 0) lives--;
    hudDirty |= HUD_LIVES;

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
    scoreAddTens(BRICK_ROWS - row);         /* (BRICK_ROWS - row) * 10 */
    hudDirty |= HUD_SCORE;

    /* Speed up a bit every 8 bricks */
    if (((bricksHitThisLevel & 7) == 0) && (speed < SPEED_MAX))
        speed += SPEED_STEP;
}

/* Check one point against the brick grid, remove brick if hit */
static bool checkBrickPoint(s16 px, s16 py)
{
    u16 dx = (u16)(px - BRICK_X0);
    u16 dy = (u16)(py - BRICK_Y0);
    u16 row, col;

    if (dx >= BRICK_AREA_W) return FALSE;
    if (dy >= BRICK_AREA_H) return FALSE;

    col = colLUT[dx];
    row = dy >> 3;

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
    bool hit;

    /* whole ball box outside the brick area: no corner can hit */
    if ((x + BALL_SIZE - 1 < BRICK_X0) || (x >= BRICK_X1) ||
        (y + BALL_SIZE - 1 < BRICK_Y0) || (y >= BRICK_Y1))
        return FALSE;

    hit  = checkBrickPoint(x, y);
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
    velY = -speed;
    velX = (random() & 1) ? (speed / 2) : -(speed / 2);
    state = ST_PLAY;
    clearMessage();
}

static void bounceOnPaddle(s16 bx)
{
    /* offset is within [-18, 18] and speed <= 0x340, so the product fits
       in 16 bits: native muls.w / divs.w give the exact same result */
    s16 offset = (bx + (BALL_SIZE / 2)) - (paddleX + (PADDLE_W / 2));
    s16 vx     = mulDiv12(offset, speed);
    s16 maxVx  = speed + (speed / 2);

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
    by = oldBottom - BALL_SIZE;

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
    u16 i, c, n;

    (void)hardReset;

    /* build pixel -> column table */
    c = 0; n = 0;
    for (i = 0; i < BRICK_AREA_W; i++)
    {
        colLUT[i] = c;
        if (++n == BRICK_PW) { n = 0; c++; }
    }

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setBackgroundColor(0);

    setupPalettes();
    VDP_setTextPalette(PAL0);
    VDP_setTextPlane(BG_A);

    VDP_loadTileData(tileData, TILE_USER_INDEX, NUM_TILES, DMA);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    drawWalls();

    /* force first sprite upload */
    lastPaddleX = 0x7FFF;
    lastBallX   = 0x7FFF;
    lastBallY   = 0x7FFF;

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
