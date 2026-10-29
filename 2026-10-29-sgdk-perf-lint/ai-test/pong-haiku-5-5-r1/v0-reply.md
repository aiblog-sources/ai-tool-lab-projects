```c
/*
 * PONG for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - Left paddle: pad 1 d-pad UP/DOWN
 * - Right paddle: simple AI tracking the ball
 * - Ball speeds up 5% on every paddle hit (v += v/20), capped at BALL_SPEED_MAX
 * - Scores drawn at the top of the screen
 * - No resource files: tiles are const u32 arrays loaded with VDP_loadTileData
 */

#include <genesis.h>

/* ---------- Screen / field layout (logical pixels, 320x224) ---------- */
#define SCREEN_W          320
#define SCREEN_H          224
#define FIELD_TOP          32      /* top wall occupies tile row 3 (y 24..31)   */
#define FIELD_BOTTOM      216      /* bottom wall occupies tile row 27 (y 216+) */
#define PADDLE_W            8
#define PADDLE_H           32
#define PADDLE_X_L         16
#define PADDLE_X_R        (SCREEN_W - 16 - PADDLE_W)
#define BALL_SIZE           8
#define PLAYER_SPEED        3       /* px per frame */
#define AI_SPEED            2       /* px per frame (slower than a fast ball) */
#define AI_DEADZONE         2
#define SPR_ORIGIN        128       /* VDP sprite coordinate origin offset */
#define BALL_SPEED_START    2       /* px per frame at serve */
#define BALL_SPEED_MAX     10       /* px per frame cap so the ball stays playable */
#define HIT_SPEEDUP_DIV    20       /* v += v / 20  -> +5% per hit */

#define TO_FIX(v)  ((fix32)(v) * 65536)

/* ---------- Tile graphics: 5 solid 8x8 tiles, color index 1 ----------
 * tile 0      -> ball (8x8)
 * tiles 1..4  -> paddle (8x32 = 4 stacked tiles)
 * Each u32 is one 8-pixel row (4bpp, one nibble per pixel). 0x1 = palette color 1.
 */
static const u32 gfxTiles[5 * 8] =
{
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

/* ---------- Game state ---------- */
static fix32 ballX, ballY, ballVX, ballVY;
static s16   leftY, rightY;
static u16   scoreL, scoreR;
static s16   serveDir;
static u16   tileBall, tilePaddle;
static bool  scoreDirty;

/* ---------- Helpers ---------- */
static void clampPaddle(s16 *y)
{
    if (*y < FIELD_TOP)                 *y = FIELD_TOP;
    if (*y > FIELD_BOTTOM - PADDLE_H)   *y = FIELD_BOTTOM - PADDLE_H;
}

static void drawScore(void)
{
    char buf[8];

    uintToStr(scoreL % 100, buf, 2);
    VDP_drawText(buf, 13, 1);

    VDP_drawText("-", 19, 1);

    uintToStr(scoreR % 100, buf, 2);
    VDP_drawText(buf, 25, 1);
}

static void resetBall(s16 dir)
{
    ballX  = TO_FIX(SCREEN_W / 2 - BALL_SIZE / 2);
    ballY  = TO_FIX((FIELD_TOP + FIELD_BOTTOM) / 2 - BALL_SIZE / 2);
    ballVX = (fix32)dir * TO_FIX(BALL_SPEED_START);
    ballVY = (random() & 1) ? TO_FIX(1) : -TO_FIX(1);
}

static void speedUpBall(void)
{
    /* +5% on each component keeps the direction and scales the speed by 1.05 */
    ballVX += ballVX / HIT_SPEEDUP_DIV;
    ballVY += ballVY / HIT_SPEEDUP_DIV;

    if (ballVX >  TO_FIX(BALL_SPEED_MAX)) ballVX =  TO_FIX(BALL_SPEED_MAX);
    if (ballVX < -TO_FIX(BALL_SPEED_MAX)) ballVX = -TO_FIX(BALL_SPEED_MAX);
    if (ballVY >  TO_FIX(BALL_SPEED_MAX)) ballVY =  TO_FIX(BALL_SPEED_MAX);
    if (ballVY < -TO_FIX(BALL_SPEED_MAX)) ballVY = -TO_FIX(BALL_SPEED_MAX);
}

/* Reflect the ball off a paddle: angle depends on where it hit, then speed up */
static void bounceFromPaddle(s16 paddleTop, s16 newX)
{
    s16 dy = (s16)(ballY >> 16) + BALL_SIZE / 2 - (paddleTop + PADDLE_H / 2);

    ballX  = TO_FIX(newX);
    ballVX = -ballVX;
    ballVY = (fix32)dy * 4096;   /* 1/16 px per px of offset from paddle centre */

    speedUpBall();
}

/* ---------- Per-frame updates ---------- */
static void updateLeftPaddle(u16 pad)
{
    if (pad & BUTTON_UP)   leftY -= PLAYER_SPEED;
    if (pad & BUTTON_DOWN) leftY += PLAYER_SPEED;
    clampPaddle(&leftY);
}

static void updateAI(void)
{
    s16 ballCenter   = (s16)(ballY >> 16) + BALL_SIZE / 2;
    s16 paddleCenter = rightY + PADDLE_H / 2;

    if (ballCenter > paddleCenter + AI_DEADZONE)      rightY += AI_SPEED;
    else if (ballCenter < paddleCenter - AI_DEADZONE) rightY -= AI_SPEED;

    clampPaddle(&rightY);
}

static void updateBall(void)
{
    s16 bx, by;

    ballX += ballVX;
    ballY += ballVY;

    bx = (s16)(ballX >> 16);
    by = (s16)(ballY >> 16);

    /* Top / bottom walls */
    if (by < FIELD_TOP)
    {
        by = FIELD_TOP;
        ballY  = TO_FIX(by);
        ballVY = -ballVY;
    }
    else if (by + BALL_SIZE > FIELD_BOTTOM)
    {
        by = FIELD_BOTTOM - BALL_SIZE;
        ballY  = TO_FIX(by);
        ballVY = -ballVY;
    }

    /* Left paddle (ball moving left) */
    if (ballVX < 0 &&
        bx <= PADDLE_X_L + PADDLE_W && bx + BALL_SIZE >= PADDLE_X_L &&
        by + BALL_SIZE > leftY && by < leftY + PADDLE_H)
    {
        bounceFromPaddle(leftY, PADDLE_X_L + PADDLE_W);
        bx = PADDLE_X_L + PADDLE_W;
    }
    /* Right paddle (ball moving right) */
    else if (ballVX > 0 &&
             bx + BALL_SIZE >= PADDLE_X_R && bx <= PADDLE_X_R + PADDLE_W &&
             by + BALL_SIZE > rightY && by < rightY + PADDLE_H)
    {
        bounceFromPaddle(rightY, PADDLE_X_R - BALL_SIZE);
        bx = PADDLE_X_R - BALL_SIZE;
    }

    /* Scoring: ball left the screen */
    if (bx + BALL_SIZE < 0)
    {
        /* missed by left player -> right scores, serve toward left */
        scoreR++;
        scoreDirty = TRUE;
        resetBall(-1);
    }
    else if (bx >= SCREEN_W)
    {
        /* missed by right player -> left scores, serve toward right */
        scoreL++;
        scoreDirty = TRUE;
        resetBall(1);
    }
}

static void drawSprites(void)
{
    /* Sprite 0: left paddle (8x32) */
    VDP_setSprite(0,
                  PADDLE_X_L + SPR_ORIGIN,
                  leftY + SPR_ORIGIN,
                  SPRITE_SIZE(1, 4),
                  TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tilePaddle),
                  1);

    /* Sprite 1: right paddle (8x32) */
    VDP_setSprite(1,
                  PADDLE_X_R + SPR_ORIGIN,
                  rightY + SPR_ORIGIN,
                  SPRITE_SIZE(1, 4),
                  TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tilePaddle),
                  2);

    /* Sprite 2: ball (8x8) */
    VDP_setSprite(2,
                  (s16)(ballX >> 16) + SPR_ORIGIN,
                  (s16)(ballY >> 16) + SPR_ORIGIN,
                  SPRITE_SIZE(1, 1),
                  TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tileBall),
                  0);

    VDP_updateSprites(3, DMA);
}

/* ---------- Entry point ---------- */
int main(bool hardReset)
{
    u16 pad;
    u16 x, y;

    (void)hardReset;

    JOY_init();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette: 0 = black background, 1 and 15 = white (sprites / walls / text) */
    VDP_setPaletteColor(0,  RGB24_TO_VDPCOLOR(0x000000));
    VDP_setPaletteColor(1,  RGB24_TO_VDPCOLOR(0xFFFFFF));
    VDP_setPaletteColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* Load our 5 tiles after the system font / reserved tiles */
    tileBall   = TILE_USERINDEX;
    VDP_loadTileData(gfxTiles, tileBall, 5, FALSE);
    tilePaddle = tileBall + 1;

    /* Static background on plan B: top and bottom walls + centre net */
    for (x = 0; x < 40; x++)
    {
        VDP_setTileMapXY(VDP_PLAN_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tileBall), x, 3);
        VDP_setTileMapXY(VDP_PLAN_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tileBall), x, 27);
    }
    for (y = 4; y < 27; y += 2)
    {
        VDP_setTileMapXY(VDP_PLAN_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tileBall), 20, y);
    }

    /* Initial game state */
    leftY  = (FIELD_TOP + FIELD_BOTTOM) / 2 - PADDLE_H / 2;
    rightY = leftY;
    scoreL = 0;
    scoreR = 0;
    serveDir = 1;
    resetBall(serveDir);
    scoreDirty = TRUE;

    while (TRUE)
    {
        pad = JOY_readJoypad(JOY_1);

        updateLeftPaddle(pad);
        updateAI();
        updateBall();

        if (scoreDirty)
        {
            drawScore();
            scoreDirty = FALSE;
        }

        drawSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
```