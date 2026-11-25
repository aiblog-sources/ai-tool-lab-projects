Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 6,214 cycles per frame on average (4.9%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    3,611 /frame  58.1% of busy  src/main.c:362  SYS_doVBlankProcess();
                     3,584 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,584 cycles each
    1,429 /frame  23.0% of busy  src/main.c:282  VDP_updateSprites(3, DMA_QUEUE);
                     1,295 in DMA_transfer.isra.0: 1.00 calls/frame, 1,295 cycles each
      318 /frame   5.1% of busy  [no game line]:0  
                       124 in no_bmp_task
                       124 in no_user_task
                        30 in no_xgm_task
      104 /frame   1.7% of busy  src/main.c:279  VDP_setSpriteFull(2, (bx >> 16) + 128, (by >> 16) + 128,
       95 /frame   1.5% of busy  src/main.c:275  VDP_setSpriteFull(0, LP_X + 128, leftY + 128,
       89 /frame   1.4% of busy  src/main.c:277  VDP_setSpriteFull(1, RP_X + 128, rightY + 128,
       68 /frame   1.1% of busy  src/main.c:91  if (v > hi) return hi;
       55 /frame   0.9% of busy  src/main.c:166  if (vx > 0 && serveTimer == 0)
       45 /frame   0.7% of busy  src/main.c:340  switch (state)
       34 /frame   0.6% of busy  src/main.c:156  if (joy & BUTTON_UP)   leftY -= PLAYER_SPEED;
       33 /frame   0.5% of busy  src/main.c:207  bx += vx;
       24 /frame   0.4% of busy  src/main.c:208  by += vy;
       21 /frame   0.3% of busy  src/main.c:226  if (vx < 0 &&
       19 /frame   0.3% of busy  src/main.c:157  if (joy & BUTTON_DOWN) leftY += PLAYER_SPEED;
       19 /frame   0.3% of busy  src/main.c:177  diff = target - rightY;

== Lint findings, by measured cost ==
negligible        14 /frame (0.01% of a frame)  src/main.c:106 [text] guarded
negligible        14 /frame (0.01% of a frame)  src/main.c:108 [text] guarded
negligible         3 /frame (0.00% of a frame)  src/main.c:105 [text] guarded
negligible         3 /frame (0.00% of a frame)  src/main.c:107 [text] guarded
negligible         2 /frame (0.00% of a frame)  src/main.c:189 [text] guarded
negligible         2 /frame (0.00% of a frame)  src/main.c:188 [text] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:144 [div] guarded
   not run         0 /frame (0.00% of a frame)  src/main.c:145 [div] guarded
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,611 /frame  src/main.c:362 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
/*
 * main.c - Pong for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - Player (left paddle): D-pad UP / DOWN on pad 1
 * - CPU (right paddle): simple tracking AI with a speed limit and aim error
 * - Ball speeds up 5% after every paddle hit
 * - Scores at the top; first to 9 wins; START begins / restarts
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 */

#include <genesis.h>

/* ---------- Layout (pixels) ---------- */
#define SCREEN_W        320
#define PF_TOP          32          /* play field top (below score area / wall) */
#define PF_BOT          216         /* play field bottom (above bottom wall)    */
#define PADDLE_W        8
#define PADDLE_H        32
#define BALL_SIZE       8
#define LP_X            16
#define RP_X            (SCREEN_W - 16 - PADDLE_W)

/* ---------- Gameplay tuning ---------- */
#define PLAYER_SPEED    4                   /* px / frame             */
#define AI_SPEED        3                   /* px / frame             */
#define AI_DEADZONE     2
#define BALL_START_VX   (5 << 15)           /* 2.5 px/frame in 16.16  */
#define BALL_MAX_VX     (7 << 16)           /* < 8 so no tunnelling   */
#define BALL_MAX_VY     (6 << 16)
#define SERVE_DELAY     60                  /* frames                 */
#define WIN_SCORE       9

/* ---------- Tile layout in VRAM ---------- */
#define T_PADDLE        (TILE_USER_INDEX + 0)   /* 4 tiles (8x32 sprite) */
#define T_BALL          (TILE_USER_INDEX + 4)
#define T_DASH          (TILE_USER_INDEX + 5)
#define NUM_TILES       6

/* Each tile: 8 rows of 8 pixels, one nibble per pixel (palette index). */
static const u32 gameTiles[NUM_TILES * 8] =
{
    /* 0..3: solid paddle segments (color 1) */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,

    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,

    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,

    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111,

    /* 4: round ball (color 1) */
    0x00111100,
    0x01111110,
    0x11111111,
    0x11111111,
    0x11111111,
    0x11111111,
    0x01111110,
    0x00111100,

    /* 5: center line dash (color 2, grey) */
    0x00022000,
    0x00022000,
    0x00022000,
    0x00022000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
};

/* ---------- Game state ---------- */
typedef enum { STATE_TITLE, STATE_PLAY, STATE_OVER } GameState;

static GameState state;
static s32 bx, by, vx, vy;          /* ball position / velocity, 16.16 fixed */
static s16 leftY, rightY;           /* paddle top Y in pixels                */
static u8  scoreL, scoreR;
static s16 aiError;                 /* AI aim error in pixels                */
static u16 serveTimer;
static s16 serveDir;                /* +1 = ball goes right, -1 = left       */

/* ---------- Helpers ---------- */
static s16 clampS16(s16 v, s16 lo, s16 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static s32 clampS32(s32 v, s32 lo, s32 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void drawScores(void)
{
    char buf[4];
    intToStr(scoreL, buf, 1);
    VDP_drawText(buf, 11, 1);
    intToStr(scoreR, buf, 1);
    VDP_drawText(buf, 32, 1);
}

static void resetBall(s16 dir)
{
    bx = ((s32)(SCREEN_W / 2 - BALL_SIZE / 2)) << 16;
    by = ((s32)((PF_TOP + PF_BOT) / 2 - BALL_SIZE / 2)) << 16;

    vx = (dir > 0) ? BALL_START_VX : -BALL_START_VX;
    vy = (s32)(0x8000 + (random() & 0x7FFF));       /* 0.5 .. 1.0 px/frame */
    if (random() & 1) vy = -vy;

    serveDir = dir;
    serveTimer = SERVE_DELAY;
    aiError = 0;
}

static void resetGame(void)
{
    scoreL = 0;
    scoreR = 0;
    leftY  = (PF_TOP + PF_BOT) / 2 - PADDLE_H / 2;
    rightY = leftY;
    resetBall((random() & 1) ? 1 : -1);
    drawScores();
}

/* Reflect the ball off a paddle: +5% speed and a little spin. */
static void paddleHit(s16 paddleY)
{
    s16 ballCenter = (s16)(by >> 16) + BALL_SIZE / 2;
    s16 offset = ballCenter - (paddleY + PADDLE_H / 2);     /* about -20..+20 */

    vx = -vx;

    /* speed up 5% */
    vx = (vx * 105) / 100;
    vy = (vy * 105) / 100;

    /* spin from where the ball struck the paddle */
    vy += (s32)offset * 4096;

    vx = clampS32(vx, -BALL_MAX_VX, BALL_MAX_VX);
    vy = clampS32(vy, -BALL_MAX_VY, BALL_MAX_VY);
}

static void updatePlayer(u16 joy)
{
    if (joy & BUTTON_UP)   leftY -= PLAYER_SPEED;
    if (joy & BUTTON_DOWN) leftY += PLAYER_SPEED;
    leftY = clampS16(leftY, PF_TOP, PF_BOT - PADDLE_H);
}

static void updateAI(void)
{
    s16 target;
    s16 diff;

    if (vx > 0 && serveTimer == 0)
    {
        s16 ballCenter = (s16)(by >> 16) + BALL_SIZE / 2;
        target = ballCenter - PADDLE_H / 2 + aiError;
    }
    else
    {
        /* ball moving away: drift back to the middle */
        target = (PF_TOP + PF_BOT) / 2 - PADDLE_H / 2;
    }

    diff = target - rightY;
    if (diff > AI_DEADZONE)
        rightY += (diff > AI_SPEED) ? AI_SPEED : diff;
    else if (diff < -AI_DEADZONE)
        rightY -= (-diff > AI_SPEED) ? AI_SPEED : -diff;

    rightY = clampS16(rightY, PF_TOP, PF_BOT - PADDLE_H);
}

static void showMessage(const char *line1, const char *line2)
{
    VDP_drawText(line1, 14, 11);
    VDP_drawText(line2, 14, 14);
}

static void clearMessage(void)
{
    VDP_clearTextArea(10, 10, 22, 6);
}

static void updateBall(void)
{
    s16 px, py;

    if (serveTimer > 0)
    {
        serveTimer--;
        return;
    }

    bx += vx;
    by += vy;

    /* top / bottom walls */
    if (by < ((s32)PF_TOP << 16))
    {
        by = (s32)PF_TOP << 16;
        vy = -vy;
    }
    else if (by > ((s32)(PF_BOT - BALL_SIZE) << 16))
    {
        by = (s32)(PF_BOT - BALL_SIZE) << 16;
        vy = -vy;
    }

    px = (s16)(bx >> 16);
    py = (s16)(by >> 16);

    /* left paddle (player) */
    if (vx < 0 &&
        px <= LP_X + PADDLE_W && px + BALL_SIZE > LP_X &&
        py + BALL_SIZE > leftY && py < leftY + PADDLE_H)
    {
        bx = (s32)(LP_X + PADDLE_W) << 16;
        paddleHit(leftY);
        aiError = (s16)(random() % 25) - 12;     /* new aim error for the AI */
    }
    /* right paddle (AI) */
    else if (vx > 0 &&
             px + BALL_SIZE >= RP_X && px < RP_X + PADDLE_W &&
             py + BALL_SIZE > rightY && py < rightY + PADDLE_H)
    {
        bx = (s32)(RP_X - BALL_SIZE) << 16;
        paddleHit(rightY);
    }

    /* scoring */
    px = (s16)(bx >> 16);
    if (px < -BALL_SIZE)
    {
        scoreR++;
        drawScores();
        if (scoreR >= WIN_SCORE)
        {
            state = STATE_OVER;
            showMessage("CPU WINS!", "PRESS START");
        }
        else resetBall(-1);     /* serve toward the player who lost the point */
    }
    else if (px > SCREEN_W)
    {
        scoreL++;
        drawScores();
        if (scoreL >= WIN_SCORE)
        {
            state = STATE_OVER;
            showMessage("PLAYER WINS!", "PRESS START");
        }
        else resetBall(1);
    }
}

static void drawSprites(void)
{
    const u16 paddleAttr = TILE_ATTR_FULL(PAL1, 0, 0, 0, T_PADDLE);
    const u16 ballAttr   = TILE_ATTR_FULL(PAL1, 0, 0, 0, T_BALL);

    /* hardware sprite coordinates have a (128,128) origin offset */
    VDP_setSpriteFull(0, LP_X + 128, leftY + 128,
                      SPRITE_SIZE(1, 4), paddleAttr, 1);
    VDP_setSpriteFull(1, RP_X + 128, rightY + 128,
                      SPRITE_SIZE(1, 4), paddleAttr, 2);
    VDP_setSpriteFull(2, (bx >> 16) + 128, (by >> 16) + 128,
                      SPRITE_SIZE(1, 1), ballAttr, 0);

    VDP_updateSprites(3, DMA_QUEUE);
}

static void drawField(void)
{
    u16 x, y;
    const u16 wallAttr = TILE_ATTR_FULL(PAL1, 0, 0, 0, T_PADDLE);
    const u16 dashAttr = TILE_ATTR_FULL(PAL1, 0, 0, 0, T_DASH);

    /* top wall (row 3 = y 24..31) and bottom wall (row 27 = y 216..223) */
    for (x = 0; x < 40; x++)
    {
        VDP_setTileMapXY(BG_B, wallAttr, x, PF_TOP / 8 - 1);
        VDP_setTileMapXY(BG_B, wallAttr, x, PF_BOT / 8);
    }

    /* dashed center line */
    for (y = PF_TOP / 8; y < PF_BOT / 8; y++)
        VDP_setTileMapXY(BG_B, dashAttr, 19, y);
}

int main(bool hardReset)
{
    u16 joy, prevJoy = 0;

    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* palettes: PAL0 for text, PAL1 for game graphics */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x666666));

    VDP_loadTileData(gameTiles, T_PADDLE, NUM_TILES, CPU);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_resetSprites();

    drawField();

    VDP_drawText("PLAYER", 4, 1);
    VDP_drawText("CPU", 28, 1);

    setRandomSeed(0x1234);
    resetGame();

    state = STATE_TITLE;
    showMessage("MEGA PONG", "PRESS START");

    while (TRUE)
    {
        joy = JOY_readJoypad(JOY_1);

        switch (state)
        {
        case STATE_TITLE:
        case STATE_OVER:
            if ((joy & BUTTON_START) && !(prevJoy & BUTTON_START))
            {
                clearMessage();
                resetGame();
                state = STATE_PLAY;
            }
            break;

        case STATE_PLAY:
            updatePlayer(joy);
            updateAI();
            updateBall();
            break;
        }

        prevJoy = joy;

        drawSprites();
        SYS_doVBlankProcess();
    }

    return 0;
}
