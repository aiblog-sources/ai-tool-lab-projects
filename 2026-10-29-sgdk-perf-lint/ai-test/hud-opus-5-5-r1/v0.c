/*
 * main.c - Game HUD demo for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * Countdown timer in minutes:seconds
 * Score that increases by 10 every frame while A is held
 * Lives counter
 * Tile-based health bar that shrinks while B is held
 *
 * When health reaches 0, a life is lost and the bar refills.
 * When the timer or lives run out, press START to restart.
 *
 * No resource files. All tiles are const u32 arrays loaded with VDP_loadTileData.
 */

#include <genesis.h>

/* Layout (40x28 tile screen at 320x224) */
#define SCORE_X         2
#define SCORE_Y         2
#define TIME_X          29
#define TIME_Y          2
#define HP_LABEL_X      2
#define BAR_X           5
#define BAR_Y           4
#define LIVES_X         29
#define LIVES_Y         4
#define MSG_Y           13

/* Health bar settings */
#define BAR_TILES       16
#define HEALTH_MAX      (BAR_TILES * 8)     /* 8 health units per tile = 1 per pixel */
#define DRAIN_DELAY     2                   /* frames per health unit while B held */

/* Game settings */
#define START_LIVES     3
#define START_TIME_SEC  (3 * 60)            /* 3:00 */
#define SCORE_MAX       99999990UL

/* Tile VRAM location */
#define TILE_BAR_BASE   TILE_USER_INDEX

/*
 * Health bar tiles: index n = n pixels filled (0..8).
 * Color 1 = fill, color 2 = empty background, color 3 = border.
 * Each u32 is one row of 8 pixels, 4 bits per pixel, leftmost pixel in the high nibble.
 */
#define BORDER_ROW  0x33333333
#define TILE_ROWS(r) BORDER_ROW, r, r, r, r, r, r, BORDER_ROW

static const u32 barTiles[9 * 8] =
{
    TILE_ROWS(0x22222222),   /* 0/8 */
    TILE_ROWS(0x12222222),   /* 1/8 */
    TILE_ROWS(0x11222222),   /* 2/8 */
    TILE_ROWS(0x11122222),   /* 3/8 */
    TILE_ROWS(0x11112222),   /* 4/8 */
    TILE_ROWS(0x11111222),   /* 5/8 */
    TILE_ROWS(0x11111122),   /* 6/8 */
    TILE_ROWS(0x11111112),   /* 7/8 */
    TILE_ROWS(0x11111111)    /* 8/8 */
};

/* Game state */
typedef enum
{
    STATE_RUNNING,
    STATE_TIME_UP,
    STATE_GAME_OVER
} GameState;

static GameState state;
static u32 score;
static u16 lives;
static u16 health;
static u16 timeLeft;        /* seconds */
static u16 frameCounter;
static u16 drainCounter;
static u16 fps;
static s16 barColorLevel;   /* -1 = not set yet */

/* Dirty flags so only changed HUD parts are redrawn */
static bool scoreDirty;
static bool timeDirty;
static bool livesDirty;

/* ------------------------------------------------------------------ */

static void drawBarTile(u16 i)
{
    s16 fill = (s16)health - (s16)(i * 8);
    if (fill < 0) fill = 0;
    if (fill > 8) fill = 8;

    VDP_setTileMapXY(BG_A,
                     TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BAR_BASE + fill),
                     BAR_X + i, BAR_Y);
}

static void drawWholeBar(void)
{
    for (u16 i = 0; i < BAR_TILES; i++)
        drawBarTile(i);
}

/* Bar fill color: green > 50%, yellow > 25%, red otherwise. Only touches CRAM on change. */
static void updateBarColor(void)
{
    s16 level;

    if (health > HEALTH_MAX / 2)      level = 2;
    else if (health > HEALTH_MAX / 4) level = 1;
    else                              level = 0;

    if (level == barColorLevel) return;
    barColorLevel = level;

    switch (level)
    {
        case 2:  PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x00E000)); break;
        case 1:  PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xE0E000)); break;
        default: PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xE00000)); break;
    }
}

static void drawScore(void)
{
    char str[12];
    uintToStr(score, str, 8);
    VDP_drawText(str, SCORE_X + 6, SCORE_Y);
}

static void drawTime(void)
{
    char str[6];
    u16 m = timeLeft / 60;
    u16 s = timeLeft - (m * 60);

    str[0] = '0' + (m / 10);
    str[1] = '0' + (m % 10);
    str[2] = ':';
    str[3] = '0' + (s / 10);
    str[4] = '0' + (s % 10);
    str[5] = 0;

    VDP_drawText(str, TIME_X + 5, TIME_Y);
}

static void drawLives(void)
{
    char str[4];
    uintToStr(lives, str, 2);
    VDP_drawText(str, LIVES_X + 6, LIVES_Y);
}

static void drawStaticHud(void)
{
    VDP_drawText("SCORE", SCORE_X, SCORE_Y);
    VDP_drawText("TIME", TIME_X, TIME_Y);
    VDP_drawText("HP", HP_LABEL_X, BAR_Y);
    VDP_drawText("LIVES", LIVES_X, LIVES_Y);
    VDP_drawText("HOLD A: SCORE   HOLD B: DAMAGE", 5, 25);
}

static void clearMessage(void)
{
    VDP_clearText(0, MSG_Y, 40);
    VDP_clearText(0, MSG_Y + 2, 40);
}

static void showMessage(const char *msg)
{
    u16 len = strlen(msg);
    VDP_drawText(msg, (40 - len) / 2, MSG_Y);
    VDP_drawText("PRESS START", (40 - 11) / 2, MSG_Y + 2);
}

static void resetGame(void)
{
    state = STATE_RUNNING;
    score = 0;
    lives = START_LIVES;
    health = HEALTH_MAX;
    timeLeft = START_TIME_SEC;
    frameCounter = 0;
    drainCounter = 0;

    clearMessage();
    drawWholeBar();
    updateBarColor();

    scoreDirty = TRUE;
    timeDirty = TRUE;
    livesDirty = TRUE;
}

/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    u16 prevJoy = 0;

    VDP_setScreenWidth320();
    fps = SYS_isPAL() ? 50 : 60;

    /* Load health bar tiles */
    VDP_loadTileData(barTiles, TILE_BAR_BASE, 9, DMA);

    /* PAL0 keeps the default font color (index 15 = white).
       PAL1 is for the health bar. */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));               /* backdrop */
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x202040));          /* empty part */
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xC0C0C0));          /* border */

    barColorLevel = -1;
    drawStaticHud();
    resetGame();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;

        if (state == STATE_RUNNING)
        {
            /* Score: +10 per frame while A held */
            if ((joy & BUTTON_A) && score < SCORE_MAX)
            {
                score += 10;
                if (score > SCORE_MAX) score = SCORE_MAX;
                scoreDirty = TRUE;
            }

            /* Health: shrink while B held */
            if (joy & BUTTON_B)
            {
                if (++drainCounter >= DRAIN_DELAY)
                {
                    drainCounter = 0;

                    if (health > 0)
                    {
                        health--;
                        drawBarTile(health >> 3);   /* only the tile that changed */
                        updateBarColor();

                        if (health == 0)
                        {
                            lives--;
                            livesDirty = TRUE;

                            if (lives == 0)
                            {
                                state = STATE_GAME_OVER;
                                showMessage("GAME OVER");
                            }
                            else
                            {
                                health = HEALTH_MAX;
                                drawWholeBar();
                                updateBarColor();
                            }
                        }
                    }
                }
            }
            else
            {
                drainCounter = 0;
            }

            /* Countdown timer */
            if (state == STATE_RUNNING && ++frameCounter >= fps)
            {
                frameCounter = 0;
                if (timeLeft > 0)
                {
                    timeLeft--;
                    timeDirty = TRUE;

                    if (timeLeft == 0)
                    {
                        state = STATE_TIME_UP;
                        showMessage("TIME UP!");
                    }
                }
            }
        }
        else if (pressed & BUTTON_START)
        {
            resetGame();
        }

        /* Redraw only what changed */
        if (scoreDirty) { drawScore(); scoreDirty = FALSE; }
        if (timeDirty)  { drawTime();  timeDirty  = FALSE; }
        if (livesDirty) { drawLives(); livesDirty = FALSE; }

        SYS_doVBlankProcess();
    }

    return 0;
}
