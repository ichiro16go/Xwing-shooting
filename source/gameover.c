//---------------------------------------------------------------------------------
// gameover.c -- 上画面のゲームオーバー表示
//
// 撃墜されて残機が尽きたことは大きく伝えたいので、文字レイヤ(textbg.c)を
// 拡大して画面いっぱいの "GAME OVER" を出す。
//
// 回転BG の変形行列は画面に 1 つしか持てない(= 画面全体が同じ倍率になる)ので、
// 上画面に出す文字はこの 1 行だけ。リトライの案内は下画面(hud.c)に任せる。
//---------------------------------------------------------------------------------
#include "game.h"

//---------------------------------------------------------------------------------
// 文字マップ上の置き場所。行はどこでもよい(画面中央に来るように映すため)。
//---------------------------------------------------------------------------------
#define GO_TEXT  "GAME OVER"
#define GO_LEN   ((int)sizeof(GO_TEXT) - 1)
#define GO_ROW   30
#define GO_COL   (TEXT_COLS / 2 - GO_LEN / 2)
#define GO_CX    (GO_COL * 8 + GO_LEN * 8 / 2)   // 文字列の中心(テクスチャ px)
#define GO_CY    (GO_ROW * 8 + 4)

//---------------------------------------------------------------------------------
// 見た目
//---------------------------------------------------------------------------------
#define GO_SCALE   FX(3)   // 定常倍率。1 文字 24px、9 文字で 216px = ほぼ画面幅
#define GO_SLAM    FX(4)   // 出現直後の倍率。ここから縮めて叩きつける
                           // (画面幅から少しはみ出すくらい。大きすぎると読めない)
#define SLAM_TIME  16      // GO_SLAM -> GO_SCALE にかかるフレーム数

#define COL_HOT    RGB15(31, 27, 22)   // 叩きつけている間の白熱した色
#define PULSE      48                  // 明滅が 1 往復するフレーム数
#define LIT_MIN    6                   // 一番暗いときの明るさ(0..16)

// 明るさ lit(0..16)の赤。暗くしても真っ黒にはせず、燃え残りのように見せる。
static u16 ember(int lit)
{
    int r = 16 + 15 * lit / 16;
    int g = 6 * lit / 16;
    return RGB15(r, g, g);
}

//---------------------------------------------------------------------------------
void gameoverEnter(void)
{
    gameSetState(ST_GAMEOVER);   // g.timer も 0 に戻る

    textClear();
    textPut(GO_COL, GO_ROW, GO_TEXT);
    textSetColor(COL_HOT);
    textShow(1);

    g.flash = 10;
}

//---------------------------------------------------------------------------------
void gameoverRender(void)
{
    if (g.state != ST_GAMEOVER) return;

    int t = g.timer;

    if (t < SLAM_TIME) {
        // 大きい状態から一気に縮める
        fx scale = GO_SCALE + (GO_SLAM - GO_SCALE) * (SLAM_TIME - t) / SLAM_TIME;
        textZoom(scale, GO_CX, GO_CY);
        textSetColor(COL_HOT);
        return;
    }

    textZoom(GO_SCALE, GO_CX, GO_CY);

    // 着弾後はゆっくり明滅させる。三角波で明 -> 暗 -> 明。
    int k = (t - SLAM_TIME) % PULSE;
    if (k > PULSE / 2) k = PULSE - k;                        // 0 .. PULSE/2
    textSetColor(ember(16 - (16 - LIT_MIN) * k / (PULSE / 2)));
}