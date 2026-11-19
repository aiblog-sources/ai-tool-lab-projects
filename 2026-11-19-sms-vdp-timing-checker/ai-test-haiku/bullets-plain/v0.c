#include <SMSlib.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                      */
/* ------------------------------------------------------------------ */
#define NUM_BULLETS      40
#define NUM_STARS        64
#define STAR_ROWS        24            /* visible tile rows in 192-line mode */
#define STAR_TILE        1
#define BULLET_TILE      2
#define LAUNCH_INTERVAL  3             /* frames between bullet launches    */
#define TURN_INTERVAL    16            /* frames between direction steps    */
#define STAR_INTERVAL    8             /* frames between star row moves     */

/* World coordinates are in 1/16 pixel units */
#define WORLD_W          (256 * 16)
#define WORLD_H          (192 * 16)
#define CENTER_X         (128 * 16)
#define CENTER_Y         (96 * 16)

/* ------------------------------------------------------------------ */
/* Palettes (SMS colour format: 00BBGGRR via RGB macro)               */
/* ------------------------------------------------------------------ */
static const unsigned char bgPalette[16] = {
    RGB(0,0,0), RGB(3,3,3), RGB(1,1,2), RGB(0,0,1),
    RGB(2,2,3), RGB(1,2,3), RGB(0,1,2), RGB(3,3,2),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* sprite colour 0 is transparent */
static const unsigned char spritePalette[16] = {
    RGB(0,0,0), RGB(3,3,0), RGB(3,1,0), RGB(3,3,3),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* ------------------------------------------------------------------ */
/* Tiles (4bpp planar, 32 bytes per 8x8 tile, 4 bytes per row)        */
/* Only colour index 1 is used: plane 0 holds the pixel mask.         */
/* Tile 0 = blank, Tile 1 = star, Tile 2 = bullet                     */
/* ------------------------------------------------------------------ */
static const unsigned char tileData[96] = {
    /* tile 0: blank */
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,  0x00,0x00,0x00,0x00,

    /* tile 1: star */
    0x00,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x38,0x00,0x00,0x00,
    0x7C,0x00,0x00,0x00,
    0x38,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,

    /* tile 2: bullet (filled ball) */
    0x18,0x00,0x00,0x00,
    0x3C,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x3C,0x00,0x00,0x00,
    0x18,0x00,0x00,0x00
};

/* ------------------------------------------------------------------ */
/* Bullet direction table: 16 directions, velocity in 1/16 px/frame   */
/* (speed = 2 px/frame)                                               */
/* ------------------------------------------------------------------ */
static const signed char dirVX[16] = {
     32, 30, 22, 12,  0, -12, -22, -30,
    -32,-30,-22,-12,  0,  12,  22,  30
};
static const signed char dirVY[16] = {
      0, 12, 22, 30, 32,  30,  22,  12,
      0,-12,-22,-30,-32, -30, -22, -12
};

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */
static unsigned char starX[NUM_STARS];
static unsigned char starY[NUM_STARS];

static int           bulletX[NUM_BULLETS];
static int           bulletY[NUM_BULLETS];
static unsigned char bulletDir[NUM_BULLETS];
static unsigned char launched;       /* number of bullets fired so far */
static unsigned char launchTimer;
static unsigned char emitDir;        /* direction of the next shot     */

/* ------------------------------------------------------------------ */
/* Star field (background tiles)                                      */
/* ------------------------------------------------------------------ */
static void initStars(void)
{
    unsigned int seed = 0xACE1u;
    unsigned char i;

    for (i = 0; i < NUM_STARS; i++) {
        seed = seed * 25173u + 13849u;
        starX[i] = (unsigned char)((seed >> 8) & 31);
        starY[i] = (unsigned char)((seed >> 3) % STAR_ROWS);
        SMS_setTileatXY(starX[i], starY[i], STAR_TILE);
    }
}

static void moveStars(void)
{
    unsigned char i;

    /* erase all stars at their old positions */
    for (i = 0; i < NUM_STARS; i++)
        SMS_setTileatXY(starX[i], starY[i], 0);

    /* move each star down one row, wrapping at the bottom */
    for (i = 0; i < NUM_STARS; i++) {
        starY[i]++;
        if (starY[i] >= STAR_ROWS)
            starY[i] = 0;
    }

    /* draw all stars at their new positions */
    for (i = 0; i < NUM_STARS; i++)
        SMS_setTileatXY(starX[i], starY[i], STAR_TILE);
}

/* ------------------------------------------------------------------ */
/* Bullets (sprites)                                                  */
/* ------------------------------------------------------------------ */
static void updateBullets(unsigned char frame)
{
    unsigned char i;

    /* launch one bullet from the centre every LAUNCH_INTERVAL frames,
       each new shot rotating the emitter, which forms a spiral */
    if (launched < NUM_BULLETS) {
        launchTimer++;
        if (launchTimer >= LAUNCH_INTERVAL) {
            launchTimer = 0;
            bulletX[launched]   = CENTER_X;
            bulletY[launched]   = CENTER_Y;
            bulletDir[launched] = emitDir;
            emitDir = (emitDir + 1) & 15;
            launched++;
        }
    }

    /* periodically turn every flying bullet, curving the paths */
    if ((frame & (TURN_INTERVAL - 1)) == 0) {
        for (i = 0; i < launched; i++)
            bulletDir[i] = (bulletDir[i] + 1) & 15;
    }

    /* move and wrap around the screen edges */
    for (i = 0; i < launched; i++) {
        bulletX[i] += dirVX[bulletDir[i]];
        bulletY[i] += dirVY[bulletDir[i]];

        if (bulletX[i] < 0)
            bulletX[i] += WORLD_W;
        else if (bulletX[i] >= WORLD_W)
            bulletX[i] -= WORLD_W;

        if (bulletY[i] < 0)
            bulletY[i] += WORLD_H;
        else if (bulletY[i] >= WORLD_H)
            bulletY[i] -= WORLD_H;
    }
}

static void drawBullets(void)
{
    unsigned char i;

    for (i = 0; i < launched; i++)
        SMS_addSprite(bulletX[i] >> 4, bulletY[i] >> 4, BULLET_TILE);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
void main(void)
{
    unsigned char frame = 0;

    SMS_loadBGPalette(bgPalette);
    SMS_loadSpritePalette(spritePalette);
    SMS_loadTiles(tileData, 0, sizeof(tileData));

    /* clear the visible name table */
    SMS_VRAMmemsetW(SMS_PNTAddress, 0x0000, 32 * 24 * 2);

    initStars();

    SMS_initSprites();
    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();
        SMS_copySpritestoSAT();

        if ((frame & (STAR_INTERVAL - 1)) == 0)
            moveStars();

        updateBullets(frame);

        SMS_initSprites();
        drawBullets();

        frame++;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
