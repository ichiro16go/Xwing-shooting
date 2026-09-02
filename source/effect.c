//---------------------------------------------------------------------------------
// effect.c -- 爆発エフェクト
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

typedef struct {
    fx  x, y;
    u8  alive;
    u8  big;
    s16 t;
} Effect;

static Effect fxpool[MAX_EFFECT];

#define STEP_SMALL 4      // 1 コマあたりのフレーム数
#define STEP_BIG   6

void effectReset(void)
{
    memset(fxpool, 0, sizeof(fxpool));
}

void effectSpawn(fx x, fx y, int big)
{
    int i;
    for (i = 0; i < MAX_EFFECT; i++) {
        if (fxpool[i].alive) continue;
        fxpool[i].alive = 1;
        fxpool[i].big = (u8)big;
        fxpool[i].x = x;
        fxpool[i].y = y;
        fxpool[i].t = 0;

        // 大きい爆発は周囲にも小さい爆発を散らして見栄えを稼ぐ
        if (big) {
            int k;
            for (k = 0; k < 3; k++) {
                int j;
                for (j = 0; j < MAX_EFFECT; j++) {
                    if (fxpool[j].alive) continue;
                    fxpool[j].alive = 1;
                    fxpool[j].big = 0;
                    fxpool[j].x = x + FX(rndRange(20) - 10);
                    fxpool[j].y = y + FX(rndRange(20) - 10);
                    fxpool[j].t = (s16)(-4 * (k + 1));   // 少し遅れて出る
                    break;
                }
            }
        }
        return;
    }
}

void effectUpdate(void)
{
    int i;
    for (i = 0; i < MAX_EFFECT; i++) {
        if (!fxpool[i].alive) continue;
        fxpool[i].t++;
        int step = fxpool[i].big ? STEP_BIG : STEP_SMALL;
        if (fxpool[i].t >= step * 4) fxpool[i].alive = 0;
    }
}

void effectRender(void)
{
    int i;
    for (i = 0; i < MAX_EFFECT; i++) {
        Effect *e = &fxpool[i];
        if (!e->alive || e->t < 0) {
            oamClearSprite(&oamMain, OAM_EFFECT + i);
            continue;
        }
        int step = e->big ? STEP_BIG : STEP_SMALL;
        int frame = e->t / step;
        if (frame > 3) frame = 3;

        oamSet(&oamMain, OAM_EFFECT + i,
               fx2i(e->x) - 8, fx2i(e->y) - 8,
               0, PB_BLAST, SpriteSize_16x16, SpriteColorFormat_16Color,
               gfxBlast[frame], -1, false, false, false, false, false);
    }
}
