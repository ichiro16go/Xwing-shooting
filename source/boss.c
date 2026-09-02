//---------------------------------------------------------------------------------
// boss.c -- ボス TIE Advanced
//
// HP の残量で 3 つのフェーズに移る。フェーズが進むほど動きが速く、
// 弾幕が厚くなり、最終フェーズでは護衛の TIE を呼ぶ。
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

Boss boss;

#define BOSS_MAX_HP 420
#define BOSS_TOP    FX(46)
#define BOSS_LEFT   FX(44)
#define BOSS_RIGHT  FX(SCR_W - 44)

void bossStart(void)
{
    memset(&boss, 0, sizeof(boss));
    boss.alive  = 1;
    boss.hp     = BOSS_MAX_HP;
    boss.maxHp  = BOSS_MAX_HP;
    boss.x      = FX(SCR_W / 2);
    boss.y      = FX(-70);        // 画面外から降りてくる
    boss.vx     = FX(1);
    boss.phase  = 0;
    boss.fireTimer   = 120;
    boss.summonTimer = 300;
}

void bossDamage(int dmg)
{
    if (!boss.alive || boss.dying) return;
    boss.hp -= dmg;
    boss.hurt = 3;
    if (boss.hp <= 0) {
        boss.hp = 0;
        boss.dying = 150;
        shotClearEnemy();
        g.shake = 60;
    }
}

//---------------------------------------------------------------------------------
// 攻撃パターン
//---------------------------------------------------------------------------------
static void spread(int ways, fx speed, int kind)
{
    // 真下を中心に扇状に撃つ
    int i;
    // libnds の角度は 1 周 = 32768。画面は下が +y なので sin が正の向き、
    // すなわち 0x2000 (90度) が「真下」になる。
    int span = 0x0600;                           // 弾どうしの開き
    int base = 0x2000 - span * (ways - 1) / 2;
    for (i = 0; i < ways; i++) {
        s16 a = (s16)(base + span * i);
        fx vx = (fx)((cosLerp(a) * speed) >> 12);
        fx vy = (fx)((sinLerp(a) * speed) >> 12);
        shotFireEnemy(boss.x, boss.y + FX(18), vx, vy, kind);
    }
}

static void radial(int ways, fx speed, int kind, int offset)
{
    int i;
    for (i = 0; i < ways; i++) {
        s16 a = (s16)(i * (32768 / ways) + offset);
        fx vx = (fx)((cosLerp(a) * speed) >> 12);
        fx vy = (fx)((sinLerp(a) * speed) >> 12);
        shotFireEnemy(boss.x, boss.y, vx, vy, kind);
    }
}

static void attack(void)
{
    if (--boss.fireTimer > 0) return;

    switch (boss.phase) {
    case 0:
        spread(3, FX(2) + 96, 3);
        boss.fireTimer = 76;
        break;

    case 1:
        if (boss.t % 3 == 0) {
            radial(12, FX(2), 3, boss.t * 97);
            boss.fireTimer = 90;
        } else {
            shotFireAimed(boss.x - FX(20), boss.y + FX(12), FX(3), 1);
            shotFireAimed(boss.x + FX(20), boss.y + FX(12), FX(3), 1);
            boss.fireTimer = 46;
        }
        break;

    default:
        if (boss.t % 4 == 0) {
            radial(16, FX(2) + 64, 3, boss.t * 61);
            boss.fireTimer = 70;
        } else {
            spread(5, FX(3), 1);
            shotFireAimed(boss.x, boss.y + FX(18), FX(4), 1);
            boss.fireTimer = 34;
        }
        break;
    }
    boss.t++;
}

//---------------------------------------------------------------------------------
void bossUpdate(void)
{
    if (!boss.alive) return;

    if (boss.hurt > 0) boss.hurt--;

    // --- 撃破演出 ---
    if (boss.dying > 0) {
        boss.dying--;
        if ((boss.dying & 7) == 0)
            effectSpawn(boss.x + FX(rndRange(56) - 28),
                        boss.y + FX(rndRange(56) - 28), FX_BIG);
        if (boss.dying == 0) {
            boss.alive = 0;
            g.state = ST_CLEAR;
            g.timer = 0;
            gameAddScore(20000);
        }
        return;
    }

    // --- 登場 ---
    if (boss.y < BOSS_TOP) {
        boss.y += FX(1);
        return;
    }

    // --- フェーズ判定 ---
    int pct = boss.hp * 100 / boss.maxHp;
    boss.phase = (pct > 65) ? 0 : (pct > 30) ? 1 : 2;

    // --- 左右移動 ---
    static const fx speedByPhase[3] = { FX(1), FX(1) + 96, FX(2) + 64 };
    fx sp = speedByPhase[boss.phase];
    boss.x += (boss.vx > 0) ? sp : -sp;
    if (boss.x < BOSS_LEFT)  { boss.x = BOSS_LEFT;  boss.vx =  FX(1); }
    if (boss.x > BOSS_RIGHT) { boss.x = BOSS_RIGHT; boss.vx = -FX(1); }

    // フェーズ 2 以降は上下にも揺れる
    if (boss.phase >= 1)
        boss.y = BOSS_TOP + ((sinLerp((s16)(boss.t * 300 + g.timer * 160)) * FX(14)) >> 12);

    attack();

    // --- 最終フェーズは護衛を呼ぶ ---
    if (boss.phase == 2 && --boss.summonTimer <= 0) {
        boss.summonTimer = 300;
        enemySpawn(EK_INTERCEPTOR, PAT_HOVER, FX(60),  FX(-20), 0, FX(2));
        enemySpawn(EK_INTERCEPTOR, PAT_HOVER, FX(196), FX(-20), 0, FX(2));
    }

    // --- 体当たり ---
    if (player.alive && player.inv == 0 &&
        hits(boss.x, boss.y, 44, 48, player.x, player.y, PLAYER_HIT, PLAYER_HIT))
        playerDamage();
}

void bossRender(void)
{
    if (!boss.alive) {
        oamClearSprite(&oamMain, OAM_BOSS);
        return;
    }
    oamSet(&oamMain, OAM_BOSS,
           fx2i(boss.x) - BOSS_W / 2, fx2i(boss.y) - BOSS_H / 2,
           2, PB_BOSS, SpriteSize_64x64, SpriteColorFormat_16Color,
           gfxBoss, -1, false, false, false, false, false);
}
