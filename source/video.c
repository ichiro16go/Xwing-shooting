//---------------------------------------------------------------------------------
// video.c -- VRAM の割り当て、スプライト画像とパレットの読み込み
//---------------------------------------------------------------------------------
#include "game.h"

#include "spr_xwing.h"
#include "spr_tie.h"
#include "spr_boss.h"
#include "spr_shots.h"
#include "spr_blast.h"

u16 *gfxXwing[3];
u16 *gfxTie[2];
u16 *gfxBoss;
u16 *gfxShot[4];
u16 *gfxBlast[4];

//---------------------------------------------------------------------------------
// 16色パレットを SPRITE_PALETTE の指定バンクへ
//---------------------------------------------------------------------------------
static void loadPal(const unsigned short *pal, int bank)
{
    dmaCopy(pal, &SPRITE_PALETTE[bank * 16], 16 * sizeof(u16));
}

//---------------------------------------------------------------------------------
// grit が吐いたタイルデータから、フレーム 1 枚ぶんを VRAM に確保してコピーする。
// grit の -Mw/-Mh によりフレームごとに連続した 1D タイル順で並んでいるので、
// バイト単位のオフセットで切り出せる。
//---------------------------------------------------------------------------------
static u16 *loadFrame(const unsigned int *tiles, int frame, int bytes,
                      SpriteSize size)
{
    u16 *dst = oamAllocateGfx(&oamMain, size, SpriteColorFormat_16Color);
    const u8 *src = (const u8 *)tiles + frame * bytes;
    dmaCopy(src, dst, bytes);
    return dst;
}

void gfxInit(void)
{
    // VRAM の割り当て
    //   A: メイン画面の BG      -> 0x06000000 (BG のマップ/タイルはここが基点)
    //   B: メイン画面のスプライト -> 0x06400000 (スプライトの基点)
    //   C は consoleDemoInit() がサブ画面用に確保する
    //
    // スロットを省略した VRAM_B_MAIN_BG は 0x06020000 に載ってしまい、
    // bgInit() の mapBase/tileBase が指す 0x06000000 が未マップになる。
    // 事故りやすいのでアドレスまで書いた名前を使う。
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);

    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE |
                 DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);

    oamInit(&oamMain, SpriteMapping_1D_32, false);

    loadPal(spr_xwingPal, PB_XWING);
    loadPal(spr_tiePal,   PB_TIE);
    loadPal(spr_bossPal,  PB_BOSS);
    loadPal(spr_shotsPal, PB_SHOT);
    loadPal(spr_blastPal, PB_BLAST);

    int i;
    for (i = 0; i < 3; i++)
        gfxXwing[i] = loadFrame(spr_xwingTiles, i, 32 * 32 / 2, SpriteSize_32x32);
    for (i = 0; i < 2; i++)
        gfxTie[i]   = loadFrame(spr_tieTiles,   i, 16 * 16 / 2, SpriteSize_16x16);
    gfxBoss         = loadFrame(spr_bossTiles,  0, 64 * 64 / 2, SpriteSize_64x64);
    for (i = 0; i < 4; i++)
        gfxShot[i]  = loadFrame(spr_shotsTiles, i,   8 * 8 / 2, SpriteSize_8x8);
    for (i = 0; i < 4; i++)
        gfxBlast[i] = loadFrame(spr_blastTiles, i, 16 * 16 / 2, SpriteSize_16x16);
}
