# X-WING — 設計メモ

スターウォーズ世界観の縦スクロールシューティング。自機 X-wing、雑魚 TIE ファイター /
TIE インターセプター、ボス TIE Advanced。

- 上画面（メインエンジン）= ゲーム画面
- 下画面（サブエンジン）= HUD（コンソール文字）

## 目次

- [操作](#操作)
- [ゲームの流れ](#ゲームの流れ)
- [モジュール構成](#モジュール構成)
- [座標系](#座標系)
- [スプライトの割り当て](#スプライトの割り当て)
- [グラフィックスの作り方](#グラフィックスの作り方)
- [調整したいときどこを触るか](#調整したいときどこを触るか)
- [踏んだ NDS の罠](#踏んだ-nds-の罠)

---

## 操作

| 場面 | 入力 | 動作 |
| --- | --- | --- |
| タイトル | START | ゲーム開始 |
| プレイ中 | 十字キー | 8 方向移動 |
| プレイ中 | （自動） | ショットは常時発射 |
| プレイ中 | A | プロトンボム（敵弾を全消し + 全体にダメージ） |
| プレイ中 | START | ポーズ / 再開 |
| プレイ中 | SELECT | **デバッグ用**: ボス戦まで飛ぶ |
| GAME OVER | START / SELECT | リスタート / タイトルへ |
| クリア | START / SELECT | もう一度 / タイトルへ |

ショットはスコアで自動的に強化される（10,000 で LV.2、30,000 で LV.3）。

## ゲームの流れ

```
ST_TITLE ──START──> ST_PLAY ──全ウェーブ消化──> ST_WARNING ──180F──> ST_BOSS
                       │                                              │
                    残機 0                                         ボス撃破
                       ↓                                              ↓
                  ST_GAMEOVER <──START(リスタート)──              ST_CLEAR
```

ボスは HP の残量で 3 フェーズに移る。

| フェーズ | HP | 動き | 攻撃 |
| --- | --- | --- | --- |
| 1 | 100–65% | ゆっくり左右 | 3 way 拡散 |
| 2 | 65–30% | 速く左右 + 上下に揺れ | 12 way 全方位 / 二門からの狙い撃ち |
| 3 | 30–0% | さらに速い | 16 way 全方位 / 5 way 拡散 + 狙い撃ち / 護衛の TIE を呼ぶ |

## モジュール構成

[game.h](../source/game.h) に型・定数・各モジュールの入口をまとめ、実装をファイルごとに分ける。

| ファイル | 責務 |
| --- | --- |
| [main.c](../source/main.c) | 初期化、状態遷移、メインループ |
| [video.c](../source/video.c) | VRAM の割り当て、スプライト画像とパレットの読み込み |
| [starfield.c](../source/starfield.c) | 視差スクロールする星空（BG0 = 手前 / BG1 = 奥） |
| [player.c](../source/player.c) | 自機の移動・ショット・ボム・被弾 |
| [shot.c](../source/shot.c) | 自機弾 / 敵弾のプールと当たり判定 |
| [enemy.c](../source/enemy.c) | 敵機の移動パターンと射撃 |
| [boss.c](../source/boss.c) | ボスのフェーズ管理と弾幕 |
| [effect.c](../source/effect.c) | 爆発アニメーション |
| [wave.c](../source/wave.c) | 敵の出現スケジュール（表を読むだけ） |
| [hud.c](../source/hud.c) | 下画面の情報表示 |

メインループは「更新 → 描画バッファ構築 → VBlank 待ち → OAM/BG へ反映」の順。
OAM の書き換えは VBlank 後にまとめて行う。

```c
while (pmMainLoop()) {
    scanKeys();
    update();          // ゲームの状態を進める
    render();          // OAM バッファと BG スクロール値を作る
    hudDraw();
    swiWaitForVBlank();
    oamUpdate(&oamMain);
    bgUpdate();
}
```

## 座標系

位置と速度は **8.8 固定小数**（`typedef s32 fx`）。整数ピクセルだと敵のサイン移動や
斜め移動が汚くなるため。

```c
player.x += FX(2) + 128;      // 2.5 px / frame
oamSet(..., fx2i(player.x) - 16, ...);   // 表示するときだけ整数に落とす
```

当たり判定は中心座標どうしの AABB（`hits()`）。見た目より小さめの矩形を使う。
自機の判定は 8x8（見た目は 32x32）。

## スプライトの割り当て

動的に確保せず、種類ごとに OAM の固定範囲を割り当てる。合計 98 / 128。

| 用途 | OAM index | 数 |
| --- | --- | --- |
| 自機 | 0 | 1 |
| ボス | 1 | 1 |
| 自機弾 | 2–25 | 24 |
| 敵機 | 26–41 | 16 |
| 敵弾 | 42–81 | 40 |
| 爆発 | 82–97 | 16 |

パレットは 16 色 × バンクで分ける（`PB_XWING`, `PB_TIE`, `PB_BOSS`, `PB_SHOT`, `PB_BLAST`）。

## グラフィックスの作り方

ドット絵は [tools/make_gfx.py](../tools/make_gfx.py) が**コードで描いて** PNG を吐く。
図形プリミティブ（`line` / `poly` / `ellipse`）で左半分を描き、`mirror_x()` で
左右対称にし、`outline()` で輪郭を付ける、という組み立て方。

```
tools/make_gfx.py  ──>  gfx/spr_*.png (4bpp インデックス PNG)
                            │  gfx/spr_*.grit が変換オプションを指定
                            ▼  make が grit を呼ぶ
                        build/spr_*.h  (spr_xwingTiles, spr_xwingPal ...)
                            ▼
                        video.c が VRAM へ転送
```

絵を描き変えたいときは 2 通り。

1. `tools/make_gfx.py` を編集して `python3 tools/make_gfx.py` で再生成する
2. `gfx/spr_*.png` をそのまま画像エディタで開いて描き替える（16 色・index 0 = マゼンタ透過を維持すること）

`.grit` の `-Mw` / `-Mh` はスプライト 1 枚のタイル数。32x32 なら `-Mw4 -Mh4`。
これを指定すると grit がスプライト用の 1D タイル順で並べてくれる。

> **注意**: `gfx/` のファイル名に `spr_` を付けているのは、`source/boss.c` と
> `gfx/boss.png` が両方 `boss.o` になって衝突するのを避けるため。

## 調整したいときどこを触るか

| 変えたいこと | 場所 |
| --- | --- |
| 敵の出現タイミング・編隊・ウェーブ構成 | [wave.c](../source/wave.c) の `stage[]` テーブル |
| 敵の移動パターン | [enemy.c](../source/enemy.c) の `move()` |
| 敵の硬さ・スコア・射撃間隔 | [enemy.c](../source/enemy.c) の `enemySpawn()` |
| ボスの HP・フェーズ境界・弾幕 | [boss.c](../source/boss.c) の `BOSS_MAX_HP` と `attack()` |
| 自機の速度・連射速度・強化段階 | [player.c](../source/player.c) の `SPD` と `fire()` |
| 星の密度・スクロール速度 | [starfield.c](../source/starfield.c) の `buildMap()` / `main.c` の `starUpdate()` |
| HUD の文言・レイアウト | [hud.c](../source/hud.c) |

`wave.c` の 1 行が 1 編隊。時刻・種類・パターン・機数・間隔・座標・速度だけの表なので、
ここを書き換えるだけで構成を差し替えられる。

```c
//  開始F  ウェーブ  種類           パターン       機数 間隔   x   dx   y     vx     vy
{   90,   1, EK_FIGHTER,     PAT_STRAIGHT, 5, 26,  40,  44, -20,     0, FX(2) },
```

## 踏んだ NDS の罠

この実装で実際に詰まった点。同じところで止まったとき用。

### 1. VRAM バンクはアドレスまで指定する

`VRAM_B_MAIN_BG` は「メイン BG のスロット **1**」= **0x06020000** にマップされる。
`bgInit()` の `mapBase` / `tileBase` は 0x06000000 起点なので、これだと未マップの領域を
指すことになり、**背景色しか出ない**。

```c
vramSetBankA(VRAM_A_MAIN_BG_0x06000000);      // アドレス付きの名前を使う
vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);
```

### 2. DMA は DTCM / ITCM を読めない

**これが一番はまった。** DMA コントローラは CPU 内蔵メモリ（DTCM / ITCM）にアクセスできない。
ARM9 のスタックは DTCM 上にあるため、**ローカル変数の配列を `dmaCopy()` しても何も転送されない**
（エラーも出ない）。

```c
static u8 tiles[256];      // static にしてメインメモリに置く
...
DC_FlushRange(tiles, sizeof(tiles));
dmaCopy(tiles, gfx, sizeof(tiles));
```

### 3. DMA は CPU のデータキャッシュを見ない

CPU が書いた直後のデータはキャッシュに残っていてメインメモリに届いていないことがある。
DMA する前に `DC_FlushRange()` が必要。grit が吐いた `const` 配列は CPU が書き込まないので不要。

### 4. 起動直後の 1 回目の `scanKeys()` は信用できない

キー入力レジスタが確定する前の値を拾って「全キーが押された」と報告することがあり、
タイトル画面が一瞬で飛ばされた。メインループに入る前に空読みしておく。

```c
scanKeys();
scanKeys();
```

### 5. 状態遷移を毎フレーム実行しない

`if (--timer <= 0) { 状態を変える }` と書くと、timer が 0 を割ったあとも毎フレーム
成立し続けて遷移処理が走り、遷移先で使うフレームカウンタが 0 に戻り続ける。
GAME OVER からリスタートできなくなっていた原因がこれ。

```c
if (timer > 0 && --timer == 0) { /* 0 になった瞬間だけ */ }
```

### 6. `xorshift` は種を 0 にしてはいけない

状態 0 から抜け出せず、永久に 0 を返し続ける。`starInit()` が種を入れる前に
`rnd()` を呼んでいて、ゲーム全体の乱数が死んでいた。
