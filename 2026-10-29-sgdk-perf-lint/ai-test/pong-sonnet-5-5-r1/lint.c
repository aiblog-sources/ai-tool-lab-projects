/*
 * Pong for Sega Mega Drive / Genesis - SGDK 2.09
 * Single file, no resources: all tiles are const u32 arrays.
 *
 * Left paddle : Player 1 D-pad (UP / DOWN)
 * Right paddle: simple AI
 * Ball speeds up 5% after every paddle hit.
 * First to 9 wins; the scores then reset (START also resets).
 */

#include <genesis.h>

/* ---------- Screen / playfield ---------- */
#define SCREEN_W        320
#define SCREEN_H        224

#define PLAY_TOP        24      /* wall row 2 covers y=16..23 */
#define PLAY_BOT        216     /* wall row 27 covers y=216..223 */

#define PADDLE_W        8
#define PADDLE_H        32
#define BALL_SIZE       8

#define LEFT_X          16
#define RIGHT_X         (SCREEN_W - 16 - PADDLE_W)

#define PLAYER_SPEED    4
#define AI_SPEED        3
#define AI_DEADZONE     4

/* Ball velocity is 8.8 fixed point (px/frame * 256) */
#define BALL_START_VX   512     /* 2.0 px/frame */
#define BALL_MAX_VX     1792    /* 7.0 px/frame, keeps collisions reliable */
#define BALL_MAX_VY     1536    /* 6.0 px/frame */

#define WIN_SCORE       9
#define SERVE_DELAY     60

/* ---------- Tile layout in VRAM ---------- */
#define T_PADDLE        (TILE_USER_INDEX)        /* 4 consecutive tiles (8x32 sprite) */
#define T_BALL          (TILE_USER_INDEX + 4)
#define T_NET           (TILE_USER_INDEX + 5)

/* ---------- Tile data (8x8, 4bpp, one nibble per pixel) ---------- */
static const u32 tileSolid[8] =
{
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

static const u32 tileBall[8] =
{
    0x00222200,
    0x02222220,
    0x22222222,
    0x22222222,
    0x22222222,
    0x22222222,
    0x02222220,
    0x00222200
};

static const u32 tileNet[8] =
{
    0x00011000, 0x00011000, 0x00011000, 0x00011000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* ---------- Game state ---------- */
static s16 leftY, rightY;           /* paddle top, in pixels */
static s32 ballX, ballY;            /* 8.8 fixed point, top-left of ball */
static s32 ballVX, ballVY;          /* 8.8 fixed point */
static u8  scoreL, scoreR;
static u8  shownL = 0xFF, shownR = 0xFF;   /* last values drawn on screen */
static u16 serveTimer;

/* ---------- Helpers ---------- */
static s32 absS32(s32 v)
{
    return (v < 0) ? -v : v;
}

static void drawScores(void)
{
    char s[2];
    s[1] = 0;

    if (scoreL != shownL)
    {
        shownL = scoreL;
        s[0] = '0' + scoreL;
        VDP_drawText(s, 10, 0);
    }

    if (scoreR != shownR)
    {
        shownR = scoreR;
        s[0] = '0' + scoreR;
        VDP_drawText(s, 29, 0);
    }
}

static void serveBall(s8 dir)
{
    ballX = ((SCREEN_W - BALL_SIZE) / 2) << 8;
    ballY = ((PLAY_TOP + PLAY_BOT - BALL_SIZE) / 2) << 8;

    ballVX = (dir > 0) ? BALL_START_VX : -BALL_START_VX;
    ballVY = 128 + (s32)((u16)random() % (u16)200);
    if (random() & 1) ballVY = -ballVY;

    serveTimer = SERVE_DELAY;
}

static void resetGame(void)
{
    scoreL = 0;
    scoreR = 0;
    leftY  = (PLAY_TOP + PLAY_BOT - PADDLE_H) / 2;
    rightY = leftY;
    drawScores();
    serveBall((random() & 1) ? 1 : -1);
}

static void paddleHit(s16 paddleY, s16 ballPxY, s8 newDir)
{
    /* Offset of ball center from paddle center: about -20..+20 */
    s16 offset = (ballPxY + BALL_SIZE / 2) - (paddleY + PADDLE_H / 2);
    u16 speed;

    /* Reverse and speed up by 5% (speed is at most BALL_MAX_VX, fits u16) */
    speed = (u16)absS32(ballVX);
    speed += speed / (u16)20;
    if (speed > BALL_MAX_VX) speed = BALL_MAX_VX;

    ballVX = (newDir > 0) ? (s32)speed : -(s32)speed;

    /* Angle depends on where the ball hit; scales with speed */
    ballVY = muls(offset, (s16)speed) / 16;
    if (ballVY >  BALL_MAX_VY) ballVY =  BALL_MAX_VY;
    if (ballVY < -BALL_MAX_VY) ballVY = -BALL_MAX_VY;
}

static void updatePlayer(void)
{
    u16 pad = JOY_readJoypad(JOY_1);

    if (pad & BUTTON_UP)   leftY -= PLAYER_SPEED;
    if (pad & BUTTON_DOWN) leftY += PLAYER_SPEED;

    if (leftY < PLAY_TOP) leftY = PLAY_TOP;
    if (leftY > PLAY_BOT - PADDLE_H) leftY = PLAY_BOT - PADDLE_H;

    if (pad & BUTTON_START) resetGame();
}

static void updateAI(void)
{
    s16 target;
    s16 center = rightY + PADDLE_H / 2;
    s16 diff;

    if (ballVX > 0)
        target = (ballY >> 8) + BALL_SIZE / 2;          /* chase the ball */
    else
        target = (PLAY_TOP + PLAY_BOT) / 2;             /* drift to center */

    diff = target - center;

    if (diff > AI_DEADZONE)       rightY += AI_SPEED;
    else if (diff < -AI_DEADZONE) rightY -= AI_SPEED;

    if (rightY < PLAY_TOP) rightY = PLAY_TOP;
    if (rightY > PLAY_BOT - PADDLE_H) rightY = PLAY_BOT - PADDLE_H;
}

static void updateBall(void)
{
    s16 bx, by;

    if (serveTimer > 0)
    {
        serveTimer--;
        return;
    }

    ballX += ballVX;
    ballY += ballVY;

    bx = ballX >> 8;
    by = ballY >> 8;

    /* Top / bottom walls */
    if (by < PLAY_TOP)
    {
        ballY = PLAY_TOP << 8;
        ballVY = -ballVY;
        by = PLAY_TOP;
    }
    else if (by + BALL_SIZE > PLAY_BOT)
    {
        ballY = (PLAY_BOT - BALL_SIZE) << 8;
        ballVY = -ballVY;
        by = PLAY_BOT - BALL_SIZE;
    }

    /* Left paddle */
    if (ballVX < 0 &&
        bx <= LEFT_X + PADDLE_W && bx > LEFT_X - BALL_SIZE &&
        by + BALL_SIZE > leftY && by < leftY + PADDLE_H)
    {
        ballX = (LEFT_X + PADDLE_W) << 8;
        paddleHit(leftY, by, 1);
    }
    /* Right paddle */
    else if (ballVX > 0 &&
             bx + BALL_SIZE >= RIGHT_X && bx < RIGHT_X + PADDLE_W &&
             by + BALL_SIZE > rightY && by < rightY + PADDLE_H)
    {
        ballX = (RIGHT_X - BALL_SIZE) << 8;
        paddleHit(rightY, by, -1);
    }

    /* Scoring */
    if (bx + BALL_SIZE < 0)
    {
        scoreR++;
        drawScores();
        if (scoreR >= WIN_SCORE) { resetGame(); return; }
        serveBall(-1);              /* serve toward the player who lost the point */
    }
    else if (bx > SCREEN_W)
    {
        scoreL++;
        drawScores();
        if (scoreL >= WIN_SCORE) { resetGame(); return; }
        serveBall(1);
    }
}

static void drawSprites(void)
{
    const u16 paddleAttr = TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, T_PADDLE);
    const u16 ballAttr   = TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, T_BALL);

    /* VDP sprite coordinates have a 128 pixel offset */
    VDP_setSpriteFull(0, LEFT_X + 128,  leftY + 128,
                      SPRITE_SIZE(1, 4), paddleAttr, 1);
    VDP_setSpriteFull(1, RIGHT_X + 128, rightY + 128,
                      SPRITE_SIZE(1, 4), paddleAttr, 2);
    VDP_setSpriteFull(2, (ballX >> 8) + 128, (ballY >> 8) + 128,
                      SPRITE_SIZE(1, 1), ballAttr, 0);

    VDP_updateSprites(3, DMA_QUEUE);
}

static void initGraphics(void)
{
    u16 i;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette: 0 black, 1 white, 2 yellow, 15 white (default font color) */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFFFF00));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));

    /* Load tiles (paddle sprite needs 4 consecutive tiles) */
    for (i = 0; i < 4; i++)
        VDP_loadTileData(tileSolid, T_PADDLE + i, 1, CPU);
    VDP_loadTileData(tileBall, T_BALL, 1, CPU);
    VDP_loadTileData(tileNet,  T_NET,  1, CPU);

    VDP_setTextPlane(BG_A);
    VDP_setTextPalette(PAL0);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    /* Top and bottom walls on BG_B */
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, T_PADDLE),
                        0, PLAY_TOP / 8 - 1, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, T_PADDLE),
                        0, PLAY_BOT / 8, 40, 1);

    /* Center net */
    for (i = PLAY_TOP / 8; i < PLAY_BOT / 8; i++)
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, T_NET), 19, i);
    for (i = PLAY_TOP / 8; i < PLAY_BOT / 8; i++)
        VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, T_NET), 20, i);

    VDP_resetSprites();
}

int main(bool hardReset)
{
    (void)hardReset;

    JOY_init();
    initGraphics();
    setRandomSeed(0x1234);
    resetGame();

    while (TRUE)
    {
        updatePlayer();
        updateAI();
        updateBall();
        drawSprites();

        SYS_doVBlankProcess();
    }

    return 0;
}
