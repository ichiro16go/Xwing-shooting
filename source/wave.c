//---------------------------------------------------------------------------------
// wave.c -- 敵の出現スケジュール
//
// 「いつ・どこから・何機・どう動くか」を表で持つ。ゲーム本体はこの表を
// 読むだけなので、難易度や構成を変えるときはここだけ触ればよい。
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

typedef struct {
    s16 start;      // ウェーブ開始からのフレーム
    u8  wave;       // 表示用のウェーブ番号
    u8  kind;       // EK_FIGHTER / EK_INTERCEPTOR
    u8  pattern;    // PAT_*
    u8  count;      // 何機出すか
    s16 interval;   // 1 機ごとの間隔(フレーム)
    s16 x, dx;      // 1 機目の出現 x と、機ごとの増分
    s16 y;          // 出現 y
    fx  vx, vy;
} Formation;

//---------------------------------------------------------------------------------
static const Formation stage[] = {
    // --- ウェーブ 1: まっすぐ降りてくる TIE ファイター ---
    {   90, 1, EK_FIGHTER,     PAT_STRAIGHT, 5, 26,  40,  44, -20,      0, FX(2)     },
    {  330, 1, EK_FIGHTER,     PAT_STRAIGHT, 5, 26, 216, -44, -20,      0, FX(2)     },
    {  560, 1, EK_FIGHTER,     PAT_SINE,     4, 34, 128,   0, -20,      0, FX(1)+128 },

    // --- ウェーブ 2: 左右から弧を描いて突っ込んでくる ---
    {  820, 2, EK_INTERCEPTOR, PAT_ARC,      4, 22, -20,   0,  30,  FX(3), FX(2)     },
    {  980, 2, EK_INTERCEPTOR, PAT_ARC,      4, 22, 276,   0,  30, -FX(3), FX(2)     },
    { 1160, 2, EK_FIGHTER,     PAT_SINE,     6, 24,  70,  24, -20,      0, FX(2)     },

    // --- ウェーブ 3: 居座って撃ってくる編隊 ---
    { 1450, 3, EK_FIGHTER,     PAT_HOVER,    4, 30,  50,  52, -20,      0, FX(2)     },
    { 1680, 3, EK_INTERCEPTOR, PAT_STRAIGHT, 6, 18, 128,   0, -20,      0, FX(3)     },
    { 1900, 3, EK_INTERCEPTOR, PAT_ARC,      5, 18, -20,   0,  60,  FX(4), FX(1)     },
    { 2060, 3, EK_INTERCEPTOR, PAT_ARC,      5, 18, 276,   0,  60, -FX(4), FX(1)     },

    // --- ウェーブ 4: 総攻撃 ---
    { 2320, 4, EK_FIGHTER,     PAT_SINE,     6, 20,  40,  36, -20,      0, FX(2)     },
    { 2360, 4, EK_INTERCEPTOR, PAT_HOVER,    4, 40, 200, -48, -20,      0, FX(2)+128 },
    { 2620, 4, EK_INTERCEPTOR, PAT_STRAIGHT, 8, 14,  24,  30, -20,      0, FX(3)+64  },
    { 2820, 4, EK_FIGHTER,     PAT_HOVER,    5, 26, 128,   0, -20,      0, FX(2)     },
};

#define FORMATIONS ((int)(sizeof(stage) / sizeof(stage[0])))

static int emitted[FORMATIONS];
static int timer;
static int curWave;

void waveReset(void)
{
    memset(emitted, 0, sizeof(emitted));
    timer = 0;
    curWave = 1;
}

void waveUpdate(void)
{
    timer++;

    int i;
    for (i = 0; i < FORMATIONS; i++) {
        const Formation *f = &stage[i];
        if (emitted[i] >= f->count) continue;
        if (timer < f->start + emitted[i] * f->interval) continue;

        enemySpawn(f->kind, f->pattern,
                   FX(f->x + f->dx * emitted[i]), FX(f->y),
                   f->vx, f->vy);
        emitted[i]++;
        curWave = f->wave;
    }
}

//---------------------------------------------------------------------------------
// 全編隊を出し切り、画面上の敵もいなくなったらウェーブ終了
//---------------------------------------------------------------------------------
int waveDone(void)
{
    int i;
    for (i = 0; i < FORMATIONS; i++)
        if (emitted[i] < stage[i].count) return 0;
    return enemyAlive() == 0;
}

int waveNumber(void) { return curWave; }
int waveTotal(void)  { return stage[FORMATIONS - 1].wave; }
