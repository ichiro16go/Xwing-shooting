//---------------------------------------------------------------------------------
// textbg.c -- BG2(回転BG)の文字レイヤ
//
// オープニングのクロールとタイトル画面が共有する。libnds のコンソールフォント
// (1bpp 8x8)を 8bpp タイルに展開しておき、64x64 タイル(= 512px 四方)のマップに
// 文字コードを置くだけ。仕組みは下画面のコンソールと同じ。
//
// 回転BG にしてあるのは、スキャンラインごとに変形行列を差し替えて擬似 3D の
// クロールを作るため(opening.c)。タイトル画面では等倍で使う。
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

// VRAM_A(0x06000000)の中の置き場所
//   0x0000 / 0x0800 : 星空 BG のマップ    (starfield.c)
//   0x1000          : この文字マップ 4KB
//   0x4000          : 星空 BG のタイル    (starfield.c)
//   0xC000          : この文字タイル 6KB
#define MAP_BASE   2    // 2KB 単位
#define TILE_BASE  3    // 16KB 単位

#define FIRST_CHAR 32   // タイル 0 = ' '。マップを 0 で埋めれば空白になる
#define NUM_CHARS  96   // ' ' .. '\x7f'
#define TEXT_PAL   4    // BG パレットの何番を文字色にするか(0..3 は星空が使用)

static int  layer;
static u16 *map;        // 8bpp マップ = 1文字 1バイト。u16 単位で読み書きする

//---------------------------------------------------------------------------------
// 1bpp のコンソールフォントを 8bpp タイルへ展開する。
// フォントは 1 バイトが 1 行、bit 0 が左端の画素。
//
// VRAM はバイト単位の書き込みができない(捨てられる)ので、必ず 16/32bit で書く。
//---------------------------------------------------------------------------------
static void loadFont(void)
{
    const u8 *src = (const u8 *)consoleGetDefault()->font.gfx;
    u32 *dst = (u32 *)bgGetGfxPtr(layer);

    int c, y, x;
    for (c = 0; c < NUM_CHARS; c++) {
        const u8 *glyph = src + (FIRST_CHAR + c) * 8;
        for (y = 0; y < 8; y++) {
            u32 lo = 0, hi = 0;          // 8 画素 = 8 バイト = 2 ワード
            for (x = 0; x < 4; x++) {
                if ((glyph[y] >> x)       & 1) lo |= (u32)TEXT_PAL << (x * 8);
                if ((glyph[y] >> (x + 4)) & 1) hi |= (u32)TEXT_PAL << (x * 8);
            }
            *dst++ = lo;
            *dst++ = hi;
        }
    }
}

//---------------------------------------------------------------------------------
void textInit(void)
{
    layer = bgInit(2, BgType_Rotation, BgSize_R_512x512, MAP_BASE, TILE_BASE);
    bgSetPriority(layer, 1);         // 星空(2, 3)より手前、スプライトより奥

    // マップの外は回り込ませず透過にする(文字のない所は星空を見せたい)
    REG_BG2CNT &= ~BG_WRAP_ON;

    // bgInit が「行列を書き直せ」の印を立てるので、ここで一度吐き出させておく。
    // 以降 BG2 の変形は自前でレジスタに書くので、bgUpdate() には触らせない。
    bgUpdate();

    map = bgGetMapPtr(layer);
    loadFont();
    textClear();
    textIdentity(TEXT_CENTER_X - SCR_W / 2, 0);
    textShow(0);
}

void textClear(void)
{
    dmaFillWords(0, map, TEXT_COLS * TEXT_ROWS);   // 64x64 バイト = 4KB
}

//---------------------------------------------------------------------------------
static void putChar(int col, int row, int tile)
{
    if ((unsigned)col >= TEXT_COLS || (unsigned)row >= TEXT_ROWS) return;

    // 1文字 1バイトだが VRAM はバイト書き込みができないので、
    // 隣の文字ごと 16bit で読んで書き戻す。
    u16 *p = &map[(row * TEXT_COLS + col) >> 1];
    u16 v  = *p;
    if (col & 1) *p = (u16)((v & 0x00FF) | (tile << 8));
    else         *p = (u16)((v & 0xFF00) |  tile);
}

void textPut(int col, int row, const char *s)
{
    for (; *s; s++, col++) {
        int c = (u8)*s - FIRST_CHAR;
        putChar(col, row, (c >= 0 && c < NUM_CHARS) ? c : 0);
    }
}

void textPutCenter(int row, const char *s)
{
    textPut(TEXT_COLS / 2 - (int)strlen(s) / 2, row, s);
}

//---------------------------------------------------------------------------------
void textSetColor(u16 color)
{
    BG_PALETTE[TEXT_PAL] = color;
}

// 変形なし。画面の (0, 0) にマップの (ox, oy) が来る。
void textIdentity(int ox, int oy)
{
    REG_BG2PA = 1 << 8;  REG_BG2PB = 0;
    REG_BG2PC = 0;       REG_BG2PD = 1 << 8;
    REG_BG2X  = ox << 8;
    REG_BG2Y  = oy << 8;
}

//---------------------------------------------------------------------------------
// 拡大表示。テクスチャの (cx, cy) が画面の中央に来るように、scale 倍で映す。
//
// 変形行列に入れるのは倍率そのものではなく「画面 1px が映すテクスチャ px 数」=
// 倍率の逆数。BG2X / BG2Y も画面 (0, 0) が映すテクスチャ座標なので、中央から
// 半画面ぶんだけ戻した位置になる。
//---------------------------------------------------------------------------------
void textZoom(fx scale, int cx, int cy)
{
    int inv = (1 << (FXS * 2)) / scale;   // 8.8。FX(1) なら 256 = 等倍

    REG_BG2PA = (s16)inv;  REG_BG2PB = 0;
    REG_BG2PC = 0;         REG_BG2PD = (s16)inv;
    REG_BG2X  = (cx << FXS) - inv * (SCR_W / 2);
    REG_BG2Y  = (cy << FXS) - inv * (SCR_H / 2);
}

void textShow(int on)
{
    if (on) bgShow(layer);
    else    bgHide(layer);
}
