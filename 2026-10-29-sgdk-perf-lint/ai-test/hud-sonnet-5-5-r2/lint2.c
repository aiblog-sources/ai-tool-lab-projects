/*
 * main.c - Game HUD demo for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 *  - Countdown timer (MM:SS), starts at 3:00
 *  - Score: +10 every frame while A is held
 *  - Lives (heart tiles). One is lost when health reaches 0
 *  - Health bar drawn with tiles, shrinks while B is held
 *  - START restarts after GAME OVER / TIME UP
 *
 * No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData.
 * Screen: 320x224 (40x28 tiles). Controls: pad 1.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Tiles (4bpp, 8 u32 per tile, one row per u32, leftmost pixel = high nibble) */
/* Palette 1 colors: 1 = green (fill), 2 = dark gray (empty), 3 = red, 4 = white */
/* ------------------------------------------------------------------ */

#define BAR_TILE(f)  { 0x44444444, f, f, f, f, f, f, 0x44444444 }

/* Health bar segment: 0..8 pixels filled (left to right) */
static const u32 barTiles[9][8] =
{
    BAR_TILE(0x22222222),
    BAR_TILE(0x12222222),
    BAR_TILE(0x11222222),
    BAR_TILE(0x11122222),
    BAR_TILE(0x11112222),
    BAR_TILE(0x11111222),
    BAR_TILE(0x11111122),
    BAR_TILE(0x11111112),
    BAR_TILE(0x11111111)
};

/* Heart icon for lives */
static const u32 heartTile[8] =
{
    0x00000000,
    0x03303300,
    0x33333330,
    0x33333330,
    0x03333300,
    0x00333000,
    0x00030000,
    0x00000000
};

#define T_BAR0      (TILE_USER_INDEX)           /* 9 tiles */
#define T_HEART     (TILE_USER_INDEX + 9)

/* ------------------------------------------------------------------ */
/* Game constants                                                      */
/* ------------------------------------------------------------------ */

#define START_SECONDS   180             /* 3:00 */
#define START_LIVES     3
#define MAX_LIVES       5
#define BAR_SEGMENTS    10
#define BAR_PIXELS_SEG  8
#define MAX_HEALTH      (BAR_SEGMENTS * BAR_PIXELS_SEG)
#define MAX_SCORE       999999

/* HUD layout (tile coordinates) */
#define TIME_X      2
#define TIME_Y      1
#define SCORE_X     22
#define SCORE_Y     1
#define LIVES_X     2
#define LIVES_Y     3
#define HEALTH_X    2
#define HEALTH_Y    5
#define BAR_X       9
#define BAR_Y       5
#define MSG_X       14
#define MSG_Y       11

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static u32  score;
static u16  secondsLeft;
static u16  frameCount;
static u16  lives;
static s16  health;
static u16  healthTick;
static bool gameOver;

static bool dirtyTime, dirtyScore, dirtyLives, dirtyBar;

/* Minutes value currently on screen (0xFFFF = force redraw) */
static u16  shownMinutes;

/* ------------------------------------------------------------------ */
/* Drawing helpers                                                     */
/* ------------------------------------------------------------------ */

static void drawNum(u32 value, u16 minSize, u16 x, u16 y)
{
    char str[12];
    intToStr(value, str, minSize);
    VDP_drawText(str, x, y);
}

static void drawTime(void)
{
    u16 minutes = secondsLeft / 60;

    /* Minutes change only once a minute */
    if (minutes != shownMinutes)
    {
        shownMinutes = minutes;
        drawNum(minutes, 2, TIME_X + 5, TIME_Y);
    }
    drawNum(secondsLeft % 60, 2, TIME_X + 8, TIME_Y);
}

static void drawScore(void)
{
    drawNum(score, 6, SCORE_X + 6, SCORE_Y);
}

static void drawLives(void)
{
    u16 buf[MAX_LIVES];

    for (u16 i = 0; i < MAX_LIVES; i++)
        buf[i] = (i < lives) ? TILE_ATTR_FULL(PAL1, 0, 0, 0, T_HEART) : 0;

    /* CPU transfer: buf lives on the stack, so DMA (queued) would be unsafe */
    VDP_setTileMapDataRect(BG_A, buf, LIVES_X + 6, LIVES_Y, MAX_LIVES, 1, MAX_LIVES, CPU);
}

static void drawHealthBar(void)
{
    u16 buf[BAR_SEGMENTS];

    for (u16 i = 0; i < BAR_SEGMENTS; i++)
    {
        s16 fill = health - (s16)(i * BAR_PIXELS_SEG);
        if (fill < 0) fill = 0;
        if (fill > BAR_PIXELS_SEG) fill = BAR_PIXELS_SEG;
        buf[i] = TILE_ATTR_FULL(PAL1, 0, 0, 0, T_BAR0 + fill);
    }

    VDP_setTileMapDataRect(BG_A, buf, BAR_X, BAR_Y, BAR_SEGMENTS, 1, BAR_SEGMENTS, CPU);
}

static void showMessage(const char *line1, const char *line2)
{
    VDP_drawText(line1, MSG_X, MSG_Y);
    VDP_drawText(line2, MSG_X - 1, MSG_Y + 2);
}

static void clearMessage(void)
{
    VDP_clearTextArea(MSG_X - 2, MSG_Y, 16, 3);
}

static void resetGame(void)
{
    score       = 0;
    secondsLeft = START_SECONDS;
    frameCount  = 0;
    lives       = START_LIVES;
    health      = MAX_HEALTH;
    healthTick  = 0;
    gameOver    = FALSE;

    clearMessage();

    shownMinutes = 0xFFFF;
    dirtyTime = dirtyScore = dirtyLives = dirtyBar = TRUE;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    const u16 fps = IS_PAL_SYSTEM ? 50 : 60;
    u16 prevPad = 0;

    /* Video setup: 320x224 */
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setTextPlane(BG_A);

    /* Palettes */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x000000));  /* background */
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* default font color (PAL0) */
    PAL_setColor(16, RGB24_TO_VDPCOLOR(0x000000));  /* PAL1 transparent */
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x00E000));  /* bar fill */
    PAL_setColor(18, RGB24_TO_VDPCOLOR(0x404040));  /* bar empty */
    PAL_setColor(19, RGB24_TO_VDPCOLOR(0xE00000));  /* heart */
    PAL_setColor(20, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* bar border */

    /* Load tiles from const arrays */
    VDP_loadTileData((const u32 *)barTiles,  T_BAR0,  9, CPU);
    VDP_loadTileData(heartTile,              T_HEART, 1, CPU);

    /* Static labels (the time colon never changes, so it is drawn once here) */
    VDP_drawText("TIME",   TIME_X,   TIME_Y);
    VDP_drawText(":",      TIME_X + 7, TIME_Y);
    VDP_drawText("SCORE",  SCORE_X,  SCORE_Y);
    VDP_drawText("LIVES",  LIVES_X,  LIVES_Y);
    VDP_drawText("HEALTH", HEALTH_X, HEALTH_Y);
    VDP_drawText("A: SCORE   B: DAMAGE", 10, 24);

    resetGame();

    while (TRUE)
    {
        u16 pad     = JOY_readJoypad(JOY_1);
        u16 pressed = pad & ~prevPad;
        prevPad = pad;

        if (!gameOver)
        {
            /* Score: +10 per frame while A held */
            if (pad & BUTTON_A)
            {
                score += 10;
                if (score > MAX_SCORE) score = MAX_SCORE;
                dirtyScore = TRUE;
            }

            /* Health: shrinks while B held (1 unit every 2 frames) */
            if (pad & BUTTON_B)
            {
                if (++healthTick >= 2)
                {
                    healthTick = 0;
                    if (health > 0)
                    {
                        health--;
                        dirtyBar = TRUE;
                    }
                }

                if (health <= 0)
                {
                    /* Lose a life, refill health */
                    if (lives > 0) lives--;
                    dirtyLives = TRUE;

                    if (lives == 0)
                    {
                        gameOver = TRUE;
                        showMessage("GAME OVER", "PRESS START");
                    }
                    else
                    {
                        health = MAX_HEALTH;
                        dirtyBar = TRUE;
                    }
                }
            }
            else
            {
                healthTick = 0;
            }

            /* Countdown timer */
            if (!gameOver && ++frameCount >= fps)
            {
                frameCount = 0;
                if (secondsLeft > 0)
                {
                    secondsLeft--;
                    dirtyTime = TRUE;
                }
                if (secondsLeft == 0)
                {
                    gameOver = TRUE;
                    showMessage("TIME UP", "PRESS START");
                }
            }
        }
        else if (pressed & BUTTON_START)
        {
            resetGame();
        }

        /* Redraw only what changed */
        if (dirtyTime)  { drawTime();      dirtyTime  = FALSE; }
        if (dirtyScore) { drawScore();     dirtyScore = FALSE; }
        if (dirtyLives) { drawLives();     dirtyLives = FALSE; }
        if (dirtyBar)   { drawHealthBar(); dirtyBar   = FALSE; }

        SYS_doVBlankProcess();
    }

    return 0;
}
