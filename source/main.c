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
void gameSetState(GameState s)
{
    g.state = s;
    g.timer = 0;
}

void gameStart(void)
{
    textShow(0);          // オープニングの文字レイヤを片付ける

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

    gameSetState(ST_PLAY);
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
    case ST_CRAWL:
    case ST_TITLE:
        openingUpdate();
        break;

    case ST_PLAY:
        waveUpdate();
        updateWorld();
        if (waveDone()) gameSetState(ST_WARNING);
        // デバッグ用: ボス戦をすぐ確認したいとき
        if (down & KEY_SELECT) gameSetState(ST_WARNING);
        break;

    case ST_WARNING:
        updateWorld();
        if (g.timer > 180) {
            bossStart();
            gameSetState(ST_BOSS);
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
            if (down & KEY_SELECT) titleEnter();
        }
        break;

    case ST_GAMEOVER:
        updateWorld();
        if (g.timer > 60) {
            if (down & KEY_START)  gameStart();      // その場でリスタート
            if (down & KEY_SELECT) titleEnter();
        }
        break;
    }

    g.timer++;
    if (g.flash > 0) g.flash--;
    if (g.shake > 0) g.shake--;

    // 星の流れる速さ。ボス戦では落として緊張感を出し、
    // オープニングでは静止した宇宙に見えるくらいまで遅くする。
    int speed = FX(2);
    if (g.state == ST_BOSS)                              speed = FX(1);
    else if (g.state == ST_CRAWL || g.state == ST_TITLE) speed = FX(1) / 8;
    starUpdate(speed);
}

//---------------------------------------------------------------------------------
static void render(void)
{
    playerRender();
    enemyRender();
    shotRender();
    effectRender();
    bossRender();
    openingRender();     // ST_CRAWL / ST_TITLE 以外では何も出さない
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
    textInit();
    hudInit();

    g.hiScore = 20000;
    playerReset(1);
    shotReset();
    enemyReset();
    effectReset();
    waveReset();
    openingEnter();

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
