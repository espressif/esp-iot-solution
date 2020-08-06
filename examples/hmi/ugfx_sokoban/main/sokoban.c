
#include "iot_ugfx.h"
#include "src/gwin/gwin_class.h"
#include "stdlib.h"
#include "string.h"
#include "sokoban.h"
#include "sokoban_levels.h"

static GEventMouse ev;
GHandle mainWin;

uint8_t sokobanField[SOKOBAN_FIELD_WIDTH * SOKOBAN_FIELD_HEIGHT]; // Current level map will be stored here
int8_t sokobanUndoList[SOKOBAN_UNDO_COUNT][4];                    // Undo list, 0 = sokobanCharPosX, 1 = sokobanCharPosY, 2 = y direction, 3 = y direction
uint8_t sokobanUndoCount = 0;                                     // How many udnos we have at the moment
uint8_t sokobanUndoCurrentPos = 0;                                // Current undo position, this is like iter
uint8_t sokobanBoxCount;                                          // Level total box count
uint8_t sokobanCharPosX, sokobanCharPosY;                         // Level characters position
uint8_t sokobanCharDir;                                           // Level characters direction
const uint8_t *sokobanCurrentLvl;                                 // Pointer to original lvl data, uuugh, ugly!
uint16_t sokobanCurrentLvlNumber = 0;                             // Level number
uint32_t sokobanMoves = 0;                                        // Moves made in current level

bool_t sokobanGameOver = FALSE;
systemticks_t sokobanLastMove;    // Stores last move time, this is to make character look south after a timeout
systemticks_t sokobanTouchRepeat; // Stores ticks for repeating moves
font_t font;
const char *sokobanGraph[] = {"wall.bmp", "floor.bmp", "box.bmp", "goal.bmp", "char_s.bmp", "char_e.bmp", "char_n.bmp", "char_w.bmp", "boxgoal.bmp"};
gdispImage sokobanImage[9];
gdispImage sokobanButtons;

#ifdef SOKOBAN_SHOW_SPLASH
GTimer sokobanSplashBlink;
bool_t sokobanSplashTxtVisible = FALSE;
gdispImage sokobanSplashImage;
#endif

static int uitoa(unsigned int value, char *buf, int max)
{
    int n = 0;
    int i = 0;
    int tmp = 0;

    if (!buf) {
        return -3;
    }
    if (2 > max) {
        return -4;
    }
    i = 1;
    tmp = value;
    if (0 > tmp) {
        tmp *= -1;
        i++;
    }
    for (;;) {
        tmp /= 10;
        if (0 >= tmp) {
            break;
        }
        i++;
    }
    if (i >= max) {
        buf[0] = '?';
        buf[1] = 0x0;
        return 2;
    }
    n = i;
    tmp = value;
    if (0 > tmp) {
        tmp *= -1;
    }
    buf[i--] = 0x0;
    for (;;) {
        buf[i--] = (tmp % 10) + '0';
        tmp /= 10;
        if (0 >= tmp) {
            break;
        }
    }
    if (-1 != i) {
        buf[i--] = '-';
    }
    return n;
}

static bool_t inRange(int16_t x, int16_t y)
{
    if ((x >= 0) && (x < SOKOBAN_FIELD_WIDTH) && (y >= 0) && (y < SOKOBAN_FIELD_HEIGHT)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static void printGameOver(void)
{
    //     gdispFillArea(JG10_TOTAL_FIELD_WIDTH, (gdispGetHeight()/2)-10, gdispGetWidth()-JG10_TOTAL_FIELD_WIDTH, 80, Black);
    if (sokobanGameOver) {
        gdispFillStringBox(0, 0, gdispGetStringWidth("Good job!", font), 18, "Good job!", font, White, Black, justifyCenter);
    }
}

static void sokobanPrintMoves(void)
{
    char pps_str[5];
    uitoa(sokobanMoves, pps_str, sizeof(pps_str));
    gdispFillStringBox(SOKOBAN_FIELD_WIDTH * SOKOBAN_CELL_WIDTH, ((SOKOBAN_FIELD_HEIGHT - 3) * SOKOBAN_CELL_HEIGHT) + 2, 40, 22, pps_str, font, White, Black, justifyCenter);
}

static void sokobanDoUndo(void)
{
    if (sokobanUndoCount > 0) {
        uint8_t lastUndo;
        if (sokobanUndoCurrentPos == 0) {
            lastUndo = SOKOBAN_UNDO_COUNT - 1;
        } else {
            lastUndo = sokobanUndoCurrentPos - 1;
        }
        // Now that we got undo position, we can actually do undo :D
        // As we collect moves that includes box moving, we need to move Character AND the boxugu
        if (sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] == SOKOBAN_GOAL || sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] == SOKOBAN_BOXGOAL) {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_GOAL;
        } else {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_FLOOR;
        }
        sokobanField[c2b(sokobanUndoList[lastUndo][1], sokobanUndoList[lastUndo][0])] = SOKOBAN_CHAR_S;
        sokobanCharPosX = sokobanUndoList[lastUndo][0];
        sokobanCharPosY = sokobanUndoList[lastUndo][1];
        sokobanMoves--;
        // Character done, now the box! :D
        if (sokobanCurrentLvl[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2])] == SOKOBAN_GOAL ||
                sokobanCurrentLvl[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2])] == SOKOBAN_BOXGOAL) {
            sokobanField[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2])] = SOKOBAN_BOXGOAL;
            sokobanBoxCount--;
        } else {
            sokobanField[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2])] = SOKOBAN_BOX;
        }
        if (sokobanCurrentLvl[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2] + sokobanUndoList[lastUndo][2])] == SOKOBAN_GOAL ||
                sokobanCurrentLvl[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2] + sokobanUndoList[lastUndo][2])] == SOKOBAN_BOXGOAL) {
            sokobanField[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2] + sokobanUndoList[lastUndo][2])] = SOKOBAN_GOAL;
            sokobanBoxCount++;
        } else {
            sokobanField[c2b(sokobanUndoList[lastUndo][1] + sokobanUndoList[lastUndo][3] + sokobanUndoList[lastUndo][3], sokobanUndoList[lastUndo][0] + sokobanUndoList[lastUndo][2] + sokobanUndoList[lastUndo][2])] = SOKOBAN_FLOOR;
        }
        sokobanUndoCurrentPos = lastUndo;
        sokobanUndoCount--;
        sokobanPrintMoves();
        gwinRedraw(mainWin);
    }
}

static void sokobanAdd2UndoList(int8_t dirX, int8_t dirY)
{
    // Add chars positions + direction to undo history list
    sokobanUndoList[sokobanUndoCurrentPos][0] = sokobanCharPosX;
    sokobanUndoList[sokobanUndoCurrentPos][1] = sokobanCharPosY;
    sokobanUndoList[sokobanUndoCurrentPos][2] = dirX;
    sokobanUndoList[sokobanUndoCurrentPos][3] = dirY;
    if (sokobanUndoCount < SOKOBAN_UNDO_COUNT) {
        sokobanUndoCount++;
    }
    sokobanUndoCurrentPos++;
    if (sokobanUndoCurrentPos == SOKOBAN_UNDO_COUNT) {
        sokobanUndoCurrentPos = 0;
    }
}

static uint8_t sokobanCoordsToDir(int8_t x, int8_t y)
{
    if (x == -1 && y == 0) {
        return SOKOBAN_CHAR_W;
    }
    if (x == 1 && y == 0) {
        return SOKOBAN_CHAR_E;
    }
    if (x == 0 && y == -1) {
        return SOKOBAN_CHAR_N;
    }
    if (x == 0 && y == 1) {
        return SOKOBAN_CHAR_S;
    }
    return SOKOBAN_CHAR_S;
}

static void sokobanMakeTurn(int8_t x, int8_t y)
{
    sokobanCharDir = sokobanCoordsToDir(x, y);
    if (inRange(sokobanCharPosX + x, sokobanCharPosY + y) &&
            (sokobanField[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_BOX || sokobanField[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_BOXGOAL) &&
            inRange(sokobanCharPosX + x + x, sokobanCharPosY + y + y) &&
            (sokobanField[c2b(sokobanCharPosY + y + y, sokobanCharPosX + x + x)] == SOKOBAN_FLOOR || sokobanField[c2b(sokobanCharPosY + y + y, sokobanCharPosX + x + x)] == SOKOBAN_GOAL)) {
        if (sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] != SOKOBAN_GOAL && sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] != SOKOBAN_BOXGOAL) {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_FLOOR;
        } else {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_GOAL;
        }
        if (sokobanCurrentLvl[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_GOAL || sokobanCurrentLvl[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_BOXGOAL) {
            sokobanBoxCount++;
        }
        if (sokobanField[c2b(sokobanCharPosY + y + y, sokobanCharPosX + x + x)] == SOKOBAN_GOAL) {
            sokobanBoxCount--;
            sokobanField[c2b(sokobanCharPosY + y + y, sokobanCharPosX + x + x)] = SOKOBAN_BOXGOAL;
        } else {
            sokobanField[c2b(sokobanCharPosY + y + y, sokobanCharPosX + x + x)] = SOKOBAN_BOX;
        }
        if (sokobanBoxCount == 0) {
            sokobanGameOver = TRUE;
        }
        sokobanAdd2UndoList(x, y);
        sokobanCharPosX += x;
        sokobanCharPosY += y;
        sokobanMoves++;
    } else if (inRange(sokobanCharPosX + x, sokobanCharPosY + y) &&
               (sokobanField[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_FLOOR || sokobanField[c2b(sokobanCharPosY + y, sokobanCharPosX + x)] == SOKOBAN_GOAL)) {
        if ((sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] != SOKOBAN_GOAL) && (sokobanCurrentLvl[c2b(sokobanCharPosY, sokobanCharPosX)] != SOKOBAN_BOXGOAL)) {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_FLOOR;
        } else {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_GOAL;
        }
        sokobanCharPosX += x;
        sokobanCharPosY += y;
        sokobanMoves++;
    }
    sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = sokobanCharDir;
    if (sokobanMoves > 9999) {
        sokobanMoves = 9999;
    }
    sokobanPrintMoves();
    gwinRedraw(mainWin);
}

static void initField(void)
{
    char pps_str[3];
    sokobanGameOver = FALSE;
    sokobanBoxCount = 0;
    sokobanMoves = 0;
    sokobanUndoCurrentPos = 0;
    sokobanUndoCount = 0;
    for (uint8_t c = 0; c < SOKOBAN_FIELD_WIDTH * SOKOBAN_FIELD_HEIGHT; c++) {
        switch (sokobanCurrentLvl[c]) {
        case SOKOBAN_BOX:
            sokobanBoxCount++;
            break;
        case SOKOBAN_CHAR_E:
        case SOKOBAN_CHAR_W:
        case SOKOBAN_CHAR_S:
        case SOKOBAN_CHAR_N:
            sokobanCharPosX = b2x(c);
            sokobanCharPosY = b2y(c);
            sokobanCharDir = sokobanCurrentLvl[c];
            break;
        }
        sokobanField[c] = sokobanCurrentLvl[c];
    }
    uitoa(sokobanCurrentLvlNumber + 1, pps_str, sizeof(pps_str));
    gdispFillStringBox(SOKOBAN_FIELD_WIDTH * SOKOBAN_CELL_WIDTH, ((SOKOBAN_FIELD_HEIGHT - 1) * SOKOBAN_CELL_HEIGHT) + 2, 40, 22, pps_str, font, White, Black, justifyCenter);
    gdispImageOpenFile(&sokobanButtons, "level.bmp");
    gdispImageDraw(&sokobanButtons, SOKOBAN_FIELD_WIDTH * SOKOBAN_CELL_WIDTH, ((SOKOBAN_FIELD_HEIGHT - 1) * SOKOBAN_CELL_HEIGHT) - 10, 40, 9, 0, 0);
    gdispImageClose(&sokobanButtons);
    sokobanPrintMoves();
    gdispImageOpenFile(&sokobanButtons, "moves.bmp");
    gdispImageDraw(&sokobanButtons, SOKOBAN_FIELD_WIDTH * SOKOBAN_CELL_WIDTH, ((SOKOBAN_FIELD_HEIGHT - 3) * SOKOBAN_CELL_HEIGHT) - 10, 40, 9, 0, 0);
    gdispImageClose(&sokobanButtons);
}

static void sokobanNextLevel(void)
{
    sokobanCurrentLvlNumber++;
    if (sokobanCurrentLvlNumber >= SOKOBAN_LEVEL_COUNT) {
        sokobanCurrentLvlNumber = 0;
    }
}

static void sokobanPrevLevel(void)
{
    if (sokobanCurrentLvlNumber > 0) {
        sokobanCurrentLvlNumber--;
    } else {
        sokobanCurrentLvlNumber = SOKOBAN_LEVEL_COUNT - 1;
    }
}

static void sokobanCalcTouchRegion(uint16_t y, int16_t x)
{
    if (sokobanGameOver) {
        return;
    }
    if (x >= SOKOBAN_TOUCH_WIDTH_X && x < gdispGetWidth() - SOKOBAN_TOUCH_WIDTH_X && y < SOKOBAN_TOUCH_WIDTH_Y) {
        // Up
        sokobanMakeTurn(0, -1);
    } else if (x >= SOKOBAN_TOUCH_WIDTH_X && y >= gdispGetHeight() - SOKOBAN_TOUCH_WIDTH_Y && x < gdispGetWidth() - SOKOBAN_TOUCH_WIDTH_X && y < gdispGetHeight()) {
        // Down
        sokobanMakeTurn(0, 1);
    } else if (x >= 0 && x < SOKOBAN_TOUCH_WIDTH_X && y < gdispGetHeight()) {
        // Left
        sokobanMakeTurn(-1, 0);
    } else if (x >= gdispGetWidth() - SOKOBAN_TOUCH_WIDTH_X && x < gdispGetWidth() && y < gdispGetHeight()) {
        if (x >= gdispGetWidth() - 30 && x < gdispGetWidth() && y < 30) {
            // Reset/Restart lvl
            initField();
            gwinRedraw(mainWin);
        } else if (x >= gdispGetWidth() - 30 && y > 30 && x < gdispGetWidth() && y <= 60) {
            // Next lvl
            sokobanNextLevel();
            sokobanCurrentLvl = lvl[sokobanCurrentLvlNumber].lvlField;
            initField();
            gwinRedraw(mainWin);
        } else if (x >= gdispGetWidth() - 30 && y > 60 && x < gdispGetWidth() && y <= 90) {
            // Prev lvl
            sokobanPrevLevel();
            sokobanCurrentLvl = lvl[sokobanCurrentLvlNumber].lvlField;
            initField();
            gwinRedraw(mainWin);
        } else if (x >= gdispGetWidth() - 30 && y > 90 && x < gdispGetWidth() && y <= 120) {
            // Undo
            sokobanDoUndo();
        } else {
            // Right
            sokobanMakeTurn(1, 0);
        }
    } else {
        // Middle
    }
    sokobanLastMove = gfxSystemTicks();
}

static DECLARE_THREAD_FUNCTION(thdSokoban, msg)
{
    (void)msg;
    while (!sokobanGameOver) {
        ginputGetMouseStatus(0, &ev);
        if (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
            sokobanCalcTouchRegion(ev.y, ev.x);
            sokobanTouchRepeat = gfxSystemTicks();
            while (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
                // Wait until release
                ginputGetMouseStatus(0, &ev);
                if (gfxSystemTicks() - sokobanTouchRepeat >= SOKOBAN_TOUCH_TIMEOUT) {
                    sokobanCalcTouchRegion(ev.y, ev.x);
                    sokobanTouchRepeat = gfxSystemTicks();
                }
            }
            //sokobanCalcTouchRegion(ev.y, ev.x);
        }
        if ((sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] != SOKOBAN_CHAR_S) && (gfxSystemTicks() - sokobanLastMove >= SOKOBAN_CHAR_TIMEOUT)) {
            sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_CHAR_S;
            gwinRedraw(mainWin);
        }
    }
    THREAD_RETURN(0);
}

static void mainWinDraw(GWidgetObject *gw, void *param)
{
    for (uint8_t c = 0; c < SOKOBAN_FIELD_WIDTH * SOKOBAN_FIELD_HEIGHT; c++) {
        if (sokobanField[c] >= 1) {
            gdispGImageDraw(gw->g.display, &sokobanImage[sokobanField[c] - 1], (b2x(c) * SOKOBAN_CELL_HEIGHT) + 1, (b2y(c) * SOKOBAN_CELL_WIDTH) + 1, SOKOBAN_CELL_WIDTH, SOKOBAN_CELL_HEIGHT, 0, 0);
        } else {
            gdispGFillArea(gw->g.display, (b2x(c) * SOKOBAN_CELL_HEIGHT) + 1, (b2y(c) * SOKOBAN_CELL_WIDTH) + 1, SOKOBAN_CELL_WIDTH, SOKOBAN_CELL_HEIGHT, Black);
        }
    }
}

static void createMainWin(void)
{
    GWidgetInit wi;
    gwinWidgetClearInit(&wi);
    // Container - mainWin
    wi.g.show = FALSE;
    wi.g.x = 0;
    wi.g.y = 0;
    wi.g.width = SOKOBAN_FIELD_WIDTH * SOKOBAN_CELL_WIDTH;
    wi.g.height = SOKOBAN_FIELD_HEIGHT * SOKOBAN_CELL_HEIGHT;
    wi.g.parent = 0;
    wi.text = "Container";
    wi.customDraw = mainWinDraw;
    wi.customParam = 0;
    wi.customStyle = 0;
    mainWin = gwinContainerCreate(0, &wi, 0);
}

void guiCreate(void)
{
    GWidgetInit wi;
    gwinWidgetClearInit(&wi);
    createMainWin();
    gwinHide(mainWin);
    gwinShow(mainWin);
}

void sokobanStart(void)
{
#ifdef SOKOBAN_SHOW_SPLASH
    gtimerStop(&sokobanSplashBlink);
    gdispClear(Black);
#endif
    sokobanCurrentLvl = lvl[sokobanCurrentLvlNumber].lvlField;
    initField();
    guiCreate();
    gdispImageOpenFile(&sokobanButtons, "reset.bmp");
    gdispImageDraw(&sokobanButtons, gdispGetWidth() - 30, 0, 30, 30, 0, 0);
    gdispImageClose(&sokobanButtons);
    gdispImageOpenFile(&sokobanButtons, "next.bmp");
    gdispImageDraw(&sokobanButtons, gdispGetWidth() - 30, 30, 30, 30, 0, 0);
    gdispImageClose(&sokobanButtons);
    gdispImageOpenFile(&sokobanButtons, "back.bmp");
    gdispImageDraw(&sokobanButtons, gdispGetWidth() - 30, 60, 30, 30, 0, 0);
    gdispImageClose(&sokobanButtons);
    gdispImageOpenFile(&sokobanButtons, "undo.bmp");
    gdispImageDraw(&sokobanButtons, gdispGetWidth() - 30, 90, 30, 30, 0, 0);
    gdispImageClose(&sokobanButtons);
    gfxThreadHandle mainTh = gfxThreadCreate(0, 1024 * 4, NORMAL_PRIORITY, thdSokoban, 0);
    while (!sokobanGameOver) {
        gfxSleepMilliseconds(100);
    }
    gfxThreadWait(mainTh);
    sokobanField[c2b(sokobanCharPosY, sokobanCharPosX)] = SOKOBAN_CHAR_S;
    gwinRedraw(mainWin);
    printGameOver();
    sokobanNextLevel();
}

void sokobanInit(void)
{
    for (uint8_t i = 0; i < 9; i++) {
        gdispImageOpenFile(&sokobanImage[i], sokobanGraph[i]);
        gdispImageCache(&sokobanImage[i]);
    }
    sokobanInitLvlStructs();
    // font = gdispOpenFont("DejaVuSans16_aa");
    // font = gdispOpenFont("*");
}

#ifdef SOKOBAN_SHOW_SPLASH
static void sokobanSplashBlinker(void *arg)
{
    (void)arg;
    sokobanSplashTxtVisible = !sokobanSplashTxtVisible;
    if (sokobanSplashTxtVisible) {
        gdispImageOpenFile(&sokobanSplashImage, "splashtxt.bmp");
    } else {
        gdispImageOpenFile(&sokobanSplashImage, "splashclr.bmp");
    }
    gdispImageDraw(&sokobanSplashImage, (gdispGetWidth() / 2) - 150 + 101, (gdispGetHeight() / 2) - 100 + 167, 98, 11, 0, 0);
    gdispImageClose(&sokobanSplashImage);
}

void sokobanShowSplash(void)
{
    gdispImageOpenFile(&sokobanSplashImage, "splash.bmp");
    gdispImageDraw(&sokobanSplashImage, (gdispGetWidth() / 2) - 150, (gdispGetHeight() / 2) - 100, 300, 200, 0, 0);
    gdispImageClose(&sokobanSplashImage);
    gtimerStart(&sokobanSplashBlink, sokobanSplashBlinker, 0, TRUE, 400);
}
#endif