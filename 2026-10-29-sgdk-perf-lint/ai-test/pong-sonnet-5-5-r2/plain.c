/*
 * main.c - Pong for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * - Player (left paddle): D-pad UP / DOWN on pad 1
 * - CPU (right paddle): simple tracking AI with a speed limit and aim error
 * - Ball speeds up 5% after every paddle hit
 * - Scores at the top; first to 9 wins; START begins / restarts
 * - No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData
 *
 * Performance notes:
 * - Sprite attributes (size/link/tile/fixed X) are written once; per frame only
 *   the changing coordinates are stored directly in the sprite cache.
 * - The sprite table DMA is queued only when something actually moved.
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

/* Last values pushed to the sprite cache (to detect changes) */
static s16 shownLeftY, shownRightY, shownBallX, shownBallY;

/* ---------- Helpers ---------- */
static inline s16 clampS16(s16 v, s16 lo, s16 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline s32 clampS32(s32 v, s32 lo, s32 hi)
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

/* Reflect the ball off a paddle: +5% speed and a little spin.
 * Runs only on a hit, so the (slow) 32-bit divide is not a per-frame cost. */
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

/* One-time sprite setup: size, link, attributes and fixed X coordinates. */
static void initSprites(void)
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

    /* force first upload */
    shownLeftY  = leftY;
    shownRightY = rightY;
    shownBallX  = (s16)(bx >> 16);
    shownBallY  = (s16)(by >> 16);
    VDP_updateSprites(3, DMA_QUEUE);
}

/* Per-frame: touch only what changed, and DMA only if something did. */
static inline void drawSprites(void)
{
    const s16 ballX = (s16)(bx >> 16);
    const s16 ballY = (s16)(by >> 16);

    if (leftY == shownLeftY && rightY == shownRightY &&
        ballX == shownBallX && ballY == shownBallY)
        return;

    shownLeftY  = leftY;
    shownRightY = rightY;
    shownBallX  = ballX;
    shownBallY  = ballY;

    vdpSpriteCache[0].y = leftY + 128;
    vdpSpriteCache[1].y = rightY + 128;
    vdpSpriteCache[2].x = ballX + 128;
    vdpSpriteCache[2].y = ballY + 128;

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
    initSprites();

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
