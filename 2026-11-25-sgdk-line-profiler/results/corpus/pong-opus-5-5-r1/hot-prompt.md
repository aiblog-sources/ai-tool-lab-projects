Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 6,210 cycles per frame on average (4.9%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,621 /frame  58.3% of busy  src/main.c:343  SYS_doVBlankProcess();
                     3,585 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,585 cycles each
    1,428 /frame  23.0% of busy  src/main.c:299  VDP_updateSprites(3, DMA_QUEUE);
                     1,294 in DMA_transfer.isra.0: 1.00 calls/frame, 1,294 cycles each
      318 /frame   5.1% of busy  [no game line]:0  
                       124 in no_bmp_task
                       123 in no_user_task
                        31 in no_xgm_task
      118 /frame   1.9% of busy  src/main.c:296  VDP_setSpriteFull(2, FROM_FP(ballX), FROM_FP(ballY), SPRITE_
       81 /frame   1.3% of busy  src/main.c:292  VDP_setSpriteFull(0, LEFT_PAD_X, leftY, SPRITE_SIZE(1, 4),
       69 /frame   1.1% of busy  src/main.c:294  VDP_setSpriteFull(1, RIGHT_PAD_X, rightY, SPRITE_SIZE(1, 4),
       62 /frame   1.0% of busy  src/main.c:205  if (ballVX > 0)
       39 /frame   0.6% of busy  src/main.c:193  if (pad & BUTTON_UP)   leftY -= PLAYER_SPEED;
       35 /frame   0.6% of busy  src/main.c:231  ballY += ballVY;
       34 /frame   0.5% of busy  src/main.c:203  s16 center = rightY + PADDLE_H / 2;
       33 /frame   0.5% of busy  src/main.c:222  if (serveTimer)
       24 /frame   0.4% of busy  src/main.c:211  else if (center > target + AI_DEADZONE) rightY -= AI_SPEED;
       23 /frame   0.4% of busy  src/main.c:128  sprintf(buf, "%-2u", scoreRight);
                        23 in sprintf: 0.01 calls/frame, 3,026 cycles each
                 lint [text] guarded: sprintf on the frame path: 12,582 cycles (10% of a frame) for "SCORE %ld"; redraw only when the value changes
       23 /frame   0.4% of busy  src/main.c:251  if (prevX >= face && ballX < face &&
       22 /frame   0.3% of busy  src/main.c:194  if (pad & BUTTON_DOWN) leftY += PLAYER_SPEED;

== Lint findings, by measured cost ==
negligible        23 /frame (0.02% of a frame)  src/main.c:128 [text] guarded
negligible        14 /frame (0.01% of a frame)  src/main.c:126 [text] guarded
negligible        14 /frame (0.01% of a frame)  src/main.c:129 [text] guarded
negligible        13 /frame (0.01% of a frame)  src/main.c:125 [text] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:179 [div] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:183 [div] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:183 [mul32] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,621 /frame  src/main.c:343 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
// main.c - Pong for Sega Mega Drive / Genesis (SGDK 2.09)
// Left paddle: pad 1 D-pad (up/down). Right paddle: simple AI.
// The ball speeds up 5% after each paddle hit. Scores are drawn at the top.
// No resource files: tiles are const u32 arrays loaded with VDP_loadTileData.

#include <genesis.h>

// ---------------------------------------------------------------------------
// Screen / playfield constants
// ---------------------------------------------------------------------------
#define SCREEN_W        320
#define SCREEN_H        224

#define FIELD_TOP       16          // first pixel row below the top wall
#define FIELD_BOTTOM    216         // first pixel row of the bottom wall

#define PADDLE_W        8
#define PADDLE_H        32
#define BALL_SIZE       8

#define LEFT_PAD_X      16
#define RIGHT_PAD_X     (SCREEN_W - 16 - PADDLE_W)   // 296

#define PLAYER_SPEED    4
#define AI_SPEED        3
#define AI_DEADZONE     6

// Fixed point: 8 fractional bits
#define FP_SHIFT        8
#define TO_FP(x)        ((s32)(x) << FP_SHIFT)
#define FROM_FP(x)      ((s16)((x) >> FP_SHIFT))

#define BALL_START_SPEED   (TO_FP(2) + 128)   // 2.5 px/frame
#define BALL_MAX_SPEED     TO_FP(7)

#define SERVE_DELAY     60

// ---------------------------------------------------------------------------
// Tiles (4bpp, one u32 per 8-pixel row, color index 15 = white)
// ---------------------------------------------------------------------------
// Paddle: 4 solid tiles (1x4 sprite, 8x32)
static const u32 tile_paddle[8 * 4] =
{
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,

    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

// Ball: rounded 8x8
static const u32 tile_ball[8] =
{
    0x00FFFF00,
    0x0FFFFFF0,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x0FFFFFF0,
    0x00FFFF00
};

// Wall: solid band in the middle of the tile
static const u32 tile_wall[8] =
{
    0x00000000,
    0x00000000,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0xFFFFFFFF,
    0x00000000,
    0x00000000
};

// Center net: 2-pixel dashed vertical line at the left edge of the tile
static const u32 tile_net[8] =
{
    0xFF000000,
    0xFF000000,
    0xFF000000,
    0xFF000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

// Tile indexes in VRAM
#define TILE_PADDLE     (TILE_USER_INDEX)
#define TILE_BALL       (TILE_PADDLE + 4)
#define TILE_WALL       (TILE_BALL + 1)
#define TILE_NET        (TILE_WALL + 1)

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
static s16 leftY;
static s16 rightY;

static s32 ballX, ballY;      // fixed point
static s32 ballVX, ballVY;    // fixed point
static s32 ballSpeed;         // fixed point, horizontal speed magnitude

static u16 scoreLeft;
static u16 scoreRight;

static u16 serveTimer;
static s16 serveDir;          // -1 = toward left, +1 = toward right

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void drawScores(void)
{
    char buf[8];

    sprintf(buf, "%2u", scoreLeft);
    VDP_drawText(buf, 15, 0);

    sprintf(buf, "%-2u", scoreRight);
    VDP_drawText(buf, 23, 0);
}

static void drawField(void)
{
    u16 x, y;

    // Top and bottom walls (row 1 and row 27)
    for (x = 0; x < 40; x++)
    {
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_WALL), x, 1);
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_WALL), x, 27);
    }

    // Center net (column 20 => pixel 160)
    for (y = 2; y < 27; y++)
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_NET), 20, y);
}

static void resetBall(s16 dir)
{
    ballX = TO_FP((SCREEN_W - BALL_SIZE) / 2);
    ballY = TO_FP((FIELD_TOP + FIELD_BOTTOM - BALL_SIZE) / 2);
    ballVX = 0;
    ballVY = 0;
    ballSpeed = BALL_START_SPEED;
    serveDir = dir;
    serveTimer = SERVE_DELAY;
}

static void serveBall(void)
{
    s32 vy;

    ballVX = (serveDir < 0) ? -ballSpeed : ballSpeed;

    // random vertical component between 0.5 and ~1.5 px/frame, random sign
    vy = TO_FP(1) / 2 + (s32)(random() & 0xFF);
    if (random() & 1) vy = -vy;
    ballVY = vy;
}

// Bounce off a paddle: speed up 5%, angle depends on where the ball hit.
static void bounceOffPaddle(s16 paddleY, s16 newDir)
{
    s16 ballCenter = FROM_FP(ballY) + BALL_SIZE / 2;
    s16 padCenter = paddleY + PADDLE_H / 2;
    s16 offset = ballCenter - padCenter;          // about -20..+20

    // +5% speed
    ballSpeed = (ballSpeed * 21) / 20;
    if (ballSpeed > BALL_MAX_SPEED) ballSpeed = BALL_MAX_SPEED;

    ballVX = (newDir < 0) ? -ballSpeed : ballSpeed;
    ballVY = (ballSpeed * offset) / 20;

    // avoid perfectly flat shots
    if (ballVY == 0) ballVY = (random() & 1) ? 64 : -64;
}

static void updatePlayer(void)
{
    u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_UP)   leftY -= PLAYER_SPEED;
    if (pad & BUTTON_DOWN) leftY += PLAYER_SPEED;

    if (leftY < FIELD_TOP) leftY = FIELD_TOP;
    if (leftY > FIELD_BOTTOM - PADDLE_H) leftY = FIELD_BOTTOM - PADDLE_H;
}

static void updateAI(void)
{
    s16 target;
    s16 center = rightY + PADDLE_H / 2;

    if (ballVX > 0)
        target = FROM_FP(ballY) + BALL_SIZE / 2;            // track the ball
    else
        target = (FIELD_TOP + FIELD_BOTTOM) / 2;            // drift to center

    if (center < target - AI_DEADZONE)      rightY += AI_SPEED;
    else if (center > target + AI_DEADZONE) rightY -= AI_SPEED;

    if (rightY < FIELD_TOP) rightY = FIELD_TOP;
    if (rightY > FIELD_BOTTOM - PADDLE_H) rightY = FIELD_BOTTOM - PADDLE_H;
}

static void updateBall(void)
{
    s32 prevX;
    s16 bx, by;

    if (serveTimer)
    {
        serveTimer--;
        if (serveTimer == 0) serveBall();
        return;
    }

    prevX = ballX;
    ballX += ballVX;
    ballY += ballVY;

    // Top / bottom walls
    if (ballY < TO_FP(FIELD_TOP))
    {
        ballY = TO_FP(FIELD_TOP) + (TO_FP(FIELD_TOP) - ballY);
        ballVY = -ballVY;
    }
    else if (ballY > TO_FP(FIELD_BOTTOM - BALL_SIZE))
    {
        ballY = TO_FP(FIELD_BOTTOM - BALL_SIZE) - (ballY - TO_FP(FIELD_BOTTOM - BALL_SIZE));
        ballVY = -ballVY;
    }

    by = FROM_FP(ballY);

    // Left paddle (swept test on its right face)
    if (ballVX < 0)
    {
        s32 face = TO_FP(LEFT_PAD_X + PADDLE_W);
        if (prevX >= face && ballX < face &&
            by + BALL_SIZE > leftY && by < leftY + PADDLE_H)
        {
            ballX = face;
            bounceOffPaddle(leftY, 1);
        }
    }
    // Right paddle (swept test on its left face)
    else if (ballVX > 0)
    {
        s32 face = TO_FP(RIGHT_PAD_X - BALL_SIZE);
        if (prevX <= face && ballX > face &&
            by + BALL_SIZE > rightY && by < rightY + PADDLE_H)
        {
            ballX = face;
            bounceOffPaddle(rightY, -1);
        }
    }

    bx = FROM_FP(ballX);

    // Scoring
    if (bx < -BALL_SIZE)
    {
        scoreRight++;
        if (scoreRight > 99) scoreRight = 0;
        drawScores();
        resetBall(-1);      // serve toward the player who lost the point
    }
    else if (bx > SCREEN_W)
    {
        scoreLeft++;
        if (scoreLeft > 99) scoreLeft = 0;
        drawScores();
        resetBall(1);
    }
}

static void updateSprites(void)
{
    // index, x, y, size, attributes, link
    VDP_setSpriteFull(0, LEFT_PAD_X, leftY, SPRITE_SIZE(1, 4),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_PADDLE), 1);
    VDP_setSpriteFull(1, RIGHT_PAD_X, rightY, SPRITE_SIZE(1, 4),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_PADDLE), 2);
    VDP_setSpriteFull(2, FROM_FP(ballX), FROM_FP(ballY), SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_BALL), 0);

    VDP_updateSprites(3, DMA_QUEUE);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setPlaneSize(64, 32, TRUE);

    // Palette: black background, white for color 15 (also used by the font)
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));
    VDP_setBackgroundColor(0);

    // Load tiles
    VDP_loadTileData(tile_paddle, TILE_PADDLE, 4, CPU);
    VDP_loadTileData(tile_ball,   TILE_BALL,   1, CPU);
    VDP_loadTileData(tile_wall,   TILE_WALL,   1, CPU);
    VDP_loadTileData(tile_net,    TILE_NET,    1, CPU);

    VDP_setTextPlane(BG_A);
    VDP_setTextPalette(PAL0);

    drawField();

    leftY  = (FIELD_TOP + FIELD_BOTTOM - PADDLE_H) / 2;
    rightY = leftY;
    scoreLeft = 0;
    scoreRight = 0;
    drawScores();

    resetBall((random() & 1) ? 1 : -1);

    while (TRUE)
    {
        updatePlayer();
        updateAI();
        updateBall();
        updateSprites();

        SYS_doVBlankProcess();
    }

    return 0;
}
