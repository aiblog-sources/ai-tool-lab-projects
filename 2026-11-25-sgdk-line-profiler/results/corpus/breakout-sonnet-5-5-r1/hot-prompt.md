Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 6,158 cycles per frame on average (4.8%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,582 /frame  58.2% of busy  src/main.c:400  SYS_doVBlankProcess();
                     3,564 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,564 cycles each
      950 /frame  15.4% of busy  src/main.c:338  VDP_updateSprites(2, DMA_QUEUE);
                       794 in DMA_queueDmaFast: 1.00 calls/frame, 794 cycles each
      318 /frame   5.2% of busy  [no game line]:0  
                       124 in no_bmp_task
                       124 in no_user_task
                        30 in no_xgm_task
      168 /frame   2.7% of busy  src/main.c:225  if (px < 0 || py < 0) return FALSE;
                         0 in hitBrickAt.part.0
      106 /frame   1.7% of busy  src/main.c:336  VDP_setSpriteFull(1, ballX, ballY, SPRITE_SIZE(1, 1),
       92 /frame   1.5% of busy  src/main.c:334  VDP_setSpriteFull(0, paddleX, PADDLE_Y, SPRITE_SIZE(4, 1),
       76 /frame   1.2% of busy  src/main.c:227  ty = py >> 3;
       76 /frame   1.2% of busy  src/main.c:241  }
       76 /frame   1.2% of busy  src/main.c:221  static bool hitBrickAt(s16 px, s16 py)
       71 /frame   1.2% of busy  src/main.c:228  if (ty < BRICK_ROW0 || ty >= BRICK_ROW0 + BRICK_ROWS) return
       70 /frame   1.1% of busy  src/main.c:247  if (hitBrickAt(ballX,                 ballY))               
                         0 in hitBrickAt.part.0
       59 /frame   1.0% of busy  src/main.c:252  }
       43 /frame   0.7% of busy  src/main.c:245  {
       43 /frame   0.7% of busy  src/main.c:365  if (state == ST_SERVE || state == ST_PLAY)
       31 /frame   0.5% of busy  src/main.c:397  if (hudDirty) drawHud();
                         0 in drawHud

== Lint findings, by measured cost ==
negligible        11 /frame (0.01% of a frame)  src/main.c:160 [text] guarded
negligible        11 /frame (0.01% of a frame)  src/main.c:158 [text] guarded
negligible        11 /frame (0.01% of a frame)  src/main.c:162 [text] guarded
negligible         9 /frame (0.01% of a frame)  src/main.c:164 [text] guarded
negligible         9 /frame (0.01% of a frame)  src/main.c:196 [text] guarded
negligible         4 /frame (0.00% of a frame)  src/main.c:159 [text] guarded
negligible         2 /frame (0.00% of a frame)  src/main.c:163 [text] guarded
negligible         0 /frame (0.00% of a frame)  src/main.c:230 [div] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:183 [tile-loop] heavy
   not run         0 /frame (0.00% of a frame)  src/main.c:295 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,582 /frame  src/main.c:400 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
/*
 * Breakout for Sega Mega Drive / Genesis - SGDK 2.09
 * Single file, no resources: all tiles are const u32 arrays loaded with VDP_loadTileData.
 *
 * Controls (pad 1): LEFT/RIGHT move the paddle, A/B/C/START launches the ball,
 * START restarts after game over or a win.
 */
#include <genesis.h>

#define BRICK_COLS      10
#define BRICK_ROWS      6
#define BRICK_ROW0      4            /* first brick row (in tiles)            */
#define BRICK_TILES_W   4            /* each brick is 4 tiles = 32 px wide    */

#define PADDLE_W        32
#define PADDLE_Y        208
#define PADDLE_SPEED    4
#define PLAY_TOP        16           /* ball bounces below the HUD            */
#define SCREEN_W        320
#define SCREEN_H        224
#define BALL_SIZE       8

/* ---- VRAM tile layout ---- */
#define T_PADDLE        (TILE_USER_INDEX + 0)    /* 4 tiles */
#define T_BALL          (TILE_USER_INDEX + 4)    /* 1 tile  */
#define T_BRICK_A       (TILE_USER_INDEX + 5)    /* 4 tiles */
#define T_BRICK_B       (TILE_USER_INDEX + 9)    /* 4 tiles */
#define TILE_COUNT      13

/* ---- Tile data (8 rows of 8 pixels, one nibble per pixel) ---- */
static const u32 paddleTiles[4 * 8] =
{
    /* left cap */
    0x00000000, 0x00333333, 0x01111111, 0x11111111,
    0x11111111, 0x02222222, 0x00000000, 0x00000000,
    /* middle */
    0x00000000, 0x33333333, 0x11111111, 0x11111111,
    0x11111111, 0x22222222, 0x00000000, 0x00000000,
    /* middle */
    0x00000000, 0x33333333, 0x11111111, 0x11111111,
    0x11111111, 0x22222222, 0x00000000, 0x00000000,
    /* right cap */
    0x00000000, 0x33333300, 0x11111110, 0x11111111,
    0x11111111, 0x22222220, 0x00000000, 0x00000000
};

static const u32 ballTile[8] =
{
    0x00222200,
    0x02333320,
    0x23333332,
    0x23333332,
    0x23333332,
    0x23333332,
    0x02333320,
    0x00222200
};

/* Brick set A: colour indices 4 (face), 5 (highlight), 6 (shadow) */
static const u32 brickTilesA[4 * 8] =
{
    /* left */
    0x55555555, 0x54444444, 0x54444444, 0x54444444,
    0x54444444, 0x54444444, 0x56666666, 0x00000000,
    /* middle */
    0x55555555, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x66666666, 0x00000000,
    /* middle */
    0x55555555, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x66666666, 0x00000000,
    /* right */
    0x55555556, 0x44444446, 0x44444446, 0x44444446,
    0x44444446, 0x44444446, 0x66666666, 0x00000000
};

/* Brick set B: colour indices 7 (face), 8 (highlight), 9 (shadow) */
static const u32 brickTilesB[4 * 8] =
{
    /* left */
    0x88888888, 0x87777777, 0x87777777, 0x87777777,
    0x87777777, 0x87777777, 0x89999999, 0x00000000,
    /* middle */
    0x88888888, 0x77777777, 0x77777777, 0x77777777,
    0x77777777, 0x77777777, 0x99999999, 0x00000000,
    /* middle */
    0x88888888, 0x77777777, 0x77777777, 0x77777777,
    0x77777777, 0x77777777, 0x99999999, 0x00000000,
    /* right */
    0x88888889, 0x77777779, 0x77777779, 0x77777779,
    0x77777779, 0x77777779, 0x99999999, 0x00000000
};

/* ---- Game state ---- */
typedef enum { ST_SERVE, ST_PLAY, ST_GAMEOVER, ST_WIN } GameState;

static u8        bricks[BRICK_ROWS][BRICK_COLS];
static GameState state;
static s16       paddleX;
static s16       ballX, ballY;
static s16       ballVX, ballVY;
static u16       score;
static u16       lives;
static u16       bricksLeft;
static bool      hudDirty;
static u16       prevJoy;

/* ---------------------------------------------------------------- */

static void setupPalettes(void)
{
    u16 i;

    /* PAL0: black background, everything else white (used by the text font) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    for (i = 1; i < 16; i++) PAL_setColor(i, RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* PAL1: sprites (1-3), red bricks (4-6), orange bricks (7-9) */
    PAL_setColor(16 + 0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x40C0FF));
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x2060A0));
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 4, RGB24_TO_VDPCOLOR(0xE02020));
    PAL_setColor(16 + 5, RGB24_TO_VDPCOLOR(0xFF8080));
    PAL_setColor(16 + 6, RGB24_TO_VDPCOLOR(0x801010));
    PAL_setColor(16 + 7, RGB24_TO_VDPCOLOR(0xFF8000));
    PAL_setColor(16 + 8, RGB24_TO_VDPCOLOR(0xFFC080));
    PAL_setColor(16 + 9, RGB24_TO_VDPCOLOR(0xA05000));

    /* PAL2: yellow (4-6), green (7-9) */
    PAL_setColor(32 + 4, RGB24_TO_VDPCOLOR(0xE0E000));
    PAL_setColor(32 + 5, RGB24_TO_VDPCOLOR(0xFFFF90));
    PAL_setColor(32 + 6, RGB24_TO_VDPCOLOR(0x808000));
    PAL_setColor(32 + 7, RGB24_TO_VDPCOLOR(0x20C020));
    PAL_setColor(32 + 8, RGB24_TO_VDPCOLOR(0x90FF90));
    PAL_setColor(32 + 9, RGB24_TO_VDPCOLOR(0x108010));

    /* PAL3: cyan (4-6), purple (7-9) */
    PAL_setColor(48 + 4, RGB24_TO_VDPCOLOR(0x20C0C0));
    PAL_setColor(48 + 5, RGB24_TO_VDPCOLOR(0x90FFFF));
    PAL_setColor(48 + 6, RGB24_TO_VDPCOLOR(0x108080));
    PAL_setColor(48 + 7, RGB24_TO_VDPCOLOR(0xA040E0));
    PAL_setColor(48 + 8, RGB24_TO_VDPCOLOR(0xD090FF));
    PAL_setColor(48 + 9, RGB24_TO_VDPCOLOR(0x602090));
}

static void loadTiles(void)
{
    VDP_loadTileData(paddleTiles, T_PADDLE, 4, CPU);
    VDP_loadTileData(ballTile,    T_BALL,   1, CPU);
    VDP_loadTileData(brickTilesA, T_BRICK_A, 4, CPU);
    VDP_loadTileData(brickTilesB, T_BRICK_B, 4, CPU);
}

static void drawHud(void)
{
    char buf[12];

    VDP_drawText("SCORE", 1, 0);
    intToStr(score, buf, 6);
    VDP_drawText(buf, 7, 0);

    VDP_drawText("LIVES", 30, 0);
    intToStr(lives, buf, 1);
    VDP_drawText(buf, 36, 0);

    hudDirty = FALSE;
}

static void drawBricks(void)
{
    u16 r, c, k;

    for (r = 0; r < BRICK_ROWS; r++)
    {
        u16 pal  = PAL1 + (r >> 1);
        u16 base = (r & 1) ? T_BRICK_B : T_BRICK_A;

        for (c = 0; c < BRICK_COLS; c++)
        {
            bricks[r][c] = 1;
            for (k = 0; k < BRICK_TILES_W; k++)
            {
                VDP_setTileMapXY(BG_A,
                                 TILE_ATTR_FULL(pal, 0, 0, 0, base + k),
                                 c * BRICK_TILES_W + k,
                                 BRICK_ROW0 + r);
            }
        }
    }
    bricksLeft = BRICK_ROWS * BRICK_COLS;
}

static void showMessage(const char *msg, u16 x)
{
    VDP_clearTextArea(8, 16, 24, 2);
    if (msg) VDP_drawText(msg, x, 16);
}

static void placeBallOnPaddle(void)
{
    ballX  = paddleX + (PADDLE_W - BALL_SIZE) / 2;
    ballY  = PADDLE_Y - BALL_SIZE;
    ballVX = 0;
    ballVY = 0;
}

static void newGame(void)
{
    VDP_clearPlane(BG_A, TRUE);
    score   = 0;
    lives   = 3;
    paddleX = (SCREEN_W - PADDLE_W) / 2;
    drawBricks();
    placeBallOnPaddle();
    state = ST_SERVE;
    drawHud();
    showMessage("PRESS A TO LAUNCH", 11);
}

/* Checks one pixel; removes the brick under it. Returns TRUE if one was hit. */
static bool hitBrickAt(s16 px, s16 py)
{
    s16 tx, ty, col, row;

    if (px < 0 || py < 0) return FALSE;
    tx = px >> 3;
    ty = py >> 3;
    if (ty < BRICK_ROW0 || ty >= BRICK_ROW0 + BRICK_ROWS) return FALSE;

    col = tx / BRICK_TILES_W;
    row = ty - BRICK_ROW0;
    if (col >= BRICK_COLS) return FALSE;
    if (!bricks[row][col]) return FALSE;

    bricks[row][col] = 0;
    VDP_clearTileMapRect(BG_A, col * BRICK_TILES_W, ty, BRICK_TILES_W, 1);
    score += 10;
    bricksLeft--;
    hudDirty = TRUE;
    return TRUE;
}

/* Tests the four corners of the ball against the brick grid. */
static bool ballHitsBrick(void)
{
    bool hit = FALSE;
    if (hitBrickAt(ballX,                 ballY))                 hit = TRUE;
    if (hitBrickAt(ballX + BALL_SIZE - 1, ballY))                 hit = TRUE;
    if (hitBrickAt(ballX,                 ballY + BALL_SIZE - 1)) hit = TRUE;
    if (hitBrickAt(ballX + BALL_SIZE - 1, ballY + BALL_SIZE - 1)) hit = TRUE;
    return hit;
}

static void updateBall(void)
{
    /* ---- horizontal ---- */
    ballX += ballVX;
    if (ballX < 0)
    {
        ballX = 0;
        ballVX = -ballVX;
    }
    else if (ballX > SCREEN_W - BALL_SIZE)
    {
        ballX = SCREEN_W - BALL_SIZE;
        ballVX = -ballVX;
    }
    else if (ballHitsBrick())
    {
        ballX -= ballVX;
        ballVX = -ballVX;
    }

    /* ---- vertical ---- */
    ballY += ballVY;
    if (ballY < PLAY_TOP)
    {
        ballY = PLAY_TOP;
        ballVY = -ballVY;
    }
    else if (ballHitsBrick())
    {
        ballY -= ballVY;
        ballVY = -ballVY;
    }

    /* ---- paddle ---- */
    if (ballVY > 0 &&
        ballY + BALL_SIZE >= PADDLE_Y &&
        ballY + BALL_SIZE - ballVY <= PADDLE_Y + 2 &&
        ballX + BALL_SIZE > paddleX &&
        ballX < paddleX + PADDLE_W)
    {
        s16 offset = (ballX + BALL_SIZE / 2) - (paddleX + PADDLE_W / 2);  /* about -20..20 */
        s16 vx = offset / 6;
        if (vx == 0) vx = (offset < 0) ? -1 : 1;
        ballVX = vx;
        ballVY = -2;
        ballY  = PADDLE_Y - BALL_SIZE;
    }

    /* ---- win ---- */
    if (bricksLeft == 0)
    {
        state = ST_WIN;
        ballVX = ballVY = 0;
        showMessage("YOU WIN! PRESS START", 10);
        return;
    }

    /* ---- ball lost ---- */
    if (ballY > SCREEN_H)
    {
        lives--;
        hudDirty = TRUE;
        if (lives == 0)
        {
            state = ST_GAMEOVER;
            ballVX = ballVY = 0;
            ballY = SCREEN_H + 16;      /* keep it off screen */
            showMessage("GAME OVER - PRESS START", 8);
        }
        else
        {
            placeBallOnPaddle();
            state = ST_SERVE;
            showMessage("PRESS A TO LAUNCH", 11);
        }
    }
}

static void updateSprites(void)
{
    VDP_setSpriteFull(0, paddleX, PADDLE_Y, SPRITE_SIZE(4, 1),
                      TILE_ATTR_FULL(PAL1, 0, 0, 0, T_PADDLE), 1);
    VDP_setSpriteFull(1, ballX, ballY, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, 0, 0, 0, T_BALL), 0);
    VDP_updateSprites(2, DMA_QUEUE);
}

/* ---------------------------------------------------------------- */

int main(bool hardReset)
{
    u16 joy, pressed;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setTextPalette(PAL0);
    setupPalettes();
    loadTiles();
    VDP_resetSprites();

    prevJoy = 0;
    newGame();

    while (TRUE)
    {
        joy     = JOY_readJoypad(JOY_1);
        pressed = joy & ~prevJoy;
        prevJoy = joy;

        /* paddle movement */
        if (state == ST_SERVE || state == ST_PLAY)
        {
            if (joy & BUTTON_LEFT)  paddleX -= PADDLE_SPEED;
            if (joy & BUTTON_RIGHT) paddleX += PADDLE_SPEED;
            if (paddleX < 0) paddleX = 0;
            if (paddleX > SCREEN_W - PADDLE_W) paddleX = SCREEN_W - PADDLE_W;
        }

        switch (state)
        {
        case ST_SERVE:
            ballX = paddleX + (PADDLE_W - BALL_SIZE) / 2;
            ballY = PADDLE_Y - BALL_SIZE;
            if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
            {
                ballVX = (random() & 1) ? 1 : -1;
                ballVY = -2;
                state  = ST_PLAY;
                showMessage(NULL, 0);
            }
            break;

        case ST_PLAY:
            updateBall();
            break;

        case ST_GAMEOVER:
        case ST_WIN:
            if (pressed & BUTTON_START) newGame();
            break;
        }

        if (hudDirty) drawHud();

        updateSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
