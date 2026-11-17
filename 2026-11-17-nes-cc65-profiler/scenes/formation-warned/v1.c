#include "nes.h"

#define ENEMY_COLS   10          /* spec: 10. Hardware fits only 4 per row without dropouts. */
#define ENEMY_ROWS   2
#define ENEMY_PITCH  20
#define ENEMY_ROW_Y0 40
#define ENEMY_ROW_DY 24
#define FORM_WIDTH   ((ENEMY_COLS - 1) * ENEMY_PITCH + 16)
#define FORM_MIN_X   8
#define FORM_MAX_X   (256 - 8 - FORM_WIDTH)

#define MAX_BULLETS  6
#define PLAYER_Y     208
#define PLAYER_MIN_X 8
#define PLAYER_MAX_X 232

#define ROT_STEP     3           /* enemy OAM start advances this many ships per frame */

static const unsigned char palette[32] = {
    /* background */
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    /* sprites */
    0x0F,0x21,0x11,0x30, 0x0F,0x16,0x26,0x30,
    0x0F,0x1A,0x2A,0x30, 0x0F,0x28,0x38,0x30
};

static unsigned char bullet_y[MAX_BULLETS];
static unsigned char bullet_x[MAX_BULLETS];
static unsigned char bullet_on[MAX_BULLETS];

static unsigned char player_x;
static unsigned char form_x;
static signed char   form_dir;
static unsigned char pad, prev_pad;
static unsigned char rot;

static void put_ship(unsigned char i, unsigned char x, unsigned char y,
                     unsigned char attr)
{
    unsigned char *p = &OAM[(unsigned char)(i << 2)];
    unsigned char flip = attr | 0x40;   /* horizontal flip for the right half */
    p[0] = y;
    p[1] = 0x10;
    p[2] = attr;
    p[3] = x;
    p[4] = y;
    p[5] = 0x10;
    p[6] = flip;
    p[7] = x + 8;
}

static void update(void)
{
    unsigned char i;

    prev_pad = pad;
    pad = pad_read();

    if ((pad & PAD_LEFT) && player_x > PLAYER_MIN_X) player_x -= 2;
    if ((pad & PAD_RIGHT) && player_x < PLAYER_MAX_X) player_x += 2;

    if ((pad & PAD_A) && !(prev_pad & PAD_A)) {
        for (i = 0; i < MAX_BULLETS; ++i) {
            if (!bullet_on[i]) {
                bullet_on[i] = 1;
                bullet_x[i] = player_x + 4;
                bullet_y[i] = PLAYER_Y - 4;
                break;
            }
        }
    }

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (bullet_on[i]) {
            if (bullet_y[i] < 16) bullet_on[i] = 0;
            else bullet_y[i] -= 4;
        }
    }

    if (form_dir > 0) {
        if (form_x >= FORM_MAX_X) form_dir = -1; else ++form_x;
    } else {
        if (form_x <= FORM_MIN_X) form_dir = 1; else --form_x;
    }

    /* rotate which enemies get the earliest OAM slots */
    rot += ROT_STEP;
    if (rot >= ENEMY_COLS) rot -= ENEMY_COLS;
}

static void draw(void)
{
    unsigned char i, r, c, k, n, x, y;

    oam_hide_all();
    n = 0;

    /* player first: lowest OAM index = highest priority */
    put_ship(n, player_x, PLAYER_Y, 0);
    n += 2;

    /* bullets: tile $00 in 8x16 mode = blank top, solid bottom => 8x8 at Y */
    for (i = 0; i < MAX_BULLETS; ++i, ++n) {
        if (bullet_on[i]) {
            OAM_Y(n)    = bullet_y[i] - 8;
            OAM_TILE(n) = 0x00;
            OAM_ATTR(n) = 1;
            OAM_X(n)    = bullet_x[i];
        }
    }

    /* enemy formation: start from a rotating column so the sprites that
       overflow the 8-per-line limit are different ones each frame */
    y = ENEMY_ROW_Y0;
    for (r = 0; r < ENEMY_ROWS; ++r) {
        c = rot;
        x = form_x + rot * ENEMY_PITCH;
        for (k = 0; k < ENEMY_COLS; ++k) {
            put_ship(n, x, y, 1 + r);
            n += 2;
            if (++c >= ENEMY_COLS) {
                c = 0;
                x = form_x;
            } else {
                x += ENEMY_PITCH;
            }
        }
        y += ENEMY_ROW_DY;
    }
}

void main(void)
{
    unsigned char i;

    ppu_setup(palette);
    ppu_ctrl |= CTRL_SPR_8X16;

    player_x = 120;
    form_x = FORM_MIN_X;
    form_dir = 1;
    pad = prev_pad = 0;
    rot = 0;
    for (i = 0; i < MAX_BULLETS; ++i) bullet_on[i] = 0;

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
