#!/usr/bin/env python3
"""gfx/*.png を生成する。

grit に食わせる 4bpp(16色) インデックス PNG を書き出す。
パレット index 0 は透過色のマゼンタ (FF00FF) で固定。

  python3 tools/make_gfx.py     # gfx/*.png を再生成
"""
import os
import struct
import zlib

GFX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gfx")

# --------------------------------------------------------------------------
# 最小限のラスタ描画
# --------------------------------------------------------------------------


class Img:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[0] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y][x]
        return 0

    def rect(self, x0, y0, x1, y1, c):
        for y in range(min(y0, y1), max(y0, y1) + 1):
            for x in range(min(x0, x1), max(x0, x1) + 1):
                self.set(x, y, c)

    def dot(self, x, y, c, r=0):
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r + r:
                    self.set(x + dx, y + dy, c)

    def line(self, x0, y0, x1, y1, c, t=0):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.dot(x0, y0, c, t)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(cy - ry, cy + ry + 1):
            for x in range(cx - rx, cx + rx + 1):
                if rx and ry and ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.set(x, y, c)

    def poly(self, pts, c):
        """凸/凹どちらでも動くスキャンライン塗り"""
        ys = [p[1] for p in pts]
        for y in range(min(ys), max(ys) + 1):
            xs = []
            n = len(pts)
            for i in range(n):
                x0, y0 = pts[i]
                x1, y1 = pts[(i + 1) % n]
                if y0 == y1:
                    continue
                if min(y0, y1) <= y < max(y0, y1):
                    xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                for x in range(int(round(xs[i])), int(round(xs[i + 1])) + 1):
                    self.set(x, y, c)

    def mirror_x(self):
        """左半分を右へ鏡像コピー(左右対称の機体用)"""
        for y in range(self.h):
            for x in range(self.w // 2):
                self.px[y][self.w - 1 - x] = self.px[y][x]

    def outline(self, c):
        """非透過ピクセルの外周に輪郭を足す"""
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x]:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if self.get(x + dx, y + dy) not in (0, c):
                        add.append((x, y))
                        break
        for x, y in add:
            self.set(x, y, c)

    def squash_x(self, factor, shift=0):
        """横方向に縮めた新しい Img を返す(バンク中の機体用)"""
        out = Img(self.w, self.h)
        cx = (self.w - 1) / 2.0
        for y in range(self.h):
            for x in range(self.w):
                sx = int(round(cx + (x - cx - shift) / factor))
                if 0 <= sx < self.w:
                    out.px[y][x] = self.px[y][sx]
        return out

    def blit(self, dst, ox, oy):
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x]:
                    dst.set(ox + x, oy + y, self.px[y][x])


def write_png(path, frames, palette):
    """縦に並べたフレーム群を 8bit インデックス PNG として書き出す"""
    w = frames[0].w
    h = sum(f.h for f in frames)
    raw = bytearray()
    for f in frames:
        for row in f.px:
            raw.append(0)  # filter type 0
            raw.extend(row)
    pal = bytearray()
    for r, g, b in palette:
        pal += bytes((r, g, b))
    pal += bytes(3 * (16 - len(palette)))

    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", bytes(pal))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as fp:
        fp.write(png)
    print("  %-24s %dx%d (%d frames)" % (os.path.basename(path), w, h, len(frames)))


def C(s):
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


# --------------------------------------------------------------------------
# 自機: X-wing (32x32 / 3 フレーム)
# --------------------------------------------------------------------------
PAL_XWING = [
    C("ff00ff"),  # 0 透過
    C("14161f"),  # 1 輪郭
    C("454a57"),  # 2 影
    C("8b93a3"),  # 3 中間
    C("d5dae4"),  # 4 明
    C("ffffff"),  # 5 ハイライト
    C("a3231f"),  # 6 赤
    C("ff5a3c"),  # 7 明赤
    C("101c33"),  # 8 キャノピー影
    C("3f7fc4"),  # 9 キャノピー
    C("8fe6ff"),  # 10 エンジン
    C("ffd76a"),  # 11 エンジン芯
    C("2a2f3a"),  # 12 パネル線
    C("6a7080"),  # 13 砲身
    C("00d2ff"),  # 14 予備(明るい水色)
    C("ffee9c"),  # 15 予備(白熱)
]


def draw_xwing():
    im = Img(32, 32)
    # --- 主翼: 前方翼と後方翼で X を作る(左半分だけ描いて後で鏡像) ---
    im.poly([(12, 12), (3, 2), (1, 7), (12, 18)], 3)      # 前方翼
    im.poly([(12, 19), (1, 25), (2, 30), (12, 24)], 3)    # 後方翼
    im.line(12, 14, 2, 5, 4)                              # 翼の稜線
    im.line(12, 21, 2, 27, 4)
    im.line(12, 12, 3, 2, 2)                              # 前縁の影
    im.line(12, 24, 2, 30, 2)
    im.poly([(9, 11), (5, 7), (4, 9), (8, 13)], 6)        # 赤ライン
    im.poly([(9, 22), (5, 25), (6, 27), (10, 24)], 6)
    # --- 翼端の砲身(4門とも前方を向く) ---
    for ty in (0, 20):
        im.rect(2, ty, 3, ty + 8, 2)
        im.rect(2, ty + 1, 2, ty + 8, 13)
        im.rect(2, ty, 3, ty + 1, 7)
    # --- 胴体 ---
    im.poly([(15, 1), (14, 5), (13, 11), (12, 20), (12, 29), (15, 31)], 3)
    im.poly([(15, 3), (14, 8), (14, 20), (15, 30)], 4)
    im.rect(15, 1, 15, 4, 5)                              # 機首ハイライト
    im.rect(12, 16, 15, 17, 6)                            # 胴体の赤帯
    # --- キャノピー ---
    im.poly([(14, 8), (15, 8), (15, 16), (13, 16)], 8)
    im.poly([(14, 9), (15, 9), (15, 15), (14, 15)], 9)
    im.rect(14, 9, 15, 10, 5)
    # --- エンジンナセル ---
    im.poly([(9, 23), (13, 23), (13, 31), (9, 31)], 2)
    im.rect(10, 24, 12, 30, 3)
    im.rect(10, 25, 11, 29, 4)
    im.rect(10, 30, 12, 31, 10)                           # 排気
    im.rect(11, 31, 12, 31, 11)
    im.mirror_x()
    im.outline(1)
    return im


# --------------------------------------------------------------------------
# 敵機: TIE ファイター / インターセプター (16x16 / 2 フレーム)
# --------------------------------------------------------------------------
PAL_TIE = [
    C("ff00ff"),
    C("0d0f16"),  # 1 輪郭
    C("242a38"),  # 2 パネル暗部
    C("3c4658"),  # 3 パネル
    C("6d7994"),  # 4 フレーム
    C("aab4c8"),  # 5 明
    C("e4eaf4"),  # 6 ハイライト
    C("1a1f2b"),  # 7 コクピット窓
    C("8a1414"),  # 8 赤
    C("ff4a2a"),  # 9 明赤
    C("2f9c4a"),  # 10 緑(発光)
    C("7dff8f"),  # 11 明緑
    C("161a24"),
    C("55607a"),
    C("00d2ff"),
    C("ffffff"),
]


def draw_tie(interceptor):
    im = Img(16, 16)
    if interceptor:
        # ダガー型パネル
        im.poly([(0, 0), (5, 5), (5, 10), (0, 15)], 3)
        im.line(0, 0, 5, 5, 4)
        im.line(0, 15, 5, 10, 4)
        im.line(0, 0, 0, 15, 4)
        im.poly([(1, 5), (4, 7), (4, 8), (1, 10)], 2)
    else:
        # 六角形パネル
        im.poly([(0, 2), (3, 0), (5, 3), (5, 12), (3, 15), (0, 13)], 3)
        im.line(0, 2, 3, 0, 5)
        im.line(3, 0, 5, 3, 5)
        im.line(0, 13, 3, 15, 4)
        im.rect(2, 4, 3, 11, 2)
        im.line(1, 3, 1, 12, 4)
    im.rect(5, 7, 6, 8, 4)  # パイロン
    im.mirror_x()
    # 中央のコクピットボール
    im.ellipse(8, 8, 3, 3, 4)
    im.ellipse(8, 8, 2, 2, 5)
    im.rect(7, 7, 8, 8, 7)
    im.set(7, 7, 6)
    im.outline(1)
    return im


# --------------------------------------------------------------------------
# ボス: TIE Advanced (64x64)
# --------------------------------------------------------------------------
PAL_BOSS = [
    C("ff00ff"),
    C("07090e"),  # 1 輪郭
    C("1b2130"),  # 2 パネル暗部
    C("2f3a4e"),  # 3 パネル
    C("55637d"),  # 4 フレーム
    C("8b98b0"),  # 5 明
    C("cdd6e6"),  # 6 ハイライト
    C("12161f"),  # 7 窓
    C("7c1212"),  # 8 赤
    C("ff3c1e"),  # 9 明赤
    C("ffb03c"),  # 10 橙
    C("2f9c4a"),  # 11 緑
    C("7dff8f"),  # 12 明緑
    C("3a4458"),  # 13 桟
    C("00d2ff"),  # 14
    C("ffffff"),  # 15
]


def draw_boss():
    im = Img(64, 64)
    # 曲がった大型ソーラーパネル(左)
    im.poly([(2, 6), (16, 12), (16, 51), (2, 57)], 3)
    im.poly([(4, 11), (14, 16), (14, 47), (4, 52)], 2)
    im.line(2, 6, 16, 12, 5)
    im.line(2, 57, 16, 51, 4)
    im.line(2, 6, 2, 57, 5)
    for i in range(1, 6):  # パネルの桟
        y = 6 + i * 8
        im.line(3, y, 15, y + 4, 13)
    im.line(9, 9, 9, 54, 4)
    im.poly([(5, 27), (13, 31), (13, 33), (5, 36)], 8)  # 赤いライン
    # パイロン
    im.rect(16, 28, 24, 35, 4)
    im.rect(17, 30, 24, 33, 3)
    im.mirror_x()
    # 中央ハル
    im.ellipse(32, 32, 12, 13, 4)
    im.ellipse(32, 32, 10, 11, 5)
    im.ellipse(32, 31, 8, 9, 3)
    im.ellipse(32, 30, 6, 6, 7)          # コクピット窓
    im.poly([(27, 27), (37, 27), (36, 24), (28, 24)], 6)
    im.rect(26, 36, 38, 38, 2)
    im.line(24, 32, 40, 32, 13)
    # 砲口
    for gx in (25, 39):
        im.ellipse(gx, 43, 3, 4, 4)
        im.ellipse(gx, 43, 2, 3, 2)
        im.ellipse(gx, 45, 1, 1, 9)
    im.ellipse(32, 45, 3, 3, 8)
    im.ellipse(32, 45, 2, 2, 9)
    im.ellipse(32, 45, 1, 1, 10)
    im.outline(1)
    return im


# --------------------------------------------------------------------------
# 弾 (8x8 / 4 フレーム)
# --------------------------------------------------------------------------
PAL_SHOT = [
    C("ff00ff"),
    C("3a0000"),  # 1 赤影
    C("c81e14"),  # 2 赤
    C("ff6a3c"),  # 3 明赤
    C("ffe9c8"),  # 4 芯(白)
    C("003a12"),  # 5 緑影
    C("14b43c"),  # 6 緑
    C("6cff8a"),  # 7 明緑
    C("d8ffe0"),  # 8 芯(白緑)
    C("06304a"),  # 9 青影
    C("1e8ad2"),  # 10 青
    C("6ad2ff"),  # 11 明青
    C("e0f6ff"),  # 12 芯
    C("ffb03c"),  # 13 橙
    C("ffe98a"),  # 14
    C("ffffff"),  # 15
]


def draw_bolt(shadow, mid, hi, core, wide=False):
    im = Img(8, 8)
    x0, x1 = (2, 5) if wide else (3, 4)
    im.rect(x0, 0, x1, 7, shadow)
    im.rect(x0, 1, x1, 6, mid)
    im.rect(x0 + (1 if wide else 0), 1, x1 - (1 if wide else 0), 6, hi)
    im.rect(3, 2, 4, 5, core)
    return im


def draw_orb(shadow, mid, hi, core):
    im = Img(8, 8)
    im.ellipse(3, 3, 3, 3, shadow)
    im.ellipse(3, 3, 2, 2, mid)
    im.ellipse(3, 3, 1, 1, hi)
    im.set(3, 3, core)
    return im


# --------------------------------------------------------------------------
# 爆発 (16x16 / 4 フレーム)
# --------------------------------------------------------------------------
PAL_BLAST = [
    C("ff00ff"),
    C("2a0c00"),  # 1
    C("7a1e00"),  # 2
    C("c8420a"),  # 3
    C("ff7a1e"),  # 4
    C("ffb03c"),  # 5
    C("ffe07a"),  # 6
    C("fff6c8"),  # 7
    C("ffffff"),  # 8
    C("6a6a7a"),  # 9 煙
    C("3a3a48"),  # 10 煙暗
    C("8fe6ff"),  # 11
    C("1e1e28"),
    C("ff3c1e"),
    C("00d2ff"),
    C("d5dae4"),
]


def draw_blast(step):
    im = Img(16, 16)
    cx = cy = 8
    if step == 0:
        im.ellipse(cx, cy, 3, 3, 5)
        im.ellipse(cx, cy, 2, 2, 7)
        im.ellipse(cx, cy, 1, 1, 8)
    elif step == 1:
        im.ellipse(cx, cy, 7, 7, 3)
        im.ellipse(cx, cy, 5, 5, 5)
        im.ellipse(cx, cy, 3, 3, 7)
        im.ellipse(cx, cy, 1, 1, 8)
        for dx, dy in ((-7, -5), (7, -5), (-6, 6), (6, 6)):
            im.dot(cx + dx, cy + dy, 4, 1)
    elif step == 2:
        im.ellipse(cx, cy, 7, 7, 2)
        im.ellipse(cx, cy, 5, 5, 3)
        im.ellipse(cx, cy, 3, 3, 5)
        for dx, dy in ((-7, -6), (7, -6), (-7, 7), (7, 7), (0, -7), (0, 7)):
            im.dot(cx + dx, cy + dy, 4, 1)
        im.ellipse(cx - 3, cy - 2, 2, 2, 0)
    else:
        im.ellipse(cx, cy, 6, 6, 10)
        im.ellipse(cx, cy, 4, 4, 9)
        im.ellipse(cx - 2, cy + 1, 2, 2, 0)
        im.ellipse(cx + 3, cy - 2, 1, 1, 0)
        for dx, dy in ((-7, -6), (7, -6), (-6, 7), (6, 7)):
            im.dot(cx + dx, cy + dy, 2, 0)
    return im


# --------------------------------------------------------------------------
# タイトルロゴ: X-WING (256x64 / 64x64 を 4 枚に分割)
#
# 太い書体で字形を塗りつぶしてから内側をくり抜き、輪郭だけを残す。
# 抜けたところは透過なので、後ろの星空がそのまま透ける。
# --------------------------------------------------------------------------
PAL_LOGO = [
    C("ff00ff"),  # 0 透過
    C("ffe81f"),  # 1 ロゴの黄色
    C("7a5c00"),  # 2 外周の影(星空との境目)
]

LOGO_W, LOGO_H = 256, 64
LOGO_TOP, LOGO_BOT = 4, 57      # 字の上端 / 下端
LOGO_STROKE = 12                # 画の太さ
LOGO_RING = 2                   # 残す輪郭の太さ


def _slant(im, xa, ya, xb, yb, t, c):
    """縦の太さを t に保った斜めの画。端は水平に切れる。"""
    h = t // 2
    im.poly([(int(xa) - h, int(ya)), (int(xa) + h, int(ya)),
             (int(xb) + h, int(yb)), (int(xb) - h, int(yb))], c)


def _glyph(im, ch, x, w, c):
    y0, y1, t = LOGO_TOP, LOGO_BOT, LOGO_STROKE
    cy = (y0 + y1) // 2
    if ch == "X":
        _slant(im, x + t // 2, y0, x + w - t // 2, y1, t, c)
        _slant(im, x + w - t // 2, y0, x + t // 2, y1, t, c)
    elif ch == "-":
        im.rect(x, cy - t // 2, x + w - 1, cy + t // 2, c)
    elif ch == "W":
        mid = y0 + (y1 - y0) // 4          # 真ん中の山は少し下げる
        px = [x + t // 2, x + int(w * 0.30), x + w // 2,
              x + int(w * 0.70), x + w - t // 2]
        _slant(im, px[0], y0, px[1], y1, t, c)
        _slant(im, px[1], y1, px[2], mid, t, c)
        _slant(im, px[2], mid, px[3], y1, t, c)
        _slant(im, px[3], y1, px[4], y0, t, c)
    elif ch == "I":
        im.rect(x + w // 2 - t // 2, y0, x + w // 2 + t // 2, y1, c)
    elif ch == "N":
        im.rect(x, y0, x + t - 1, y1, c)
        im.rect(x + w - t, y0, x + w - 1, y1, c)
        _slant(im, x + t // 2, y0, x + w - t // 2, y1, t, c)
    elif ch == "G":
        cx = x + w // 2
        im.ellipse(cx, cy, w // 2, (y1 - y0) // 2, c)
        im.ellipse(cx, cy, w // 2 - t, (y1 - y0) // 2 - t, 0)
        im.rect(cx, y0, x + w, cy - t, 0)          # 右上を開けて C にする
        im.rect(cx + 2, cy - t, x + w - 1, cy, c)  # 中央の横棒


def _hollow(im, t, c):
    """塗りの内側(縁から t 以上離れた画素)を抜いて輪郭だけにする"""
    inner = []
    for y in range(im.h):
        for x in range(im.w):
            if im.px[y][x] != c:
                continue
            if all(im.get(x + dx, y + dy)
                   for dy in range(-t, t + 1) for dx in range(-t, t + 1)):
                inner.append((x, y))
    for x, y in inner:
        im.set(x, y, 0)


def draw_logo():
    # 字幅と字間。合計 241px を 256px の中央に置く
    glyphs = [("X", 42), ("-", 18), ("W", 56), ("I", 16), ("N", 42), ("G", 42)]
    gap = 5
    total = sum(w for _, w in glyphs) + gap * (len(glyphs) - 1)

    im = Img(LOGO_W, LOGO_H)
    x = (LOGO_W - total) // 2
    for ch, w in glyphs:
        _glyph(im, ch, x, w, 1)
        x += w + gap

    _hollow(im, LOGO_RING, 1)
    im.outline(2)
    return im


def split_frames(im, size):
    """横に並んだ絵を size x size のコマへ切り分ける"""
    out = []
    for i in range(im.w // size):
        f = Img(size, size)
        for y in range(min(size, im.h)):
            for x in range(size):
                f.px[y][x] = im.px[y][i * size + x]
        out.append(f)
    return out


# --------------------------------------------------------------------------
def main():
    os.makedirs(GFX, exist_ok=True)
    print("generating gfx/")

    base = draw_xwing()
    write_png(
        os.path.join(GFX, "spr_xwing.png"),
        [base, base.squash_x(0.80, -2), base.squash_x(0.80, 2)],
        PAL_XWING,
    )
    write_png(os.path.join(GFX, "spr_tie.png"), [draw_tie(False), draw_tie(True)], PAL_TIE)
    write_png(os.path.join(GFX, "spr_boss.png"), [draw_boss()], PAL_BOSS)
    write_png(
        os.path.join(GFX, "spr_shots.png"),
        [
            draw_bolt(1, 2, 3, 4),            # 0: 自機レーザー(赤)
            draw_bolt(5, 6, 7, 8),            # 1: 敵レーザー(緑)
            draw_bolt(9, 10, 11, 12, True),   # 2: 自機強化ショット(青/太)
            draw_orb(5, 6, 7, 8),             # 3: 敵の弾(球)
        ],
        PAL_SHOT,
    )
    write_png(
        os.path.join(GFX, "spr_blast.png"),
        [draw_blast(i) for i in range(4)],
        PAL_BLAST,
    )

    write_png(
        os.path.join(GFX, "spr_logo.png"),
        split_frames(draw_logo(), 64),
        PAL_LOGO,
    )


if __name__ == "__main__":
    main()
