//---------------------------------------------------------------------------------
// opening.c -- プロローグ -> オープニングクロール -> タイトル画面
//
// クロールは擬似 3D(いわゆる mode 7)。文字を貼った平面を床に見立て、回転BG の
// 変形行列をスキャンラインごとに差し替えて奥へ流す。
//
//   d      = y - H0                 その行から地平線までの距離(H0 は画面外の上)
//   PA     = CAM_H / d              1 画面ピクセルあたりのテクスチャ幅
//   texRow = BASE + scroll - K / d  その行が映すテクスチャの行 (K = CAM_H * FOCAL)
//
//        y=0   ~~~~~~~~~~~~   地平線の少し下。文字は潰れて消える
//                 ------
//                 ------
//   y=191   ==============    手前。文字が一番大きい
//
// scroll を増やすと texRow が全体に増え、文字が奥へ吸い込まれていく。
// d が小さい(画面上部)ほど PA が大きく = 文字が小さくなり、遠近感が出る。
//
// 遠くの文字はドットが潰れて汚いので、行ごとのアルファ値で星空に溶かして消す。
//---------------------------------------------------------------------------------
#include "game.h"

//---------------------------------------------------------------------------------
// クロールの見た目を決める定数
//---------------------------------------------------------------------------------
#define H0        (-20)                    // 地平線(画面より 20px 上)
#define CAM_H     155                      // カメラの高さ(テクスチャ px)
#define FOCAL     200                      // 焦点距離(画面 px)
#define K         (CAM_H * FOCAL)
#define BASE      (K / (SCR_H - 1 - H0))   // scroll=0 で画面最下段がテクスチャ 0 行目

#define FADE_HI   120     // これより下は文字がはっきり見える
#define FADE_LO    50     // これより上は完全に星空に溶ける

#define SPEED     152     // 8.8 固定小数。約 0.6 テクスチャ px / フレーム
#define CRAWL_END FX(780) // 最後の行が消えきる scroll

#define PROLOGUE_HOLD 150 // 「遠い昔…」を出しておくフレーム数
#define PROLOGUE_FADE  30 // そこから消えるまで

#define SKIP_KEYS (KEY_START | KEY_A | KEY_TOUCH)
#define SKIP_LOCK  10     // 押しっぱなしで素通りしないための無視フレーム

#define COL_CRAWL RGB15(31, 29,  4)   // クロールとタイトルの黄色
#define COL_INTRO RGB15( 9, 14, 31)   // プロローグの青

#define LOGO_Y     34     // ロゴスプライトの上端
#define ROW_SUB    15     // タイトルの副題
#define ROW_START  18     // "PRESS START"
#define ROW_HINT   21     // 操作の案内

//---------------------------------------------------------------------------------
// クロール本文。"" は行送りだけの空行。1 行 22 文字まで(手前で画面幅に収まる幅)
//---------------------------------------------------------------------------------
static const char *const crawlText[] = {
    "EPISODE  I",
    "",
    "REBEL  ASSAULT",
    "",
    "IT IS A PERIOD OF",
    "CIVIL WAR.  REBEL",
    "STARFIGHTERS HAVE WON",
    "THEIR FIRST VICTORY",
    "AGAINST THE EMPIRE.",
    "",
    "IMPERIAL PATROLS NOW",
    "SWARM THE SECTOR,",
    "HUNTING THE LAST OF",
    "THE REBEL PILOTS.",
    "",
    "PURSUED BY A SINISTER",
    "TIE ADVANCED, YOU RACE",
    "ABOARD YOUR X-WING",
    "WITH THE STOLEN PLANS",
    "THAT CAN RESTORE",
    "FREEDOM TO THE GALAXY.",
};
#define CRAWL_LINES ((int)(sizeof(crawlText) / sizeof(crawlText[0])))

//---------------------------------------------------------------------------------
// 行ごとの変形パラメータ。scroll 以外は動かないので初期化時に一度作るだけ。
//---------------------------------------------------------------------------------
static s16 tabPa[SCR_H];
static s32 tabX[SCR_H];
static s32 tabY[SCR_H];      // texRow の scroll を含まない部分
static u16 tabAlpha[SCR_H];

static s32 scroll;           // 8.8 固定小数
static int phase;            // 0: プロローグ 1: クロール
static int t;                // 現在のフェーズに入ってからのフレーム数
static int blinkShown;

static void buildTables(void)
{
    int y;
    for (y = 0; y < SCR_H; y++) {
        int d  = y - H0;
        int pa = (CAM_H << 8) / d;
        int a  = (y >= FADE_HI) ? 16
               : (y <= FADE_LO) ? 0
               : 16 * (y - FADE_LO) / (FADE_HI - FADE_LO);

        tabPa[y]    = (s16)pa;
        tabX[y]     = (TEXT_CENTER_X << 8) - pa * (SCR_W / 2);
        tabY[y]     = (BASE << 8) - ((K << 8) / d);
        tabAlpha[y] = (u16)(a | ((16 - a) << 8));   // EVA=文字 / EVB=星空
    }
}

//---------------------------------------------------------------------------------
// HBlank 割り込み。次の行ぶんのパラメータを積む。
// VBlank 中も呼ばれるので、そこでは次フレームの 0 行目を用意しておく。
//---------------------------------------------------------------------------------
static void crawlHBlank(void)
{
    int line = REG_VCOUNT + 1;
    if (line >= SCR_H) line = 0;

    REG_BG2PA    = tabPa[line];
    REG_BG2X     = tabX[line];
    REG_BG2Y     = tabY[line] + scroll;
    REG_BLDALPHA = tabAlpha[line];
}

//---------------------------------------------------------------------------------
// 文字マップの用意
//---------------------------------------------------------------------------------
static void buildCrawlMap(void)
{
    textClear();
    int row = 2, i;      // タイル行。先頭に少し余白を置く
    for (i = 0; i < CRAWL_LINES; i++) {
        if (crawlText[i][0] == '\0') { row += 2; continue; }
        textPutCenter(row, crawlText[i]);
        row += 3;        // 1 行 24px。詰めすぎると奥で潰れて読めなくなる
    }
}

static void buildPrologueMap(void)
{
    textClear();
    textPutCenter(10, "A LONG TIME AGO IN A GALAXY");
    textPutCenter(12, "FAR, FAR AWAY....");
}

static void buildTitleMap(void)
{
    textClear();
    textPutCenter(ROW_SUB,   "R E B E L   A S S A U L T");
    textPutCenter(ROW_START, "PRESS  START");
    textPutCenter(ROW_HINT,  "OR  TOUCH  THE  SCREEN");
    blinkShown = -1;
}

//---------------------------------------------------------------------------------
// 演出中はゲームのスプライトを出さない
//---------------------------------------------------------------------------------
static void clearWorld(void)
{
    player.alive = 0;
    boss.alive   = 0;
    shotReset();
    enemyReset();
    effectReset();
}

//---------------------------------------------------------------------------------
void openingEnter(void)
{
    gameSetState(ST_CRAWL);
    clearWorld();

    buildTables();

    phase = 0;
    t     = 0;
    buildPrologueMap();
    textSetColor(COL_INTRO);
    textIdentity(TEXT_CENTER_X - SCR_W / 2, 0);
    textShow(1);

    // 文字を星空に溶かすためのアルファ合成。クロールでは行ごとに EVA を変える。
    REG_BLDCNT   = BLEND_ALPHA | BLEND_SRC_BG2 |
                   BLEND_DST_BG0 | BLEND_DST_BG1 | BLEND_DST_BACKDROP;
    REG_BLDALPHA = 16 | (0 << 8);
}

static void startCrawl(void)
{
    phase  = 1;
    t      = 0;
    scroll = 0;
    buildCrawlMap();
    textSetColor(COL_CRAWL);

    // 行ごとに BG2X / BG2Y を直接書くので、走査線をまたぐ自動増分(PB / PD)は
    // 止めておく。PC = 0 は「1 走査線が映すのはテクスチャの 1 行だけ」の意味。
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = 0;

    irqSet(IRQ_HBLANK, crawlHBlank);
    irqEnable(IRQ_HBLANK);
}

void titleEnter(void)
{
    irqDisable(IRQ_HBLANK);
    REG_BLDCNT = 0;

    gameSetState(ST_TITLE);
    clearWorld();

    buildTitleMap();
    textSetColor(COL_CRAWL);
    textIdentity(TEXT_CENTER_X - SCR_W / 2, 0);
    textShow(1);
}

//---------------------------------------------------------------------------------
void openingUpdate(void)
{
    int skip = (keysDown() & SKIP_KEYS) != 0;
    t++;

    if (g.state == ST_TITLE) {
        if (g.timer > SKIP_LOCK && skip) gameStart();
        return;
    }

    // START / 画面タップでいつでもタイトルへ飛ばせる
    if (g.timer > SKIP_LOCK && skip) { titleEnter(); return; }

    if (phase == 0) {
        if (t >= PROLOGUE_HOLD + PROLOGUE_FADE) startCrawl();
    } else {
        scroll += SPEED;
        if (scroll >= CRAWL_END) titleEnter();
    }
}

//---------------------------------------------------------------------------------
static void blinkPressStart(void)
{
    int on = ((g.timer / 24) & 1) == 0;
    if (on == blinkShown) return;
    blinkShown = on;
    textPutCenter(ROW_START, on ? "PRESS  START" : "            ");
}

void openingRender(void)
{
    int i;

    if (g.state == ST_TITLE) {
        for (i = 0; i < LOGO_FRAMES; i++)
            oamSet(&oamMain, OAM_LOGO + i, i * 64, LOGO_Y, 0, PB_LOGO,
                   SpriteSize_64x64, SpriteColorFormat_16Color, gfxLogo[i],
                   -1, false, false, false, false, false);
        blinkPressStart();
        return;
    }

    for (i = 0; i < LOGO_FRAMES; i++)
        oamClearSprite(&oamMain, OAM_LOGO + i);

    // プロローグは画面全体を一様に薄くして消す(クロールは HBlank 側で行ごと)
    if (g.state == ST_CRAWL && phase == 0) {
        int a = 16;
        if (t > PROLOGUE_HOLD) a = 16 - 16 * (t - PROLOGUE_HOLD) / PROLOGUE_FADE;
        if (a < 0) a = 0;
        REG_BLDALPHA = (u16)(a | ((16 - a) << 8));
    }
}
