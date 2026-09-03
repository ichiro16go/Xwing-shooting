//---------------------------------------------------------------------------------
// hud.c -- 下画面(サブエンジン)の情報表示
//
// consoleDemoInit() が用意するテキストコンソールに ANSI エスケープで
// 位置を指定して書き込む。毎フレーム全消し + 再描画。
//---------------------------------------------------------------------------------
#include "game.h"
#include <stdio.h>

#define AT(row, col) iprintf("\x1b[%d;%dH", (row), (col))

void hudInit(void)
{
    consoleDemoInit();
}

//---------------------------------------------------------------------------------
static void drawBar(int cur, int max, int width)
{
    int fill = (max > 0) ? cur * width / max : 0;
    int i;
    iprintf("[");
    for (i = 0; i < width; i++) iprintf(i < fill ? "#" : ".");
    iprintf("]");
}

static void drawStatus(void)
{
    AT(4, 1);  iprintf("SCORE     %08d", g.score);
    AT(5, 1);  iprintf("HI        %08d", g.hiScore);

    AT(7, 1);  iprintf("LIFE      ");
    int i;
    for (i = 0; i < player.lives; i++) iprintf("<>");
    AT(8, 1);  iprintf("BOMB      ");
    for (i = 0; i < player.bombs; i++) iprintf("* ");
    AT(9, 1);  iprintf("POWER     LV.%d", player.power);

    AT(11, 1); iprintf("WAVE      %d / %d", waveNumber(), waveTotal());
}

static void drawBossBar(void)
{
    AT(14, 1); iprintf("TARGET  TIE ADVANCED x1");
    AT(15, 1); iprintf(" ");
    drawBar(boss.hp, boss.maxHp, 24);
}

//---------------------------------------------------------------------------------
void hudDraw(void)
{
    consoleClear();

    AT(0, 0);  iprintf("================================");
    AT(1, 5);  iprintf("REBEL  ASSAULT");
    AT(2, 0);  iprintf("================================");
    AT(13, 0); iprintf("--------------------------------");
    if (g.state == ST_GAMEOVER || g.state == ST_CLEAR)
        { AT(23, 1); iprintf("START RETRY   SELECT TITLE"); }
    else if (g.state == ST_TITLE)
        { AT(23, 1); iprintf("START / TOUCH  TO  LAUNCH"); }
    else if (g.state == ST_CRAWL)
        { AT(23, 1); iprintf("START / TOUCH  TO  SKIP"); }
    else
        { AT(23, 1); iprintf("D-PAD MOVE  A BOMB  START PAUSE"); }

    switch (g.state) {
    case ST_CRAWL:
        AT(10, 8); iprintf("EPISODE   I");
        AT(12, 6); iprintf("REBEL  ASSAULT");
        break;

    case ST_TITLE:
        AT(5, 9);  iprintf("X - W I N G");
        AT(7, 4);  iprintf("PRESS  START  OR  TAP");
        AT(10, 2); iprintf("D-PAD  ... MANEUVER");
        AT(11, 2); iprintf("CANNON ... AUTO FIRE");
        AT(12, 2); iprintf("A      ... PROTON BOMB");
        AT(15, 2); iprintf("HI-SCORE   %08d", g.hiScore);
        AT(17, 2); iprintf("DESTROY THE TIE ADVANCED");
        AT(18, 2); iprintf("AND ESCAPE THE SECTOR.");
        break;

    case ST_PLAY:
        drawStatus();
        break;

    case ST_WARNING:
        drawStatus();
        if ((g.timer / 15) & 1) {
            AT(15, 4); iprintf("!!  W A R N I N G  !!");
            AT(17, 3); iprintf("CAPITAL SHIP INBOUND");
        }
        break;

    case ST_BOSS:
        drawStatus();
        drawBossBar();
        if (boss.dying) { AT(17, 6); iprintf("TARGET  BURNING"); }
        break;

    case ST_CLEAR:
        drawStatus();
        AT(15, 4); iprintf("TARGET  DESTROYED");
        AT(17, 3); iprintf("GREAT SHOT, KID.");
        if (g.timer > 90) {
            AT(20, 2); iprintf("START  ... FLY AGAIN");
            AT(21, 2); iprintf("SELECT ... TITLE");
        }
        break;

    case ST_GAMEOVER:
        drawStatus();
        AT(15, 6); iprintf("G A M E  O V E R");
        if (g.timer > 60) {
            AT(20, 2); iprintf("START  ... RESTART");
            AT(21, 2); iprintf("SELECT ... TITLE");
        }
        break;
    }

    if (g.paused) { AT(18, 10); iprintf("P A U S E"); }
}
