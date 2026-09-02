//---------------------------------------------------------------------------------
// starfield.c -- 視差スクロールする星空
//
// BG0(手前の星, 速い)と BG1(奥の星, 遅い)の 2 枚を別々の速度で縦スクロールさせる。
// タイル画像は PNG を用意せずコードで組み立てている(点を打つだけなので)。
//---------------------------------------------------------------------------------
#include "game.h"
#include <string.h>

#define STAR_TILES 8      // 0 は空タイル、1..7 が星入りタイル

static int bgNear, bgFar;
static fx  scrollNear, scrollFar;

//---------------------------------------------------------------------------------
// 4bpp タイル 1 枚 = 32 バイト。1 ピクセル 4 ビットなので u8 に 2 ピクセル入る。
//---------------------------------------------------------------------------------
static void putPixel(u8 *tile, int x, int y, int color)
{
    u8 *p = &tile[y * 4 + (x >> 1)];
    if (x & 1) *p = (*p & 0x0F) | (u8)(color << 4);
    else       *p = (*p & 0xF0) | (u8)(color & 0x0F);
}

static void buildTiles(u16 *gfx)
{
    // static にしてメインメモリへ置くこと。ローカル変数はスタック=DTCM に
    // 乗るが、DMA コントローラは DTCM/ITCM(CPU 内蔵メモリ)を読めないため、
    // スタック上のバッファを dmaCopy すると何も転送されない。
    static u8 tiles[STAR_TILES * 32];
    memset(tiles, 0, sizeof(tiles));

    // タイル 1..7 に、位置と明るさを変えた星を 1〜2 個ずつ置く
    static const u8 spot[7][3] = {   // x, y, color
        { 2, 3, 1 }, { 5, 1, 2 }, { 1, 6, 1 }, { 6, 5, 3 },
        { 3, 0, 2 }, { 0, 2, 1 }, { 4, 6, 2 },
    };
    int i;
    for (i = 0; i < 7; i++) {
        u8 *t = &tiles[(i + 1) * 32];
        putPixel(t, spot[i][0], spot[i][1], spot[i][2]);
        if (i & 1) putPixel(t, (spot[i][0] + 4) & 7, (spot[i][1] + 3) & 7, 1);
    }

    // DMA は CPU のデータキャッシュを見ない。直前に CPU が書いた内容は
    // まだキャッシュにいてメインメモリに届いていないので、フラッシュしてから
    // 転送する。(grit が吐いた const 配列は書き込んでいないので不要)
    DC_FlushRange(tiles, sizeof(tiles));
    dmaCopy(tiles, gfx, sizeof(tiles));
}

//---------------------------------------------------------------------------------
// 星をまばらに配置したマップを作る。density が小さいほど星が多い。
//---------------------------------------------------------------------------------
static void buildMap(u16 *map, int density)
{
    int i;
    for (i = 0; i < 32 * 32; i++)
        map[i] = (rndRange(density) == 0) ? (u16)(1 + rndRange(7)) : 0;
}

void starInit(void)
{
    bgFar  = bgInit(1, BgType_Text4bpp, BgSize_T_256x256, 1, 1);
    bgNear = bgInit(0, BgType_Text4bpp, BgSize_T_256x256, 0, 1);

    // スプライトより後ろに回す(数字が大きいほど奥)
    bgSetPriority(bgNear, 2);
    bgSetPriority(bgFar,  3);

    buildTiles(bgGetGfxPtr(bgNear));   // タイルは 2 枚で共有(tileBase が同じ)

    buildMap(bgGetMapPtr(bgNear), 22);
    buildMap(bgGetMapPtr(bgFar),  10);

    // 背景色と星の色
    BG_PALETTE[0] = RGB15(1, 1, 4);      // 宇宙の地の色
    BG_PALETTE[1] = RGB15(9, 9, 14);     // 暗い星
    BG_PALETTE[2] = RGB15(18, 19, 24);   // 中くらい
    BG_PALETTE[3] = RGB15(31, 31, 31);   // 明るい星

    scrollNear = scrollFar = 0;
}

void starUpdate(int speed)
{
    scrollNear += speed;
    scrollFar  += speed / 3;
}

void starApply(void)
{
    bgSetScroll(bgNear, 0, -fx2i(scrollNear));
    bgSetScroll(bgFar,  0, -fx2i(scrollFar));
}
