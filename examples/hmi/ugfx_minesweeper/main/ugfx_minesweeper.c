/*
 * Copyright (c) 2012, 2013, Joel Bodenmann aka Tectu <joel@unormal.org>
 * Copyright (c) 2012, 2013, Andrew Hannam aka inmarket
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include "esp_log.h"
#include "iot_ugfx.h"
#include "mines.h"

typedef struct {
    // Node properties
    uint8_t num;       // Node number, how many mines around
    bool_t open;       // Node shown or hidden
    bool_t check;      // Node needs to be checked or not, used for opening up empty nodes
    bool_t flag;       // Node is marked with flag by player
    uint16_t fieldNum; // Node number, used to randomize gamestart "animation"
} nodeProps;

static GEventMouse ev;
static nodeProps minesField[MINES_FIELD_WIDTH][MINES_FIELD_HEIGHT]; // Mines field array
static bool_t minesGameOver = FALSE;
static bool_t minesGameWinner = FALSE;
static int16_t minesEmptyNodes; // Empty node counter
static int16_t minesFlags;      // Flag counter
static int16_t minesTime;       // Time counter
static GTimer minesTimeCounterTimer;
static const char *minesGraph[] = {"1.bmp", "2.bmp", "3.bmp", "4.bmp", "5.bmp", "6.bmp", "7.bmp", "8.bmp", "closed.bmp", "empty.bmp", "explode.bmp", "flag.bmp", "mine.bmp", "wrong.bmp"}; // 14 elements (0-13)
static gdispImage minesImage;
static gdispImage minesImage1;
static uint8_t minesStatusIconWidth = 0;
static uint8_t minesStatusIconHeight = 0;
static bool_t minesFirstGame = TRUE; // Just don't clear field for the first time, as we have black screen already... :/
static bool_t minesSplashTxtVisible = FALSE;

#if MINES_SHOW_SPLASH
static GTimer minesSplashBlink;
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

static void initRng(void)
{
    srand(gfxSystemTicks());
}

static uint32_t randomInt(uint32_t max)
{
    return rand() % max;
}

static void printStats(void)
{
    char pps_str[12];

    font_t font = gdispOpenFont("fixed_5x8");
    uitoa(MINES_MINE_COUNT, pps_str, sizeof(pps_str));
    gdispFillString(minesStatusIconWidth + 8, gdispGetHeight() - 11, "    ", font, Black, Black);
    gdispDrawString(minesStatusIconWidth + 8, gdispGetHeight() - 11, pps_str, font, White);
    uitoa(minesFlags, pps_str, sizeof(pps_str));
    gdispFillString(8 + (minesStatusIconWidth * 2) + gdispGetStringWidth("99999", font), gdispGetHeight() - 11, "    ", font, Black, Black);
    gdispDrawString(8 + (minesStatusIconWidth * 2) + gdispGetStringWidth("99999", font), gdispGetHeight() - 11, pps_str, font, White);
    gdispCloseFont(font);
}

static void minesUpdateTime(void)
{
    char pps_str[12];

    if (minesTime > 9999) {
        minesTime = 9999;
    }

    font_t font = gdispOpenFont("digital_7__mono_20");
    uitoa(minesTime, pps_str, sizeof(pps_str));
    gdispFillArea((MINES_FIELD_WIDTH * MINES_CELL_WIDTH) - gdispGetStringWidth("9999", font), gdispGetHeight() - 15, gdispGetWidth(), 15, Black);
    gdispDrawString((MINES_FIELD_WIDTH * MINES_CELL_WIDTH) - gdispGetStringWidth(pps_str, font), gdispGetHeight() - 15, pps_str, font, Lime);
    gdispCloseFont(font);
}

static void minesTimeCounter(void *arg)
{
    (void)arg;

    minesTime++;
    minesUpdateTime();
}

static bool_t inRange(int16_t x, int16_t y)
{
    if ((x >= 0) && (x < MINES_FIELD_WIDTH) && (y >= 0) && (y < MINES_FIELD_HEIGHT)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static void showOne(int16_t x, int16_t y)
{
    minesField[x][y].open = TRUE;
    if (minesField[x][y].flag) {
        minesField[x][y].flag = FALSE;
        minesFlags--;
    }

    gdispFillArea((x * MINES_CELL_WIDTH) + 1, (y * MINES_CELL_HEIGHT) + 1, MINES_CELL_WIDTH - 1, MINES_CELL_HEIGHT - 1, Black);

    if ((minesField[x][y].num > 0) && (minesField[x][y].num < 9)) {
        gdispImageOpenFile(&minesImage, minesGraph[minesField[x][y].num - 1]);
        gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
        gdispImageClose(&minesImage);
        minesEmptyNodes--;
    } else if (minesField[x][y].num == 9) {
        minesGameOver = TRUE;
        minesGameWinner = FALSE;
        gdispImageOpenFile(&minesImage, minesGraph[10]);
        gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
        gdispImageClose(&minesImage);
        // Dirty HACK to not draw mine icon on GameOver event :D
        minesField[x][y].num = 0;
    } else if (minesField[x][y].num == 0) {
        gdispImageOpenFile(&minesImage, minesGraph[9]);
        gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
        gdispImageClose(&minesImage);
        minesField[x][y].check = TRUE;
        minesEmptyNodes--;
    }
}

static void openEmptyNodes(void)
{
    int16_t x, y, i, j;
    bool_t needToCheck = TRUE;

    while (needToCheck) {
        needToCheck = FALSE;
        for (x = 0; x < MINES_FIELD_WIDTH; x++) {
            for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
                if (minesField[x][y].check) {
                    for (i = -1; i <= 1; i++) {
                        for (j = -1; j <= 1; j++) {
                            if ((i != 0) || (j != 0)) {
                                // We don't need to check middle node as it is the one we are checking right now! :D
                                if (inRange(x + i, y + j)) {
                                    if (!minesField[x + i][y + j].open) {
                                        showOne(x + i, y + j);
                                    }
                                    if (minesField[x + i][y + j].check) {
                                        needToCheck = TRUE;
                                    }
                                }
                            }
                        }
                    }
                    minesField[x][y].check = FALSE;
                }
            }
        }
    }
}

static DECLARE_THREAD_FUNCTION(thdMines, msg)
{
    (void)msg;
    uint16_t x, y, delay;
    bool_t delayed = FALSE;
    while (!minesGameOver) {
        if (minesEmptyNodes == 0) {
            minesGameOver = TRUE;
            minesGameWinner = TRUE;
        }
        initRng();
        ginputGetMouseStatus(0, &ev);
        delayed = FALSE;
        if (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
            x = ev.x / MINES_CELL_WIDTH;
            y = ev.y / MINES_CELL_WIDTH;
            delay = 0;
            while (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
                // Wait until release
                ginputGetMouseStatus(0, &ev);
                gfxSleepMilliseconds(1);
                delay++;
                if (delay >= MINES_FLAG_DELAY) {
                    delay = MINES_FLAG_DELAY;
                    if (!delayed && inRange(x, y) && !minesField[x][y].open) {
                        if (minesField[x][y].flag) {
                            gdispImageOpenFile(&minesImage, minesGraph[8]);
                            gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH - 1, MINES_CELL_HEIGHT - 1, 0, 0);
                            gdispImageClose(&minesImage);
                            minesField[x][y].flag = FALSE;
                            minesFlags--;
                            printStats();
                        } else {
                            gdispImageOpenFile(&minesImage, minesGraph[11]);
                            gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
                            gdispImageClose(&minesImage);
                            minesField[x][y].flag = TRUE;
                            minesFlags++;
                            printStats();
                        }
                        delayed = TRUE;
                    }
                }
            }
            // Check time, if longer than MINES_FLAG_DELAY then add flag...
            if (delay < MINES_FLAG_DELAY) {
                if ((x < MINES_FIELD_WIDTH) && (y < MINES_FIELD_HEIGHT) && !minesField[x][y].open && !minesField[x][y].flag) {
                    showOne(x, y);
                    openEmptyNodes();
                    printStats();
                }
            }
        }
    }
    THREAD_RETURN(0);
}

static void printGameOver(void)
{
    if (minesGameOver) {
        font_t font = gdispOpenFont("DejaVuSans16");
        if (minesGameWinner) {
            gdispDrawString((gdispGetWidth() - gdispGetStringWidth("You LIVE!", font)) / 2, gdispGetHeight() - 15, "You LIVE!", font, White);
        } else {
            gdispDrawString((gdispGetWidth() - gdispGetStringWidth("You DIED!", font)) / 2, gdispGetHeight() - 15, "You DIED!", font, White);
        }
        gdispCloseFont(font);
    } else {
        gdispFillArea(0, gdispGetHeight() - 25, gdispGetWidth(), 25, Black);
    }
}

static void initField(void)
{
    int16_t x, y, mines, i, j;

    minesFlags = 0;
    minesGameOver = FALSE;
    printGameOver();

    font_t font = gdispOpenFont("fixed_5x8");
    gdispImageOpenFile(&minesImage, "plainmine.bmp");
    // Saving status icons width/height for later use
    minesStatusIconWidth = minesImage.width;
    minesStatusIconHeight = minesImage.height;
    gdispImageDraw(&minesImage, 4, gdispGetHeight() - minesImage.height, minesImage.width, minesImage.height, 0, 0);
    gdispImageClose(&minesImage);
    gdispImageOpenFile(&minesImage, "plainflag.bmp");
    gdispImageDraw(&minesImage, 4 + minesImage.width + gdispGetStringWidth("99999", font), gdispGetHeight() - minesImage.height, minesImage.width, minesImage.height, 0, 0);
    gdispImageClose(&minesImage);
    gdispCloseFont(font);
    printStats();

    initRng();

    // Clearing/resetting field here...
    i = 0;
    for (x = 0; x < MINES_FIELD_WIDTH; x++) {
        for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
            minesField[x][y].num = 0;
            minesField[x][y].open = FALSE;
            minesField[x][y].check = FALSE;
            minesField[x][y].flag = FALSE;
            minesField[x][y].fieldNum = i;
            i++;
        }
    }

    // Randomizing closed field drawing...
    for (x = 0; x < MINES_FIELD_WIDTH; x++) {
        for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
            // Getting random node and swapping it with current
            i = randomInt(MINES_FIELD_WIDTH);
            j = randomInt(MINES_FIELD_HEIGHT);
            mines = minesField[x][y].fieldNum;
            minesField[x][y].fieldNum = minesField[i][j].fieldNum;
            minesField[i][j].fieldNum = mines;
        }
    }

    // Clearing nodes randomly
    if (!minesFirstGame) {
        for (x = 0; x < MINES_FIELD_WIDTH; x++) {
            for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
                i = minesField[x][y].fieldNum / MINES_FIELD_HEIGHT;
                j = minesField[x][y].fieldNum - (i * MINES_FIELD_HEIGHT);
                gdispFillArea((i * MINES_CELL_WIDTH) + 1, (j * MINES_CELL_HEIGHT) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, Black);
                gfxSleepMilliseconds(2);
            }
        }
    } else {
        minesFirstGame = FALSE;
    }

    // Drawing closed nodes randomly
    gdispImageOpenFile(&minesImage, minesGraph[8]);
    gdispImageCache(&minesImage);
    for (x = 0; x < MINES_FIELD_WIDTH; x++) {
        for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
            i = minesField[x][y].fieldNum / MINES_FIELD_HEIGHT;
            j = minesField[x][y].fieldNum - (i * MINES_FIELD_HEIGHT);
            gdispImageDraw(&minesImage, (i * MINES_CELL_HEIGHT) + 1, (j * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
            gfxSleepMilliseconds(2);
        }
    }
    gdispImageClose(&minesImage);
    minesEmptyNodes = MINES_FIELD_WIDTH * MINES_FIELD_HEIGHT;

    // Placing mines in random nodes :D
    mines = 0;
    while (mines != MINES_MINE_COUNT) {
        x = randomInt(MINES_FIELD_WIDTH);
        y = randomInt(MINES_FIELD_HEIGHT);
        if (minesField[x][y].num != 9) {
            mines++;
            minesEmptyNodes--;
            minesField[x][y].num = 9;
        }
    }

    // Calculating numbers for nearby mine nodes
    for (x = 0; x < MINES_FIELD_WIDTH; x++) {
        for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
            if (minesField[x][y].num != 9) {
                for (i = -1; i <= 1; i++) {
                    for (j = -1; j <= 1; j++) {
                        if ((i != 0) || (j != 0)) {
                            // We don't need to check middle node as we already know it is not a mine! :D
                            if (inRange(x + i, y + j) && (minesField[x + i][y + j].num == 9)) {
                                minesField[x][y].num++;
                            }
                        }
                    }
                }
            }
        }
    }

    minesTime = 0;
    minesUpdateTime();
    gtimerStart(&minesTimeCounterTimer, minesTimeCounter, 0, TRUE, 1000);
}

void minesStart(void)
{
    int16_t x, y;

#if MINES_SHOW_SPLASH
    gtimerStop(&minesSplashBlink);
    gdispClear(Black);
#endif

    initField();
    gfxThreadCreate(0, 1024, NORMAL_PRIORITY, thdMines, 0);
    while (!minesGameOver) {
        gfxSleepMilliseconds(100);
    }
    printGameOver();
    gtimerStop(&minesTimeCounterTimer);

    if (!minesGameWinner) {
        // Print generated mines for player to see
        font_t font = gdispOpenFont("fixed_10x20");
        gdispImageOpenFile(&minesImage, minesGraph[12]);
        gdispImageOpenFile(&minesImage1, minesGraph[13]);
        gdispImageCache(&minesImage);
        gdispImageCache(&minesImage1);

        for (x = 0; x < MINES_FIELD_WIDTH; x++) {
            for (y = 0; y < MINES_FIELD_HEIGHT; y++) {
                if (minesField[x][y].num == 9 && !minesField[x][y].flag) {
                    gdispImageDraw(&minesImage, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
                }
                if (minesField[x][y].flag && (minesField[x][y].num != 9)) {
                    gdispImageDraw(&minesImage1, (x * MINES_CELL_HEIGHT) + 1, (y * MINES_CELL_WIDTH) + 1, MINES_CELL_WIDTH, MINES_CELL_HEIGHT, 0, 0);
                }
            }
        }
        gdispImageClose(&minesImage);
        gdispImageClose(&minesImage1);
        gdispCloseFont(font);
    }
}

#if MINES_SHOW_SPLASH
static void minesSplashBlinker(void *arg)
{
    (void)arg;

    minesSplashTxtVisible = !minesSplashTxtVisible;

    if (minesSplashTxtVisible) {
        gdispImageOpenFile(&minesImage, "splashtxt.bmp");
    } else {
        gdispImageOpenFile(&minesImage, "splashclr.bmp");
    }

    gdispImageDraw(&minesImage, (gdispGetWidth() / 2) - 150 + 93, (gdispGetHeight() / 2) - 100 + 161, 112, 10, 0, 0);

    gdispImageClose(&minesImage);
}

void minesShowSplash(void)
{
    gdispImageOpenFile(&minesImage, "splash.bmp");
    gdispImageDraw(&minesImage, (gdispGetWidth() / 2) - 150, (gdispGetHeight() / 2) - 100, 300, 200, 0, 0);
    gdispImageClose(&minesImage);

    gtimerStart(&minesSplashBlink, minesSplashBlinker, 0, TRUE, 400);
}
#endif

void minesInit(void)
{
    initRng();

    gdispClear(Black);
}

void ugfx_minisweeper(void)
{
    GEventMouse ev;
#if !MINES_SHOW_SPLASH
    font_t font;
#endif

    gfxInit();
    ginputGetMouse(0);
    minesInit();

#if MINES_SHOW_SPLASH
    minesShowSplash();
#else
    font = gdispOpenFont("DejaVuSans16");
    gdispDrawString((gdispGetWidth() - gdispGetStringWidth("Touch to start!", font)) / 2, gdispGetHeight() - 25, "Touch to start!", font, White);
    gdispCloseFont(font);
#endif

    while (TRUE) {
        ginputGetMouseStatus(0, &ev);
        if (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
            while (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
                // Wait until release
                ginputGetMouseStatus(0, &ev);
            }

            minesStart();
        }
    }
}
