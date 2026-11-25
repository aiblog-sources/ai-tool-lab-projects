/* Validation ROM for mdlines/sgdkhot: every count below is known by construction.
   Per game loop: 100 s32 divisions on LINE_DIV, 50 s32 multiplies on LINE_MUL, one call to spin(200),
   and in the VBlank callback 30 s32 divisions on LINE_VDIV (interrupt context). */
#include <genesis.h>

static volatile s32 num[8] = { 100000, -7777, 123456, 99, -100000, 31337, 65536, 4242 };
static volatile s32 den[8] = { 7, 13, -3, 1000, 17, -29, 255, 3 };
static volatile s32 acc, acc2, vacc;
static volatile u16 sink;

static __attribute__((noinline)) void spin(u16 n)
{
    u16 i;
    for (i = 0; i < n; i++)     /* LINE_SPIN */
        sink += i;
}

static void vint_cb(void)
{
    u16 i;
    for (i = 0; i < 30; i++)
        vacc += num[i & 7] / den[(i + 1) & 7];   /* LINE_VDIV */
}

int main(bool hard)
{
    u16 i;
    SYS_setVIntCallback(vint_cb);
    VDP_drawText("mdlines validation", 2, 2);
    while (TRUE)
    {
        for (i = 0; i < 100; i++)
            acc += num[i & 7] / den[i & 7];      /* LINE_DIV */
        for (i = 0; i < 50; i++)
            acc2 += num[i & 7] * den[i & 7];     /* LINE_MUL */
        spin(200);                                /* LINE_CALL */
        SYS_doVBlankProcess();
    }
    return 0;
}
