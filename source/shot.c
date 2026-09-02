//---------------------------------------------------------------------------------
// shot.c -- 自機弾と敵弾。どちらも固定長配列のオブジェクトプールで管理する。
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

Shot pshot[MAX_PSHOT];
Shot eshot[MAX_ESHOT];

void shotReset(void)
{
    memset(pshot, 0, sizeof(pshot));
    memset(eshot, 0, sizeof(eshot));
}

//---------------------------------------------------------------------------------
// 空きスロットを探して弾を置く。空きが無ければ黙って捨てる。
//---------------------------------------------------------------------------------
static void spawn(Shot *pool, int n, fx x, fx y, fx vx, fx vy, int kind, int w, int h)
{
    int i;
    for (i = 0; i < n; i++) {
        if (pool[i].alive) continue;
        pool[i].alive = 1;
        pool[i].x = x;  pool[i].y = y;
        pool[i].vx = vx; pool[i].vy = vy;
        pool[i].kind = (u8)kind;
        pool[i].w = (u8)w; pool[i].h = (u8)h;
        return;
    }
}

void shotFirePlayer(fx x, fx y, fx vx, fx vy, int kind)
{
    spawn(pshot, MAX_PSHOT, x, y, vx, vy, kind, (kind == 2) ? 6 : 4, 8);
}

void shotFireEnemy(fx x, fx y, fx vx, fx vy, int kind)
{
    spawn(eshot, MAX_ESHOT, x, y, vx, vy, kind, 5, 5);
}

//---------------------------------------------------------------------------------
// 自機に向かって撃つ。方向は自機との差分から近似で求める。
//---------------------------------------------------------------------------------
void shotFireAimed(fx x, fx y, fx speed, int kind)
{
    s32 dx = fx2i(player.x) - fx2i(x);
    s32 dy = fx2i(player.y) - fx2i(y);
    s32 len = sqrt32(dx * dx + dy * dy);
    if (len < 1) len = 1;
    shotFireEnemy(x, y, (fx)(dx * speed / len), (fx)(dy * speed / len), kind);
}

void shotClearEnemy(void)
{
    int i;
    for (i = 0; i < MAX_ESHOT; i++) {
        if (!eshot[i].alive) continue;
        eshot[i].alive = 0;
        effectSpawn(eshot[i].x, eshot[i].y, FX_SMALL);
    }
}

//---------------------------------------------------------------------------------
static int offscreen(const Shot *s)
{
    s32 x = fx2i(s->x), y = fx2i(s->y);
    return x < -16 || x > SCR_W + 16 || y < -16 || y > SCR_H + 16;
}

void shotUpdate(void)
{
    int i, j;

    // --- 自機弾: 移動 -> 敵とボスに当てる ---
    for (i = 0; i < MAX_PSHOT; i++) {
        Shot *s = &pshot[i];
        if (!s->alive) continue;
        s->x += s->vx;
        s->y += s->vy;
        if (offscreen(s)) { s->alive = 0; continue; }

        int dmg = (s->kind == 2) ? 2 : 1;

        for (j = 0; j < MAX_ENEMY; j++) {
            Enemy *e = &enemy[j];
            if (!e->alive) continue;
            if (!hits(s->x, s->y, s->w, s->h, e->x, e->y, 14, 14)) continue;
            e->hp -= dmg;
            s->alive = 0;
            if (e->hp <= 0) {
                e->alive = 0;
                effectSpawn(e->x, e->y, FX_BIG);
                gameAddScore(e->score);
            } else {
                effectSpawn(s->x, s->y, FX_SMALL);
            }
            break;
        }
        if (!s->alive) continue;

        if (boss.alive && !boss.dying &&
            hits(s->x, s->y, s->w, s->h, boss.x, boss.y, 46, 50)) {
            bossDamage(dmg);
            s->alive = 0;
            effectSpawn(s->x, s->y, FX_SMALL);
        }
    }

    // --- 敵弾: 移動 -> 自機に当てる ---
    for (i = 0; i < MAX_ESHOT; i++) {
        Shot *s = &eshot[i];
        if (!s->alive) continue;
        s->x += s->vx;
        s->y += s->vy;
        if (offscreen(s)) { s->alive = 0; continue; }

        if (player.alive && player.inv == 0 &&
            hits(s->x, s->y, s->w, s->h, player.x, player.y, PLAYER_HIT, PLAYER_HIT)) {
            s->alive = 0;
            playerDamage();
        }
    }
}

void shotRender(void)
{
    int i;
    for (i = 0; i < MAX_PSHOT; i++) {
        if (!pshot[i].alive) { oamClearSprite(&oamMain, OAM_PSHOT + i); continue; }
        oamSet(&oamMain, OAM_PSHOT + i,
               fx2i(pshot[i].x) - 4, fx2i(pshot[i].y) - 4,
               1, PB_SHOT, SpriteSize_8x8, SpriteColorFormat_16Color,
               gfxShot[pshot[i].kind], -1, false, false, false, false, false);
    }
    for (i = 0; i < MAX_ESHOT; i++) {
        if (!eshot[i].alive) { oamClearSprite(&oamMain, OAM_ESHOT + i); continue; }
        oamSet(&oamMain, OAM_ESHOT + i,
               fx2i(eshot[i].x) - 4, fx2i(eshot[i].y) - 4,
               1, PB_SHOT, SpriteSize_8x8, SpriteColorFormat_16Color,
               gfxShot[eshot[i].kind], -1, false, false, false, false, false);
    }
}
