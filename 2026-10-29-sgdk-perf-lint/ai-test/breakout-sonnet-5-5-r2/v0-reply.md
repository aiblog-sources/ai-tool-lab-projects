```c
/*
 * BREAKOUT for Sega Mega Drive / Genesis - SGDK 2.09
 * Single file, no resources: all tiles are const u32 arrays loaded with VDP_loadTileData.
 *
 * Controls (pad 1): LEFT/RIGHT move paddle, A/B/C/START launch ball.
 * Screen: 320x224 (40x28 tiles). Bricks: 10 columns x 6 rows, each brick 3 tiles wide.
 */

#include <genesis.h>

/* ------------------------------------------------------------------------- */
/* Tile data (8x8, 4bpp, one nibble per pixel = palette index)                */
/* ------------------------------------------------------------------------- */

/* Brick: index 7 = dark outline, c = fill color */
#define BRICK_TILE(c) \
    0x77777777UL, \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    (((u32)(c) * 0x01111110UL) | 0x70000007UL), \
    0x77777777UL

static const u32 brickTiles[6 * 8] =
{
    BRICK_TILE(1), BRICK_TILE(2), BRICK_TILE(3),
    BRICK_TILE(4), BRICK_TILE(5), BRICK_TILE(6)
};

static const u32 ballTile[8] =
{
    0x00888800,
    0x08888880,
    0x88888888,
    0x88888888,
    0x88888888,
    0x88888888,
    0x08888880,
    0x00888800
};

/* Paddle: 4 tiles side by side (32x8): left cap, mid, mid, right cap */
static const u32 paddleTiles[4 * 8] =
{
    /* left cap */
    0x0AAAAAAA, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x07777777,
    /* mid */
    0xAAAAAAAA, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x77777777,
    /* mid */
    0xAAAAAAAA, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x77777777,
    /* right cap */
    0xAAAAAAA0, 0x99999999, 0x99999999, 0x99999999,
    0x99999999, 0x99999999, 0x99999999, 0x77777770
};

static const u32 wallTile[8] =
{
    0xBBBBBBBB, 0xBBBBBBBB, 0xBBBBBBBB, 0xBBBBBBBB,
    0xBBBBBBBB, 0xBBBBBBBB, 0xBBBBBBBB, 0x77777777
};

/* Palette for PAL1 (VDP format 0x0BGR) */
static const u16 gamePalette[16] =
{
    0x0000, /* 0 transparent */
    0x000E, /* 1 red */
    0x006E, /* 2 orange */
    0x00EE, /* 3 yellow */
    0x00E0, /* 4 green */
    0x0EE0, /* 5 cyan */
    0x0E0A, /* 6 purple */
    0x0222, /* 7 dark outline */
    0x0EEE, /* 8 white (ball) */
    0x0888, /* 9 paddle body */
    0x0CCC, /* A paddle highlight */
    0x0A64, /* B wall */
    0x0000, 0x0000, 0x0000, 0x0000
};

/* VRAM tile layout */
#define TILE_BRICK   (TILE_USER_INDEX)        /* 6 tiles */
#define TILE_BALL    (TILE_BRICK + 6)         /* 1 tile  */
#define TILE_PADDLE  (TILE_BALL + 1)          /* 4 tiles */
#define TILE_WALL    (TILE_PADDLE + 4)        /* 1 tile  */

/* ------------------------------------------------------------------------- */
/* Game constants                                                            */
/* ------------------------------------------------------------------------- */

#define BRICK_COLS    10
#define BRICK_ROWS    6
#define BRICK_W_TILES 3
#define BRICK_X0      5          /* first brick column, in tiles  */
#define BRICK_Y0      4          /* first brick row, in tiles     */

#define FIELD_TOP     16         /* pixel Y of the top wall bottom edge */
#define BALL_SIZE     8
#define BALL_MAX_X    (320 - BALL_SIZE)
#define PADDLE_Y      200
#define PADDLE_W      32
#define PADDLE_SPEED  4
#define START_LIVES   3

#define MSG_ROW_TITLE 12
#define MSG_ROW_INFO  16

typedef enum
{
    ST_TITLE,
    ST_SERVE,
    ST_PLAY,
    ST_OVER,
    ST_WIN
} GameState;

/* ------------------------------------------------------------------------- */
/* Game variables                                                            */
/* ------------------------------------------------------------------------- */

static u8 bricks[BRICK_ROWS][BRICK_COLS];
static GameState state;
static u16 prevPad;

static s16 padX;                 /* paddle X in pixels            */
static s32 ballX, ballY;         /* ball position, 24.8 fixed     */
static s32 ballVX, ballVY;       /* ball velocity, 24.8 fixed     */

static u16 score;
static u16 lives;
static u16 bricksLeft;
static bool hudDirty;

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void showMessage(const char *s, u16 row)
{
    u16 len = strlen(s);
    VDP_clearTextLine(row);
    VDP_drawText(s, (40 - len) / 2, row);
}

static void clearMessages(void)
{
    VDP_clearTextLine(MSG_ROW_TITLE);
    VDP_clearTextLine(MSG_ROW_INFO);
}

static void drawHUD(void)
{
    char buf[8];

    VDP_drawText("SCORE", 1, 0);
    uintToStr(score, buf, 5);
    VDP_drawText(buf, 7, 0);

    VDP_drawText("LIVES", 28, 0);
    uintToStr(lives, buf, 1);
    VDP_drawText(buf, 34, 0);

    hudDirty = FALSE;
}

static void placeBallOnPaddle(void)
{
    ballX = (s32)(padX + (PADDLE_W / 2) - (BALL_SIZE / 2)) << 8;
    ballY = (s32)(PADDLE_Y - BALL_SIZE) << 8;
    ballVX = 0;
    ballVY = 0;
}

static s32 currentSpeed(void)
{
    /* Gets faster as bricks disappear (24.8 fixed, ~1.3 .. 2.5 px/frame) */
    return 340 + (s32)(BRICK_ROWS * BRICK_COLS - bricksLeft) * 5;
}

/* Test one pixel against the brick grid; remove the brick if hit. */
static bool hitBrickPoint(s16 x, s16 y)
{
    s16 col, row;

    if (x < BRICK_X0 * 8 || y < BRICK_Y0 * 8) return FALSE;

    col = (x - BRICK_X0 * 8) / (BRICK_W_TILES * 8);
    row = (y - BRICK_Y0 * 8) / 8;

    if (col >= BRICK_COLS || row >= BRICK_ROWS) return FALSE;
    if (!bricks[row][col]) return FALSE;

    bricks[row][col] = 0;
    VDP_fillTileMapRect(BG_A, 0,
                        BRICK_X0 + col * BRICK_W_TILES, BRICK_Y0 + row,
                        BRICK_W_TILES, 1);

    score += (BRICK_ROWS - row) * 10;
    bricksLeft--;
    hudDirty = TRUE;
    return TRUE;
}

/* Test the 4 corners of the ball box placed at (x, y). */
static bool hitBricks(s16 x, s16 y)
{
    bool r = FALSE;
    r |= hitBrickPoint(x, y);
    r |= hitBrickPoint(x + BALL_SIZE - 1, y);
    r |= hitBrickPoint(x, y + BALL_SIZE - 1);
    r |= hitBrickPoint(x + BALL_SIZE - 1, y + BALL_SIZE - 1);
    return r;
}

static void newGame(void)
{
    u16 r, c;

    score = 0;
    lives = START_LIVES;
    bricksLeft = BRICK_ROWS * BRICK_COLS;
    padX = (320 - PADDLE_W) / 2;

    VDP_clearPlane(BG_A, TRUE);

    /* top wall under the HUD row */
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_WALL),
                        0, 1, 40, 1);

    /* bricks */
    for (r = 0; r < BRICK_ROWS; r++)
    {
        for (c = 0; c < BRICK_COLS; c++)
        {
            bricks[r][c] = 1;
        }
        VDP_fillTileMapRect(BG_A,
                            TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_BRICK + r),
                            BRICK_X0, BRICK_Y0 + r,
                            BRICK_COLS * BRICK_W_TILES, 1);
    }

    placeBallOnPaddle();
    drawHUD();
}

static void loseLife(void)
{
    lives--;
    hudDirty = TRUE;
    padX = (320 - PADDLE_W) / 2;
    placeBallOnPaddle();

    if (lives == 0)
    {
        state = ST_OVER;
        showMessage("GAME OVER", MSG_ROW_TITLE);
        showMessage("PRESS START", MSG_ROW_INFO);
    }
    else
    {
        state = ST_SERVE;
        showMessage("A: LAUNCH BALL", MSG_ROW_INFO);
    }
}

static void launchBall(u16 pad)
{
    s32 spd = currentSpeed();

    if (pad & BUTTON_LEFT)       ballVX = -200;
    else if (pad & BUTTON_RIGHT) ballVX = 200;
    else                         ballVX = 160;
    ballVY = -spd;
}

/* ------------------------------------------------------------------------- */
/* Ball physics                                                              */
/* ------------------------------------------------------------------------- */

static void updateBall(void)
{
    s32 nx, ny;
    s16 bx, by;

    /* ---- X axis ---- */
    nx = ballX + ballVX;
    if (nx < 0)
    {
        nx = 0;
        ballVX = -ballVX;
    }
    else if (nx > ((s32)BALL_MAX_X << 8))
    {
        nx = (s32)BALL_MAX_X << 8;
        ballVX = -ballVX;
    }

    if (hitBricks(nx >> 8, ballY >> 8)) ballVX = -ballVX;
    else ballX = nx;

    /* ---- Y axis ---- */
    ny = ballY + ballVY;
    if (ny < ((s32)FIELD_TOP << 8))
    {
        ny = (s32)FIELD_TOP << 8;
        ballVY = -ballVY;
    }

    if (hitBricks(ballX >> 8, ny >> 8)) ballVY = -ballVY;
    else ballY = ny;

    /* ---- Paddle ---- */
    bx = ballX >> 8;
    by = ballY >> 8;

    if (ballVY > 0 &&
        by + BALL_SIZE >= PADDLE_Y && by < PADDLE_Y + 4 &&
        bx + BALL_SIZE > padX && bx < padX + PADDLE_W)
    {
        s16 offset = (bx + BALL_SIZE / 2) - (padX + PADDLE_W / 2); /* about -19..19 */
        ballY = (s32)(PADDLE_Y - BALL_SIZE) << 8;
        ballVX = (s32)offset * 28;
        ballVY = -currentSpeed();
    }

    /* ---- Fell below the screen ---- */
    if (by > 224)
    {
        loseLife();
        return;
    }

    /* ---- Cleared the board ---- */
    if (bricksLeft == 0)
    {
        state = ST_WIN;
        showMessage("YOU WIN!", MSG_ROW_TITLE);
        showMessage("PRESS START", MSG_ROW_INFO);
    }
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(bool hardReset)
{
    u16 pad, pressed;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palettes: PAL0 = text, PAL1 = game graphics */
    PAL_setColor(0, 0x0000);
    PAL_setColor(15, 0x0EEE);
    PAL_setPalette(PAL1, gamePalette, CPU);
    VDP_setTextPalette(PAL0);

    /* Tiles */
    VDP_loadTileData(brickTiles,  TILE_BRICK,  6, CPU);
    VDP_loadTileData(ballTile,    TILE_BALL,   1, CPU);
    VDP_loadTileData(paddleTiles, TILE_PADDLE, 4, CPU);
    VDP_loadTileData(wallTile,    TILE_WALL,   1, CPU);

    /* Hardware sprites: 0 = ball, 1 = paddle */
    VDP_resetSprites();
    VDP_setSpriteFull(0, 0, 0, SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_BALL), 1);
    VDP_setSpriteFull(1, 0, 0, SPRITE_SIZE(4, 1),
                      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_PADDLE), 0);

    JOY_init();
    prevPad = 0;

    newGame();
    state = ST_TITLE;
    showMessage("BREAKOUT", MSG_ROW_TITLE);
    showMessage("PRESS START", MSG_ROW_INFO);

    while (TRUE)
    {
        pad = JOY_readJoypad(JOY_1);
        pressed = pad & ~prevPad;
        prevPad = pad;

        /* Paddle movement is always available */
        if (pad & BUTTON_LEFT)  padX -= PADDLE_SPEED;
        if (pad & BUTTON_RIGHT) padX += PADDLE_SPEED;
        if (padX < 0) padX = 0;
        if (padX > 320 - PADDLE_W) padX = 320 - PADDLE_W;

        switch (state)
        {
            case ST_TITLE:
                placeBallOnPaddle();
                if (pressed & BUTTON_START)
                {
                    clearMessages();
                    showMessage("A: LAUNCH BALL", MSG_ROW_INFO);
                    state = ST_SERVE;
                }
                break;

            case ST_SERVE:
                placeBallOnPaddle();
                if (pressed & (BUTTON_A | BUTTON_B | BUTTON_C | BUTTON_START))
                {
                    clearMessages();
                    launchBall(pad);
                    state = ST_PLAY;
                }
                break;

            case ST_PLAY:
                updateBall();
                break;

            case ST_OVER:
            case ST_WIN:
                if (pressed & BUTTON_START)
                {
                    newGame();
                    showMessage("A: LAUNCH BALL", MSG_ROW_INFO);
                    state = ST_SERVE;
                }
                break;
        }

        if (hudDirty) drawHUD();

        /* Sprites */
        VDP_setSpritePosition(0, ballX >> 8, ballY >> 8);
        VDP_setSpritePosition(1, padX, PADDLE_Y);
        VDP_updateSprites(2, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
```