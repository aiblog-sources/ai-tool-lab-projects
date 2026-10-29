```c
#include <genesis.h>

/* ------------------------------------------------------------------ */
/*  Configuration                                                     */
/* ------------------------------------------------------------------ */
#define SCR_W               320
#define SCR_H               224
#define PLAY_TOP            16      /* ball bounces off this line (below HUD) */

#define BRICK_COLS          10
#define BRICK_ROWS          6
#define BRICK_TILE_W        3       /* brick size in tiles                */
#define BRICK_TILE_H        2
#define BRICK_TILE_X_STEP   4       /* 3 tiles brick + 1 tile gap         */
#define BRICK_TILE_Y0       4       /* first brick row at tile y = 4      */
#define BRICK_TILE_Y_STEP   3       /* 2 tiles brick + 1 tile gap         */

#define BRICK_PX_W          24
#define BRICK_PX_H          16
#define BRICK_PX_X_STEP    32
#define BRICK_PX_Y0        32
#define BRICK_PX_Y_STEP    24

#define PADDLE_W            32
#define PADDLE_H            8
#define PADDLE_Y            200
#define PADDLE_SPEED        4

#define BALL_SIZE           8
#define BALL_VY             (-32)   /* velocities in 1/16 px per frame    */
#define BALL_VX_BASE        24

#define START_LIVES         3

/* Tile layout relative to TILE_USERINDEX */
#define T_BRICK_BASE        0       /* 6 tiles, one per brick row colour  */
#define T_PADDLE_BASE       6       /* 4 tiles (SPRITE_SIZE(4,1))         */
#define T_BALL              10      /* 1 tile                             */
#define TILE_COUNT          11

/* ------------------------------------------------------------------ */
/*  Palettes (Mega Drive colour format 0x0BGR, 3 bits per channel)    */
/* ------------------------------------------------------------------ */
static const u16 palBG[16] = {
    0x0000,                 /* 0: backdrop (black)            */
    0x000E,                 /* 1: red                         */
    0x008E,                 /* 2: orange                      */
    0x00EE,                 /* 3: yellow                      */
    0x00E0,                 /* 4: green                       */
    0x0EE0,                 /* 5: cyan                        */
    0x0E00,                 /* 6: blue                        */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palSprite[16] = {
    0x0000,                 /* 0: transparent                 */
    0x0AAA,                 /* 1: paddle (grey)               */
    0x00EE,                 /* 2: ball (yellow)               */
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palText[16] = {
    0x0000,                 /* 0: background                  */
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE,
    0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE, 0x0EEE
};

/* ------------------------------------------------------------------ */
/*  Tile data: 11 tiles x 8 rows (one u32 per 8-pixel row, 4bpp)      */
/* ------------------------------------------------------------------ */
static const u32 gameTiles[TILE_COUNT * 8] = {
    /* 0: brick row 0 (colour index 1) */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* 1: brick row 1 (colour index 2) */
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    /* 2: brick row 2 (colour index 3) */
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    0x33333333, 0x33333333, 0x33333333, 0x33333333,
    /* 3: brick row 3 (colour index 4) */
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    /* 4: brick row 4 (colour index 5) */
    0x55555555, 0x55555555, 0x55555555, 0x55555555,
    0x55555555, 0x55555555, 0x55555555, 0x55555555,
    /* 5: brick row 5 (colour index 6) */
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    0x66666666, 0x66666666, 0x66666666, 0x66666666,
    /* 6-9: paddle, 4 solid tiles (colour index 1 in PAL1) */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    /* 10: ball, round shape (colour index 2 in PAL1) */
    0x00222200,
    0x02222220,
    0x22222222,
    0x22222222,
    0x22222222,
    0x22222222,
    0x02222220,
    0x00222200
};

/* ------------------------------------------------------------------ */
/*  Game state                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    ST_SERVE,
    ST_PLAY,
    ST_OVER
} GameState;

static u8        bricks[BRICK_ROWS][BRICK_COLS];
static u16       bricksLeft;
static u16       score;
static u8        lives;
static u8        hudDirty;
static s16       paddleX;
static s32       ballX, ballY;      /* 1/16 px units */
static s16       ballVX, ballVY;    /* 1/16 px per frame */
static GameState state;
static u16       prevJoy;
static u16       frameCount;

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */
static s16 iabs(s16 v)
{
    return (v < 0) ? -v : v;
}

static void showMessage(const char *msg)
{
    char buf[25];
    u16 i = 0;

    while (i < 24 && msg[i]) {
        buf[i] = msg[i];
        i++;
    }
    while (i < 24) {
        buf[i++] = ' ';
    }
    buf[24] = 0;
    VDP_drawText(buf, 8, 26);
}

static void drawHUD(void)
{
    char s[] = "SCORE 00000";
    char l[] = "LIVES 0";
    u16 v = score;

    for (s16 i = 10; i >= 6; i--) {
        s[i] = (char)('0' + (v % 10));
        v /= 10;
    }
    l[6] = (char)('0' + lives);

    VDP_drawText(s, 1, 0);
    VDP_drawText(l, 30, 0);
}

/* ------------------------------------------------------------------ */
/*  Bricks                                                            */
/* ------------------------------------------------------------------ */
static void drawBrick(u8 r, u8 c)
{
    u16 tx = c * BRICK_TILE_X_STEP;
    u16 ty = BRICK_TILE_Y0 + r * BRICK_TILE_Y_STEP;
    u16 tile = TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_USERINDEX + T_BRICK_BASE + r);

    for (u16 y = 0; y < BRICK_TILE_H; y++) {
        for (u16 x = 0; x < BRICK_TILE_W; x++) {
            VDP_setTileMapXY(BG_B, tile, tx + x, ty + y);
        }
    }
}

static void clearBrick(u8 r, u8 c)
{
    u16 tx = c * BRICK_TILE_X_STEP;
    u16 ty = BRICK_TILE_Y0 + r * BRICK_TILE_Y_STEP;

    for (u16 y = 0; y < BRICK_TILE_H; y++) {
        for (u16 x = 0; x < BRICK_TILE_W; x++) {
            VDP_setTileMapXY(BG_B, 0, tx + x, ty + y);
        }
    }
}

static void resetBoard(void)
{
    for (u8 r = 0; r < BRICK_ROWS; r++) {
        for (u8 c = 0; c < BRICK_COLS; c++) {
            bricks[r][c] = 1;
            drawBrick(r, c);
        }
    }
    bricksLeft = BRICK_ROWS * BRICK_COLS;
}

/* Tests the ball against all bricks; handles at most one hit per frame. */
static void checkBricks(s16 bxp, s16 byp)
{
    for (u8 r = 0; r < BRICK_ROWS; r++) {
        for (u8 c = 0; c < BRICK_COLS; c++) {
            s16 brx, bry, dx, dy, ox, oy;

            if (!bricks[r][c]) {
                continue;
            }

            brx = c * BRICK_PX_X_STEP;
            bry = BRICK_PX_Y0 + r * BRICK_PX_Y_STEP;

            /* Centre distances; half sizes: ball 4, brick 12 x 8 */
            dx = (bxp + BALL_SIZE / 2) - (brx + BRICK_PX_W / 2);
            dy = (byp + BALL_SIZE / 2) - (bry + BRICK_PX_H / 2);
            ox = (BALL_SIZE / 2 + BRICK_PX_W / 2) - iabs(dx);
            oy = (BALL_SIZE / 2 + BRICK_PX_H / 2) - iabs(dy);

            if (ox <= 0 || oy <= 0) {
                continue;
            }

            /* Reflect on the axis of least penetration and push out */
            if (ox < oy) {
                ballVX = -ballVX;
                ballX += (s32)(dx > 0 ? ox : -ox) * 16;
            } else {
                ballVY = -ballVY;
                ballY += (s32)(dy > 0 ? oy : -oy) * 16;
            }

            bricks[r][c] = 0;
            clearBrick(r, c);
            bricksLeft--;
            score += (BRICK_ROWS - r) * 10;   /* top rows are worth more */
            hudDirty = 1;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Ball / paddle / lives                                             */
/* ------------------------------------------------------------------ */
static void placeBallOnPaddle(void)
{
    ballX = (s32)(paddleX + PADDLE_W / 2 - BALL_SIZE / 2) << 4;
    ballY = (s32)(PADDLE_Y - BALL_SIZE) << 4;
}

static void loseLife(void)
{
    if (lives > 0) {
        lives--;
    }
    hudDirty = 1;

    if (lives == 0) {
        state = ST_OVER;
        showMessage("GAME OVER - PRESS START");
    } else {
        state = ST_SERVE;
        showMessage("PRESS A TO LAUNCH");
    }
    placeBallOnPaddle();
}

static void stepBall(void)
{
    s16 bxp, byp;

    ballX += ballVX;
    ballY += ballVY;

    bxp = (s16)(ballX >> 4);
    byp = (s16)(ballY >> 4);

    /* Side walls */
    if (bxp < 0) {
        bxp = 0;
        ballX = 0;
        if (ballVX < 0) ballVX = -ballVX;
    } else if (bxp > SCR_W - BALL_SIZE) {
        bxp = SCR_W - BALL_SIZE;
        ballX = (s32)bxp << 4;
        if (ballVX > 0) ballVX = -ballVX;
    }

    /* Top wall (under the HUD) */
    if (byp < PLAY_TOP) {
        byp = PLAY_TOP;
        ballY = (s32)byp << 4;
        if (ballVY < 0) ballVY = -ballVY;
    }

    /* Paddle */
    if (ballVY > 0 &&
        byp + BALL_SIZE >= PADDLE_Y && byp < PADDLE_Y + PADDLE_H &&
        bxp + BALL_SIZE > paddleX && bxp < paddleX + PADDLE_W) {

        /* Hit position decides the outgoing angle */
        s16 off = (bxp + BALL_SIZE / 2) - (paddleX + PADDLE_W / 2);

        byp = PADDLE_Y - BALL_SIZE;
        ballY = (s32)byp << 4;
        ballVY = BALL_VY;
        ballVX = off * 2;
    }

    /* Bricks */
    checkBricks(bxp, byp);

    /* Board cleared */
    if (bricksLeft == 0) {
        resetBoard();
        state = ST_SERVE;
        placeBallOnPaddle();
        showMessage("LEVEL CLEAR - PRESS A");
        return;
    }

    /* Ball fell past the paddle */
    if (byp >= SCR_H) {
        loseLife();
    }
}

/* ------------------------------------------------------------------ */
/*  Game flow                                                         */
/* ------------------------------------------------------------------ */
static void newGame(void)
{
    score = 0;
    lives = START_LIVES;
    hudDirty = 1;
    resetBoard();
    state = ST_SERVE;
    placeBallOnPaddle();
    showMessage("PRESS A TO LAUNCH");
}

static void gameFrame(u16 joy, u16 pressed)
{
    frameCount++;

    /* Paddle movement (d-pad left/right) */
    if (joy & BUTTON_LEFT)  paddleX -= PADDLE_SPEED;
    if (joy & BUTTON_RIGHT) paddleX += PADDLE_SPEED;
    if (paddleX < 0) paddleX = 0;
    if (paddleX > SCR_W - PADDLE_W) paddleX = SCR_W - PADDLE_W;

    switch (state) {
        case ST_SERVE:
            placeBallOnPaddle();
            if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C)) {
                ballVX = (frameCount & 1) ? BALL_VX_BASE : -BALL_VX_BASE;
                ballVY = BALL_VY;
                state = ST_PLAY;
                showMessage("");
            }
            break;

        case ST_PLAY:
            stepBall();
            break;

        case ST_OVER:
            if (pressed & BUTTON_START) {
                newGame();
            }
            break;

        default:
            break;
    }
}

static void updateSprites(void)
{
    s16 bxp = (s16)(ballX >> 4);
    s16 byp = (s16)(ballY >> 4);

    VDP_setSprite(0,
                  paddleX + 128, PADDLE_Y + 128,
                  SPRITE_SIZE(4, 1),
                  TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, TILE_USERINDEX + T_PADDLE_BASE),
                  1);

    VDP_setSprite(1,
                  bxp + 128, byp + 128,
                  SPRITE_SIZE(1, 1),
                  TILE_ATTR_FULL(PAL1, 0, FALSE, FALSE, TILE_USERINDEX + T_BALL),
                  0);

    VDP_updateSprites(2, CPU);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                       */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    (void)hardReset;

    JOY_init();

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setPalette(PAL0, palBG, CPU);
    PAL_setPalette(PAL1, palSprite, CPU);
    PAL_setPalette(PAL2, palText, CPU);
    VDP_setTextPalette(PAL2);

    VDP_loadTileData(gameTiles, TILE_USERINDEX, TILE_COUNT, CPU);

    VDP_clearPlane(BG_A, CPU);
    VDP_clearPlane(BG_B, CPU);

    paddleX = (SCR_W - PADDLE_W) / 2;
    prevJoy = 0;
    frameCount = 0;

    newGame();

    while (TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;

        gameFrame(joy, pressed);

        if (hudDirty) {
            drawHUD();
            hudDirty = 0;
        }

        updateSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
```