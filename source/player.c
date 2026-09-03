//---------------------------------------------------------------------------------
// player.c -- 自機 X-wing
//---------------------------------------------------------------------------------
#include "game.h"

Player player;

// 敵の狙い撃ちより速くしておくこと。自機より速い誘導弾は原理的に避けられない。
#define SPD        (FX(3))           // 3.0 px / frame
#define MARGIN_X   16
#define MARGIN_TOP 20
#define MARGIN_BOT 12

void playerReset(int fullRestart)
{
    player.x = FX(SCR_W / 2);
    player.y = FX(SCR_H - 40);
    player.alive = 1;
    player.inv = 120;
    player.fireTimer = 0;
    player.bank = 0;
    player.deadTimer = 0;
    if (fullRestart) {
        player.lives = 3;
        player.bombs = 3;
        player.power = 1;
    }
}

//---------------------------------------------------------------------------------
// ショット。power に応じて弾数と種類が変わる。
//---------------------------------------------------------------------------------
static void fire(void)
{
    static const int rate[4] = { 8, 7, 6, 5 };
    int p = player.power;
    if (p > 3) p = 3;

    if (--player.fireTimer > 0) return;
    player.fireTimer = rate[p];

    // 翼端の 4 門から。左右 2 門は常に、強化すると外側 2 門も撃つ。
    shotFirePlayer(player.x - FX(9), player.y - FX(10), 0, -FX(7), 0);
    shotFirePlayer(player.x + FX(9), player.y - FX(10), 0, -FX(7), 0);

    if (p >= 2) {
        // 中央に太い青ショット
        shotFirePlayer(player.x, player.y - FX(14), 0, -FX(8), 2);
    }
    if (p >= 3) {
        // 外側に少し開いた弾
        shotFirePlayer(player.x - FX(14), player.y - FX(4), -FX(1), -FX(6), 0);
        shotFirePlayer(player.x + FX(14), player.y - FX(4),  FX(1), -FX(6), 0);
    }
}

//---------------------------------------------------------------------------------
// ボム: 敵弾を全部消し、画面上の敵に大ダメージ
//---------------------------------------------------------------------------------
static void bomb(void)
{
    if (player.bombs <= 0) return;
    player.bombs--;

    shotClearEnemy();
    enemyDamageAll(8);
    if (boss.alive) bossDamage(60);

    // 弾を消した直後に撃ち返されると緊急回避にならないので、少しだけ無敵にする
    if (player.inv < 90) player.inv = 90;

    g.flash = 14;
    g.shake = 20;

    // 画面全体に爆発をばらまく
    int i;
    for (i = 0; i < 10; i++)
        effectSpawn(FX(rndRange(SCR_W)), FX(rndRange(SCR_H - 40)), i & 1);
}

void playerUpdate(void)
{
    // 撃墜中は復帰待ち。
    // deadTimer が 0 に「なった瞬間」だけ遷移させる。ここを毎フレーム
    // 実行してしまうと g.timer が 0 に戻り続け、ゲームオーバー画面での
    // 入力受付(g.timer による待ち時間)が永久に成立しなくなる。
    if (!player.alive) {
        if (player.deadTimer > 0 && --player.deadTimer == 0) {
            if (player.lives > 0) {
                playerReset(0);
            } else {
                g.state = ST_GAMEOVER;
                g.timer = 0;
            }
        }
        return;
    }

    int held = keysHeld();
    int down = keysDown();

    fx dx = 0, dy = 0;
    if (held & KEY_LEFT)  dx -= SPD;
    if (held & KEY_RIGHT) dx += SPD;
    if (held & KEY_UP)    dy -= SPD;
    if (held & KEY_DOWN)  dy += SPD;

    // 斜め移動が速くならないようにする
    if (dx && dy) { dx = dx * 3 / 4; dy = dy * 3 / 4; }

    player.x += dx;
    player.y += dy;

    if (player.x < FX(MARGIN_X))          player.x = FX(MARGIN_X);
    if (player.x > FX(SCR_W - MARGIN_X))  player.x = FX(SCR_W - MARGIN_X);
    if (player.y < FX(MARGIN_TOP))        player.y = FX(MARGIN_TOP);
    if (player.y > FX(SCR_H - MARGIN_BOT))player.y = FX(SCR_H - MARGIN_BOT);

    player.bank = (dx < 0) ? -1 : (dx > 0) ? 1 : 0;

    fire();
    if (down & KEY_A) bomb();

    if (player.inv > 0) player.inv--;

    // スコアに応じてショットが強化される。
    // 雑魚を全滅させても 10,700 点にしかならない(wave.c の stage[] の合計)ので、
    // しきい値はそれよりずっと手前に置くこと。ボス戦は LV.3 で戦う想定。
    if (g.score >= 8000)      player.power = 3;
    else if (g.score >= 3000) player.power = 2;
    else                      player.power = 1;
}

void playerDamage(void)
{
    if (!player.alive || player.inv > 0) return;

    player.alive = 0;
    player.lives--;
    player.deadTimer = 90;

    // 復帰位置に弾が残っていると連続で落とされるので、被弾時に敵弾を一掃する
    shotClearEnemy();

    effectSpawn(player.x, player.y, FX_BIG);
    effectSpawn(player.x - FX(8), player.y + FX(6), FX_SMALL);
    effectSpawn(player.x + FX(10), player.y - FX(4), FX_SMALL);
    g.flash = 8;
    g.shake = 24;
}

void playerRender(void)
{
    if (!player.alive) {
        oamClearSprite(&oamMain, OAM_PLAYER);
        return;
    }
    // 無敵中は点滅
    if (player.inv > 0 && (player.inv & 2)) {
        oamClearSprite(&oamMain, OAM_PLAYER);
        return;
    }

    int frame = (player.bank < 0) ? 1 : (player.bank > 0) ? 2 : 0;
    oamSet(&oamMain, OAM_PLAYER,
           fx2i(player.x) - PLAYER_W / 2, fx2i(player.y) - PLAYER_H / 2,
           0, PB_XWING,
           SpriteSize_32x32, SpriteColorFormat_16Color, gfxXwing[frame],
           -1, false, false, false, false, false);
}
