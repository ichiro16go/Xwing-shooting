//---------------------------------------------------------------------------------
// enemy.c -- 敵機 TIE ファイター / インターセプター
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

Enemy enemy[MAX_ENEMY];

#define ENEMY_HIT 14

void enemyReset(void)
{
    memset(enemy, 0, sizeof(enemy));
}

int enemyAlive(void)
{
    int i, n = 0;
    for (i = 0; i < MAX_ENEMY; i++) if (enemy[i].alive) n++;
    return n;
}

Enemy *enemySpawn(int kind, int pattern, fx x, fx y, fx vx, fx vy)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &enemy[i];
        if (e->alive) continue;

        e->alive   = 1;
        e->kind    = (u8)kind;
        e->pattern = (u8)pattern;
        e->x = e->baseX = x;
        e->y  = y;
        e->vx = vx;
        e->vy = vy;
        e->t  = 0;

        if (kind == EK_INTERCEPTOR) {
            e->hp = 3;
            e->score = 200;
            e->fireTimer = 50 + rndRange(40);
        } else {
            e->hp = 2;
            e->score = 100;
            e->fireTimer = 70 + rndRange(60);
        }
        return e;
    }
    return NULL;
}

void enemyDamageAll(int dmg)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &enemy[i];
        if (!e->alive) continue;
        e->hp -= dmg;
        if (e->hp <= 0) {
            e->alive = 0;
            effectSpawn(e->x, e->y, FX_BIG);
            gameAddScore(e->score);
        }
    }
}

//---------------------------------------------------------------------------------
// 移動パターン
//---------------------------------------------------------------------------------
static void move(Enemy *e)
{
    switch (e->pattern) {
    case PAT_STRAIGHT:
        e->x += e->vx;
        e->y += e->vy;
        break;

    case PAT_SINE:                       // 左右に振れながら降下
        e->y += e->vy;
        e->x  = e->baseX + ((sinLerp((s16)(e->t * 420)) * FX(46)) >> 12);
        break;

    case PAT_ARC:                        // 弧を描いて画面外へ抜けていく
        e->x += e->vx;
        e->y += e->vy;
        e->vy -= 7;
        break;

    case PAT_HOVER:                      // 降りてきて留まり、撃ってから離脱
        if (e->t < 60) {
            e->x += e->vx;
            e->y += e->vy;
        } else if (e->t < 260) {
            e->x = e->baseX + ((sinLerp((s16)(e->t * 260)) * FX(30)) >> 12);
        } else {
            e->y += FX(3);
        }
        break;
    }
}

//---------------------------------------------------------------------------------
static void shoot(Enemy *e)
{
    if (--e->fireTimer > 0) return;

    // 画面外や画面下からは撃たない
    s32 py = fx2i(e->y);
    if (py < 0 || py > SCR_H - 40) {
        e->fireTimer = 30;
        return;
    }

    if (e->kind == EK_INTERCEPTOR) {
        shotFireAimed(e->x, e->y + FX(8), FX(3), 1);
        e->fireTimer = 70 + rndRange(50);
    } else {
        shotFireEnemy(e->x, e->y + FX(8), 0, FX(2) + 64, 3);
        e->fireTimer = 100 + rndRange(80);
    }
}

void enemyUpdate(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &enemy[i];
        if (!e->alive) continue;

        move(e);
        e->t++;
        shoot(e);

        // 画面外に出たら消す(上は少し猶予をとる: 出現直後に消えないように)
        s32 x = fx2i(e->x), y = fx2i(e->y);
        if (y > SCR_H + 24 || y < -80 || x < -40 || x > SCR_W + 40) {
            e->alive = 0;
            continue;
        }

        // 体当たり
        if (player.alive && player.inv == 0 &&
            hits(e->x, e->y, ENEMY_HIT, ENEMY_HIT,
                 player.x, player.y, PLAYER_HIT, PLAYER_HIT)) {
            e->alive = 0;
            effectSpawn(e->x, e->y, FX_BIG);
            playerDamage();
        }
    }
}

void enemyRender(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        if (!enemy[i].alive) { oamClearSprite(&oamMain, OAM_ENEMY + i); continue; }
        oamSet(&oamMain, OAM_ENEMY + i,
               fx2i(enemy[i].x) - 8, fx2i(enemy[i].y) - 8,
               1, PB_TIE, SpriteSize_16x16, SpriteColorFormat_16Color,
               gfxTie[enemy[i].kind], -1, false, false, false, false, false);
    }
}
