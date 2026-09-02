//---------------------------------------------------------------------------------
// main.c -- 初期化と状態遷移、メインループ
//---------------------------------------------------------------------------------
#include "game.h"

Game g;

//---------------------------------------------------------------------------------
// 疑似乱数 (xorshift32)
//---------------------------------------------------------------------------------
u32 rnd(void)
{
    g.rng ^= g.rng << 13;
    g.rng ^= g.rng >> 17;
    g.rng ^= g.rng << 5;
    return g.rng;
}

void gameAddScore(int n)
{
    g.score += n;
    if (g.score > g.hiScore) g.hiScore = g.score;
}

//---------------------------------------------------------------------------------
static void setState(GameState s)
{
    g.state = s;
    g.timer = 0;
}

void gameStart(void)
{
    g.score = 0;
    g.flash = 0;
    g.shake = 0;
    g.paused = 0;

    playerReset(1);
    shotReset();
    enemyReset();
    effectReset();
    waveReset();
    boss.alive = 0;

    setState(ST_PLAY);
}

//---------------------------------------------------------------------------------
// 実際に動いているもの全部の更新。状態によらず共通。
//---------------------------------------------------------------------------------
static void updateWorld(void)
{
    playerUpdate();
    enemyUpdate();
    shotUpdate();
    effectUpdate();
}

// プレイ中の状態か(ポーズを受け付ける状態か)
static int inPlay(void)
{
    return g.state == ST_PLAY || g.state == ST_WARNING || g.state == ST_BOSS;
}

static void update(void)
{
    int down = keysDown();

    // START で一時停止 / 再開
    if (inPlay()) {
        if (down & KEY_START) g.paused = !g.paused;
        if (g.paused) { g.timer++; return; }
    }

    switch (g.state) {
    case ST_TITLE:
        // 念のため数フレームは入力を受けない
        if (g.timer > 15 && (down & KEY_START)) gameStart();
        break;

    case ST_PLAY:
        waveUpdate();
        updateWorld();
        if (waveDone()) setState(ST_WARNING);
        // デバッグ用: ボス戦をすぐ確認したいとき
        if (down & KEY_SELECT) setState(ST_WARNING);
        break;

    case ST_WARNING:
        updateWorld();
        if (g.timer > 180) {
            bossStart();
            setState(ST_BOSS);
        }
        break;

    case ST_BOSS:
        updateWorld();
        bossUpdate();
        break;

    case ST_CLEAR:
        updateWorld();
        if (g.timer > 90) {
            if (down & KEY_START)  gameStart();      // もう一度遊ぶ
            if (down & KEY_SELECT) setState(ST_TITLE);
        }
        break;

    case ST_GAMEOVER:
        updateWorld();
        if (g.timer > 60) {
            if (down & KEY_START)  gameStart();      // その場でリスタート
            if (down & KEY_SELECT) setState(ST_TITLE);
        }
        break;
    }

    g.timer++;
    if (g.flash > 0) g.flash--;
    if (g.shake > 0) g.shake--;

    // 星の流れる速さ。ボス戦では止めて緊張感を出す。
    starUpdate((g.state == ST_BOSS) ? FX(1) : FX(2));
}

//---------------------------------------------------------------------------------
static void render(void)
{
    playerRender();
    enemyRender();
    shotRender();
    effectRender();
    bossRender();
    starApply();

    // 被弾やボムのときに画面を白く飛ばす
    setBrightness(1, g.flash > 0 ? (g.flash > 12 ? 12 : g.flash) : 0);
}

//---------------------------------------------------------------------------------
int main(void)
{
    irqEnable(IRQ_VBLANK);

    // starInit() が星の配置に rnd() を使うので、種は必ずその前に入れる
    // (xorshift は状態 0 から抜け出せず、0 のままだと永久に 0 を返す)
    g.rng = 0x1234567;

    gfxInit();
    starInit();
    hudInit();

    g.hiScore = 20000;
    playerReset(1);
    shotReset();
    enemyReset();
    effectReset();
    waveReset();
    setState(ST_TITLE);

    // 起動直後の 1 回目の scanKeys() は、キー入力レジスタが確定する前の値を
    // 拾って「全キーが押された」と報告することがある。空読みしておく。
    scanKeys();
    scanKeys();

    while (pmMainLoop()) {
        scanKeys();

        update();
        render();
        hudDraw();

        swiWaitForVBlank();
        oamUpdate(&oamMain);
        bgUpdate();
    }
    return 0;
}
