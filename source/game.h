//---------------------------------------------------------------------------------
// game.h -- ゲーム全体で共有する型・定数・各モジュールの入口
//---------------------------------------------------------------------------------
#ifndef GAME_H
#define GAME_H

#include <nds.h>

//---------------------------------------------------------------------------------
// 固定小数(8.8)
//   座標と速度はすべてこの形式。表示するときだけ fx2i() で整数ピクセルに落とす。
//---------------------------------------------------------------------------------
typedef s32 fx;
#define FXS       8
#define FX(n)     ((fx)((n) * (1 << FXS)))
#define i2fx(n)   ((fx)((n) << FXS))
#define fx2i(v)   ((s32)(v) >> FXS)

#define SCR_W 256
#define SCR_H 192

//---------------------------------------------------------------------------------
// OAM(スプライト)ID の割り当て
//   動的に確保せず、種類ごとに固定の範囲を割り当てる。合計 98 / 128。
//---------------------------------------------------------------------------------
#define OAM_PLAYER  0
#define OAM_BOSS    1
#define MAX_PSHOT   24
#define OAM_PSHOT   2
#define MAX_ENEMY   16
#define OAM_ENEMY   (OAM_PSHOT + MAX_PSHOT)
#define MAX_ESHOT   40
#define OAM_ESHOT   (OAM_ENEMY + MAX_ENEMY)
#define MAX_EFFECT  16
#define OAM_EFFECT  (OAM_ESHOT + MAX_ESHOT)
#define LOGO_FRAMES 4
#define OAM_LOGO    (OAM_EFFECT + MAX_EFFECT)

// スプライトパレットのバンク番号(16色 x 16バンク)
enum { PB_XWING = 0, PB_TIE, PB_BOSS, PB_SHOT, PB_BLAST, PB_LOGO };

//---------------------------------------------------------------------------------
// ゲーム状態
//---------------------------------------------------------------------------------
typedef enum {
    ST_CRAWL,      // オープニング(プロローグ -> クロール)
    ST_TITLE,      // タイトル
    ST_PLAY,       // 通常ウェーブ
    ST_WARNING,    // ボス出現警告
    ST_BOSS,       // ボス戦
    ST_CLEAR,      // クリア
    ST_GAMEOVER
} GameState;

typedef struct {
    GameState state;
    int  timer;      // 現在の状態に入ってからのフレーム数
    int  score;
    int  hiScore;
    int  flash;      // 画面フラッシュ(ボム・被弾)の残フレーム
    int  shake;      // 画面揺れの残フレーム
    int  paused;     // プレイ中に START で一時停止
    u32  rng;
} Game;

extern Game g;

void gameStart(void);   // タイトル -> プレイ開始
void gameAddScore(int n);
void gameSetState(GameState s);

// 疑似乱数(xorshift)
u32 rnd(void);
static inline int rndRange(int n) { return (int)(rnd() % (u32)n); }

//---------------------------------------------------------------------------------
// 当たり判定: 中心座標どうしの AABB
//---------------------------------------------------------------------------------
static inline int hits(fx ax, fx ay, int aw, int ah, fx bx, fx by, int bw, int bh)
{
    s32 dx = fx2i(ax) - fx2i(bx);
    s32 dy = fx2i(ay) - fx2i(by);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx * 2 < aw + bw) && (dy * 2 < ah + bh);
}

//---------------------------------------------------------------------------------
// video.c -- VRAM / OAM / パレットの初期化とスプライト画像の確保
//---------------------------------------------------------------------------------
extern u16 *gfxXwing[3];   // 0:通常 1:左バンク 2:右バンク
extern u16 *gfxTie[2];     // 0:TIEファイター 1:TIEインターセプター
extern u16 *gfxBoss;
extern u16 *gfxShot[4];    // 0:自機赤 1:敵緑 2:自機青(強化) 3:敵球
extern u16 *gfxBlast[4];   // 爆発アニメ 4コマ
extern u16 *gfxLogo[LOGO_FRAMES];  // タイトルロゴを横に 4 分割したもの

void gfxInit(void);

//---------------------------------------------------------------------------------
// starfield.c -- 視差スクロールする星空 BG
//---------------------------------------------------------------------------------
void starInit(void);
void starUpdate(int speed);   // speed は 8.8 固定小数のスクロール量
void starApply(void);

//---------------------------------------------------------------------------------
// textbg.c -- BG2(回転BG)の文字レイヤ
//   オープニングとタイトルが共有する。マップは 64x64 タイル(= 512px 四方)。
//---------------------------------------------------------------------------------
#define TEXT_COLS     64
#define TEXT_ROWS     64
#define TEXT_CENTER_X (TEXT_COLS * 8 / 2)   // マップ中央の x(テクスチャ px)

void textInit(void);
void textClear(void);
void textPut(int col, int row, const char *s);
void textPutCenter(int row, const char *s);   // マップ中央に揃えて置く
void textSetColor(u16 color);
void textIdentity(int ox, int oy);            // 等倍・回転なしで表示する
void textShow(int on);

//---------------------------------------------------------------------------------
// opening.c -- プロローグ -> オープニングクロール -> タイトル画面
//---------------------------------------------------------------------------------
void openingEnter(void);   // ST_CRAWL へ(プロローグから始める)
void titleEnter(void);     // ST_TITLE へ
void openingUpdate(void);
void openingRender(void);

//---------------------------------------------------------------------------------
// player.c -- 自機 X-wing
//---------------------------------------------------------------------------------
typedef struct {
    fx  x, y;
    int alive;
    int lives;
    int bombs;
    int inv;         // 無敵の残フレーム(被弾直後)
    int fireTimer;
    int power;       // 1..3 ショットの強化段階
    int bank;        // -1:左 0:水平 1:右
    int deadTimer;   // 撃墜後の復帰待ち
} Player;

extern Player player;

#define PLAYER_W 32
#define PLAYER_H 32
#define PLAYER_HIT 8     // 実際の当たり判定は見た目より小さくする

void playerReset(int fullRestart);
void playerUpdate(void);
void playerRender(void);
void playerDamage(void);

//---------------------------------------------------------------------------------
// shot.c -- 自機弾 / 敵弾(オブジェクトプール)
//---------------------------------------------------------------------------------
typedef struct {
    fx  x, y, vx, vy;
    u8  alive;
    u8  kind;     // gfxShot[] のインデックス
    u8  w, h;     // 当たり判定サイズ
} Shot;

extern Shot pshot[MAX_PSHOT];
extern Shot eshot[MAX_ESHOT];

void shotReset(void);
void shotUpdate(void);
void shotRender(void);
void shotFirePlayer(fx x, fx y, fx vx, fx vy, int kind);
void shotFireEnemy(fx x, fx y, fx vx, fx vy, int kind);
void shotFireAimed(fx x, fx y, fx speed, int kind);   // 自機を狙って撃つ
void shotClearEnemy(void);                            // ボムで敵弾を消す

//---------------------------------------------------------------------------------
// enemy.c -- 敵機 TIE
//---------------------------------------------------------------------------------
enum { EK_FIGHTER = 0, EK_INTERCEPTOR = 1 };
enum { PAT_STRAIGHT = 0, PAT_SINE, PAT_ARC, PAT_HOVER };

typedef struct {
    fx  x, y, vx, vy;
    fx  baseX;
    u8  alive, kind, pattern;
    s16 hp, t, fireTimer, score;
} Enemy;

extern Enemy enemy[MAX_ENEMY];

void   enemyReset(void);
void   enemyUpdate(void);
void   enemyRender(void);
Enemy *enemySpawn(int kind, int pattern, fx x, fx y, fx vx, fx vy);
int    enemyAlive(void);
void   enemyDamageAll(int dmg);   // ボム

//---------------------------------------------------------------------------------
// boss.c -- ボス TIE Advanced
//---------------------------------------------------------------------------------
typedef struct {
    fx  x, y, vx;
    int alive;
    int hp, maxHp;
    int phase;
    int t;
    int fireTimer;
    int summonTimer;
    int hurt;        // 被弾フラッシュ
    int dying;       // 撃破演出の残フレーム
} Boss;

extern Boss boss;

#define BOSS_W 64
#define BOSS_H 64

void bossStart(void);
void bossUpdate(void);
void bossRender(void);
void bossDamage(int dmg);

//---------------------------------------------------------------------------------
// effect.c -- 爆発
//---------------------------------------------------------------------------------
enum { FX_SMALL = 0, FX_BIG = 1 };

void effectReset(void);
void effectSpawn(fx x, fx y, int big);
void effectUpdate(void);
void effectRender(void);

//---------------------------------------------------------------------------------
// wave.c -- 敵の出現スケジュール
//---------------------------------------------------------------------------------
void waveReset(void);
void waveUpdate(void);
int  waveDone(void);      // 全ウェーブを出し終えたか
int  waveNumber(void);
int  waveTotal(void);

//---------------------------------------------------------------------------------
// hud.c -- 下画面の情報表示
//---------------------------------------------------------------------------------
void hudInit(void);
void hudDraw(void);

#endif // GAME_H
