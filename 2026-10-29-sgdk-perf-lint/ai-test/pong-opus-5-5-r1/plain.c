// main.c - Pong for Sega Mega Drive / Genesis (SGDK 2.09)
// Left paddle: pad 1 D-pad (up/down). Right paddle: simple AI.
// The ball speeds up 5% after each paddle hit. Scores are drawn at the top.
// No resource files: tiles are const u32 arrays loaded with VDP_loadTileData.
//
// Performance notes (68000 @ 7.67 MHz):
//  - 32-bit mul/div (libgcc __mulsi3/__divsi3) replaced by native
//    mulu.w/muls.w/divu.w/divs.w; value ranges guarantee identical results.
//  - Static sprite attributes are written once; per frame only positions.
//  - sprintf removed from score drawing.

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
// Native 68000 16-bit multiply / divide helpers
// ---------------------------------------------------------------------------
// u16 * u16 -> u32 (mulu.w)
static inline u32 mul_u16(u16 a, u16 b)
{
    u32 r = a;
    __asm__ ("mulu.w %1,%0" : "+d" (r) : "dmi" (b) : "cc");
    return r;
}

// s16 * s16 -> s32 (muls.w)
static inline s32 mul_s16(s16 a, s16 b)
{
    s32 r = a;
    __asm__ ("muls.w %1,%0" : "+d" (r) : "dmi" (b) : "cc");
    return r;
}

// u32 / u16 -> u16 quotient (divu.w), caller guarantees no overflow
static inline u16 div_u32_u16(u32 a, u16 b)
{
    __asm__ ("divu.w %1,%0" : "+d" (a) : "dmi" (b) : "cc");
    return (u16) a;
}

// s32 / s16 -> s16 quotient (divs.w, truncates toward zero like C)
static inline s16 div_s32_s16(s32 a, s16 b)
{
    __asm__ ("divs.w %1,%0" : "+d" (a) : "dmi" (b) : "cc");
    return (s16) a;
}

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
static s32 ballSpeed;         // fixed point, horizontal speed magnitude (640..1792)

static u16 scoreLeft;
static u16 scoreRight;

static u16 serveTimer;
static s16 serveDir;          // -1 = toward left, +1 = toward right

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void drawScores(void)
{
    char buf[3];
    u16 v, tens;

    // Left score: "%2u" (right-justified, width 2)
    v = scoreLeft;
    tens = 0;
    while (v >= 10) { v -= 10; tens++; }
    buf[0] = tens ? ('0' + tens) : ' ';
    buf[1] = '0' + v;
    buf[2] = 0;
    VDP_drawText(buf, 15, 0);

    // Right score: "%-2u" (left-justified, width 2)
    v = scoreRight;
    tens = 0;
    while (v >= 10) { v -= 10; tens++; }
    if (tens)
    {
        buf[0] = '0' + tens;
        buf[1] = '0' + v;
    }
    else
    {
        buf[0] = '0' + v;
        buf[1] = ' ';
    }
    buf[2] = 0;
    VDP_drawText(buf, 23, 0);
}

static void drawField(void)
{
    // Top and bottom walls (row 1 and row 27)
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_WALL), 0, 1, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_WALL), 0, 27, 40, 1);

    // Center net (column 20 => pixel 160), rows 2..26
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_NET), 20, 2, 1, 25);
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
    s16 offset = ballCenter - padCenter;          // -19..+19
    s16 speed;

    // +5% speed: ballSpeed <= 1792, so 1792*21 = 37632 fits mulu.w result,
    // and the quotient (<= 1881) fits divu.w's 16-bit result.
    speed = (s16) div_u32_u16(mul_u16((u16) ballSpeed, 21), 20);
    if (speed > BALL_MAX_SPEED) speed = BALL_MAX_SPEED;
    ballSpeed = speed;

    ballVX = (newDir < 0) ? -(s32) speed : (s32) speed;
    // |speed * offset| <= 1792*19 = 34048, quotient fits in s16.
    ballVY = div_s32_s16(mul_s16(speed, offset), 20);

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
        const s32 face = TO_FP(LEFT_PAD_X + PADDLE_W);
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
        const s32 face = TO_FP(RIGHT_PAD_X - BALL_SIZE);
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

// Static sprite attributes (size, tile, link) are written once.
static void initSprites(void)
{
    // index, x, y, size, attributes, link
    VDP_setSpriteFull(0, LEFT_PAD_X, leftY, SPRITE_SIZE(1, 4),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_PADDLE), 1);
    VDP_setSpriteFull(1, RIGHT_PAD_X, rightY, SPRITE_SIZE(1, 4),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_PADDLE), 2);
    VDP_setSpriteFull(2, FROM_FP(ballX), FROM_FP(ballY), SPRITE_SIZE(1, 1),
                      TILE_ATTR_FULL(PAL0, 1, 0, 0, TILE_BALL), 0);
}

// Per frame: only positions change.
static void updateSprites(void)
{
    VDP_setSpritePosition(0, LEFT_PAD_X, leftY);
    VDP_setSpritePosition(1, RIGHT_PAD_X, rightY);
    VDP_setSpritePosition(2, FROM_FP(ballX), FROM_FP(ballY));

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

    initSprites();

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
