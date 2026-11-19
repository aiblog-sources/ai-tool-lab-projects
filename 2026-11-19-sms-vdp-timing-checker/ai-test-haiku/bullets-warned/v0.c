/*
 * Bullet spiral + star field - Sega Master System
 * devkitSMS / SMSlib / SDCC
 *
 *   sdcc -c -mz80 main.c
 *   link with crt0_sms.rel and SMSlib.lib, then run makesms
 *
 * 40 bullet sprites (8x8) are launched one by one from the screen centre.
 * Each one curves as it flies (its heading slowly turns), which forms a
 * spiral pattern. Bullets wrap around the screen edges forever.
 *
 * 64 stars are background tiles. They move down one tile row (8 pixels)
 * every 8 frames. This is done with the VDP vertical scroll register,
 * so no VRAM writes happen during the game loop.
 *
 * Timing: the loop runs once per VBlank. Every VRAM and SAT access
 * happens right after SMS_waitForVBlank(), and all other work is RAM only.
 */

#include <SMSlib.h>

#define NUM_BULLETS         40
#define NUM_STARS           64

/* Positions are stored in 1/16 pixel units */
#define SCREEN_W_UNITS      (256 * 16)
#define SCREEN_H_UNITS      (192 * 16)

/* Launch point: centre of the screen, offset by half a sprite (4 px) */
#define START_X             ((128 - 4) * 16)
#define START_Y             ((96 - 4) * 16)

/* Heading uses 64 steps per full turn (5.625 degrees per step) */
#define ANGLE_STEPS         64

/* ------------------------------------------------------------------ */
/* Data: quarter-wave sine table (0..90 deg in 16 steps), scaled to 127 */
/* ------------------------------------------------------------------ */
static const unsigned char quarter[17] =
{
    0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126, 127
};

/* ------------------------------------------------------------------ */
/* Data: tiles (4bpp, 32 bytes each: 8 rows x 4 bitplanes)             */
/* ------------------------------------------------------------------ */

/* Bullet: 8x8 diamond, colour index 1 (bitplane 0 only) */
static const unsigned char bullet_tile[32] =
{
    0x18,0x00,0x00,0x00,
    0x3C,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x7E,0x00,0x00,0x00,
    0x3C,0x00,0x00,0x00,
    0x18,0x00,0x00,0x00
};

/* Star: small plus sign, colour index 1 (bitplane 0 only) */
static const unsigned char star_tile[32] =
{
    0x00,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x7C,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x10,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00
};

/* ------------------------------------------------------------------ */
/* Data: palettes (SMS CRAM format: --BBGGRR, 2 bits per channel)     */
/* ------------------------------------------------------------------ */

/* Background: 0 = black backdrop, 1 = white (stars) */
static const unsigned char bg_palette[16] =
{
    0x00, 0x3F, 0x15, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* Sprites: 0 = transparent, 1 = yellow (bullets) */
static const unsigned char spr_palette[16] =
{
    0x00, 0x0F, 0x03, 0x3F,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* ------------------------------------------------------------------ */
/* Game state (RAM)                                                   */
/* ------------------------------------------------------------------ */
static int           bx[NUM_BULLETS];
static int           by[NUM_BULLETS];
static unsigned char bdir[NUM_BULLETS];
static unsigned char bactive[NUM_BULLETS];

static unsigned char star_x[NUM_STARS];
static unsigned char star_y[NUM_STARS];

static unsigned int  frame;
static unsigned char sprite_rot;     /* rotates draw order to spread flicker */
static unsigned char bg_scroll_y;
static unsigned char scroll_timer;
static unsigned int  rng_state;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* sin(a * 360/64) * 127, a in 0..63 */
static int sinq(unsigned char a)
{
    unsigned char i = a & 15;

    switch (a >> 4) {
        case 0:  return (int)quarter[i];
        case 1:  return (int)quarter[16 - i];
        case 2:  return -(int)quarter[i];
        default: return -(int)quarter[16 - i];
    }
}

/* Simple 16-bit LCG for star placement */
static unsigned int rng_next(void)
{
    rng_state = rng_state * 25173u + 13849u;
    return rng_state >> 8;
}

/* Place 64 distinct stars as background tile 1 */
static void setup_stars(void)
{
    unsigned char n = 0;
    unsigned char k;
    unsigned char x, y;
    unsigned char dup;

    rng_state = 0x1234u;

    while (n < NUM_STARS) {
        x = (unsigned char)(rng_next() % 32u);
        y = (unsigned char)(rng_next() % 28u);

        dup = 0;
        for (k = 0; k < n; k++) {
            if (star_x[k] == x && star_y[k] == y) {
                dup = 1;
                break;
            }
        }

        if (!dup) {
            star_x[n] = x;
            star_y[n] = y;
            SMS_setTileatXY(x, y, 1);   /* tile 1 = star */
            n++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Bullets                                                            */
/* ------------------------------------------------------------------ */

/* RAM-only update. Called once per frame, after SMS_waitForVBlank(). */
static void update_bullets(void)
{
    unsigned char i;
    unsigned char spin;
    int vx, vy;

    /* Heading turns by one step every 4 frames. This makes the paths curve. */
    spin = ((frame & 3u) == 0u);

    for (i = 0; i < NUM_BULLETS; i++) {

        if (!bactive[i]) {
            /* Launch bullet i at frame 2*i, from the centre, along its own heading */
            if (frame >= (unsigned int)i * 2u) {
                bactive[i] = 1;
                bx[i] = START_X;
                by[i] = START_Y;
                bdir[i] = (unsigned char)(((unsigned int)i * 13u) >> 3); /* spread over 0..63 */
            }
            continue;
        }

        if (spin)
            bdir[i] = (unsigned char)((bdir[i] + 1) & (ANGLE_STEPS - 1));

        /* Velocity in 1/16 px per frame (max about 2 px per frame) */
        vx = sinq((unsigned char)((bdir[i] + 16) & (ANGLE_STEPS - 1))) / 4;
        vy = sinq(bdir[i]) / 4;

        bx[i] += vx;
        by[i] += vy;

        /* Wrap around the screen edges */
        if (bx[i] < 0)
            bx[i] += SCREEN_W_UNITS;
        else if (bx[i] >= SCREEN_W_UNITS)
            bx[i] -= SCREEN_W_UNITS;

        if (by[i] < 0)
            by[i] += SCREEN_H_UNITS;
        else if (by[i] >= SCREEN_H_UNITS)
            by[i] -= SCREEN_H_UNITS;
    }
}

/* Add active bullets to the sprite buffer. Draw order rotates each frame. */
static void add_bullet_sprites(void)
{
    unsigned char k, j;

    for (k = 0; k < NUM_BULLETS; k++) {
        j = (unsigned char)(sprite_rot + k);
        if (j >= NUM_BULLETS)
            j = (unsigned char)(j - NUM_BULLETS);

        if (bactive[j])
            SMS_addSprite(bx[j] >> 4, by[j] >> 4, 0);   /* sprite tile 0 = VRAM tile 256 */
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
void main(void)
{
    unsigned char i;

    SMS_displayOff();

    /* Sprites use VRAM tiles 256..511. Sprite tile 0 is the bullet. */
    SMS_useFirstHalfTilesforSprites(0);

    /* Tiles: 0 = blank, 1 = star, 256 = bullet */
    SMS_VRAMmemset(0, 0, 32);
    SMS_loadTiles(star_tile, 1, 32);
    SMS_loadTiles(bullet_tile, 256, 32);

    /* Palettes */
    SMS_setBackdropColor(0);
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* Clear the background tilemap, then place the stars */
    SMS_VRAMmemset(SMS_PNTAddress, 0, 1792);   /* 32 x 28 entries, 2 bytes each */
    setup_stars();

    SMS_setBGScrollX(0);
    SMS_setBGScrollY(0);

    /* Game state */
    SMS_initSprites();
    for (i = 0; i < NUM_BULLETS; i++) {
        bactive[i] = 0;
        bdir[i] = 0;
        bx[i] = START_X;
        by[i] = START_Y;
    }
    frame = 0;
    sprite_rot = 0;
    bg_scroll_y = 0;
    scroll_timer = 0;

    SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ);
    SMS_displayOn();

    while (1) {
        SMS_waitForVBlank();

        /* ---- VBlank: all VDP work happens here ---- */
        SMS_copySpritestoSAT();

        /*
         * Star field: scrolling 8 px (one tile row) every 8 frames.
         * Vertical scroll decreases so the map moves down on screen.
         * The map is 28 rows (224 px), so the value wraps 0 -> 216 -> ... -> 0.
         */
        scroll_timer++;
        if (scroll_timer == 8) {
            scroll_timer = 0;
            if (bg_scroll_y == 0)
                bg_scroll_y = 216;
            else
                bg_scroll_y = (unsigned char)(bg_scroll_y - 8);
            SMS_setBGScrollY(bg_scroll_y);
        }

        /* ---- Active display: game logic (RAM only) ---- */
        update_bullets();

        SMS_initSprites();
        add_bullet_sprites();

        frame++;
        sprite_rot++;
        if (sprite_rot >= NUM_BULLETS)
            sprite_rot = 0;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
