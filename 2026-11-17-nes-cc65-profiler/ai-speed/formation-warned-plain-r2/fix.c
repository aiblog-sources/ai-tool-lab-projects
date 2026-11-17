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

static void put_ship_static(unsigned char i, unsigned char y, unsigned char attr)
{
    unsigned char o = i << 2;
    OAM[o + 0] = y;
    OAM[o + 1] = 0x10;
    OAM[o + 2] = attr;
    OAM[o + 4] = y;
    OAM[o + 5] = 0x10;
    OAM[o + 6] = attr | 0x40;
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
}

/* Only the dynamic bytes are written; tiles/attrs/Ys of ships are set once. */
static void draw(void)
{
    unsigned char i, c, x;
    unsigned char *p;

    /* player */
    OAM[3] = player_x;
    OAM[7] = player_x + 8;

    /* bullets: OAM entries 2..7 */
    p = OAM + 8;
    for (i = 0; i < MAX_BULLETS; ++i) {
        if (bullet_on[i]) {
            p[0] = bullet_y[i] - 8;
            p[3] = bullet_x[i];
        } else {
            p[0] = 0xFF;
        }
        p += 4;
    }

    /* enemy formation: entries 8.., two rows of 10 ships (2 sprites each) */
    p = OAM + 32;
    x = form_x;
    for (c = 0; c < ENEMY_COLS; ++c) {
        p[3]  = x;
        p[7]  = x + 8;
        p[83] = x;
        p[87] = x + 8;
        p += 8;
        x += ENEMY_PITCH;
    }
}

void main(void)
{
    unsigned char i, r, c, n;

    ppu_setup(palette);
    ppu_ctrl |= CTRL_SPR_8X16;

    player_x = 120;
    form_x = FORM_MIN_X;
    form_dir = 1;
    pad = prev_pad = 0;
    for (i = 0; i < MAX_BULLETS; ++i) bullet_on[i] = 0;

    /* static OAM contents, written once */
    oam_hide_all();
    put_ship_static(0, PLAYER_Y, 0);
    for (i = 0; i < MAX_BULLETS; ++i) {
        OAM_TILE(2 + i) = 0x00;
        OAM_ATTR(2 + i) = 1;
    }
    n = 2 + MAX_BULLETS;
    for (r = 0; r < ENEMY_ROWS; ++r) {
        for (c = 0; c < ENEMY_COLS; ++c) {
            put_ship_static(n, ENEMY_ROW_Y0 + r * ENEMY_ROW_DY, 1 + r);
            n += 2;
        }
    }

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
