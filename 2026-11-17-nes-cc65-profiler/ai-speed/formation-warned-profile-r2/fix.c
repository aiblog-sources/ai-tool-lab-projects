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

/* One-time setup of the constant OAM fields (Y, tile, attr). */
static void init_oam(void)
{
    unsigned char s, o, y, a;

    oam_hide_all();

    /* player: entries 0,1 */
    OAM[0] = PLAYER_Y; OAM[1] = 0x10; OAM[2] = 0;    
    OAM[4] = PLAYER_Y; OAM[5] = 0x10; OAM[6] = 0x40;

    /* bullets: entries 2..7 (tile/attr constant, Y stays $FF until active) */
    for (s = 0; s < MAX_BULLETS; ++s) {
        o = (2 + s) << 2;
        OAM[o + 1] = 0x00;
        OAM[o + 2] = 1;
    }

    /* enemies: entries 8..47 */
    o = 32;
    for (s = 0; s < ENEMY_ROWS; ++s) {
        y = ENEMY_ROW_Y0 + s * ENEMY_ROW_DY;
        a = 1 + s;
        {
            unsigned char c;
            for (c = 0; c < ENEMY_COLS; ++c) {
                OAM[o + 0] = y; OAM[o + 1] = 0x10; OAM[o + 2] = a;
                OAM[o + 4] = y; OAM[o + 5] = 0x10; OAM[o + 6] = a | 0x40;
                o += 8;
            }
        }
    }
}

static void draw(void)
{
    unsigned char i, x;
    unsigned char *p;

    /* player */
    OAM[3] = player_x;
    OAM[7] = player_x + 8;

    /* bullets */
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

    /* enemy formation: only X changes; both rows share the same X */
    x = form_x;
    p = OAM + 32 + 3;
    for (i = 0; i < ENEMY_COLS; ++i) {
        p[0]  = x;
        p[4]  = x + 8;
        p[80] = x;
        p[84] = x + 8;
        p += 8;
        x += ENEMY_PITCH;
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
    for (i = 0; i < MAX_BULLETS; ++i) bullet_on[i] = 0;

    init_oam();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
