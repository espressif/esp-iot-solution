
#include "iot_ugfx.h"
#include "src/gwin/gwin_class.h"
#include "stdlib.h"
#include "string.h"
#include "jg10.h"

GEventMouse ev;
GHandle mainWin, Jg10SelectWidget;

typedef struct jg10WidgetObject_t {
    GWidgetObject w; // Base Class
} jg10WidgetObject;

GHandle jg10SelectionWidgetGCreate(GDisplay *g, jg10WidgetObject *wo, GWidgetInit *pInit);
#define jg10SelectionWidgetCreate(wo, pInit) jg10SelectionWidgetGCreate(GDISP, wo, pInit)

typedef struct {
    // Node properties
    uint8_t num;  // Node number
    bool_t check; // Node needs to be checked or not
    bool_t sel;   // Node selected or not
} nodeProps;

nodeProps jg10Field[JG10_FIELD_WIDTH][JG10_FIELD_HEIGHT]; // jg10 field array
bool_t jg10GameOver = FALSE;
const char *jg10Graph[] = {"background.bmp", "1.bmp", "2.bmp", "3.bmp", "4.bmp", "5.bmp", "6.bmp", "7.bmp", "8.bmp", "9.bmp", "10.bmp", "11.bmp", "12.bmp", "13.bmp", "14.bmp", "15.bmp", "16.bmp", "17.bmp", "18.bmp", "19.bmp", "20.bmp"}; // 21 elements (0-20)
gdispImage jg10Image[JG10_MAX_COUNT];
#define JG10_ANIM_IMAGES 5
#define JG10_ANIM_DELAY 60
const char *jg10GraphAnim[] = {"a1.bmp", "a2.bmp", "a3.bmp", "a4.bmp", "background.bmp"}; // 5 elements (0-4)
gdispImage jg10ImageAnim[JG10_ANIM_IMAGES];
uint8_t jg10MaxVal = 4; // Max value in field...
static font_t font;
#if JG10_SHOW_SPLASH
GTimer jg10SplashBlink;
bool_t jg10SplashTxtVisible = FALSE;
gdispImage jg10SplashImage;
#endif

static void initRng(void)
{
    srand(gfxSystemTicks());
}

static uint32_t randomInt(uint32_t max)
{
    return rand() % max;
}

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
    if ((x >= 0) && (x < JG10_FIELD_WIDTH) && (y >= 0) && (y < JG10_FIELD_HEIGHT)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static void clean_SelCheck(void)
{
    uint16_t i, j;
    for (i = 0; i < JG10_FIELD_WIDTH; i++) {
        for (j = 0; j < JG10_FIELD_HEIGHT; j++) {
            jg10Field[i][j].check = FALSE;
            jg10Field[i][j].sel = FALSE;
        }
    }
}

static void remove_Selected(void)
{
    uint16_t i, j, step;
    systemticks_t delay_start = 0;
    systemticks_t delay = 0;
    for (step = 0; step < JG10_ANIM_IMAGES; step++) {
        delay_start = gfxSystemTicks();
        for (i = 0; i < JG10_FIELD_WIDTH; i++) {
            for (j = 0; j < JG10_FIELD_HEIGHT; j++) {
                if (jg10Field[i][j].sel) {
                    gdispImageDraw(&jg10ImageAnim[step], (i * JG10_CELL_HEIGHT) + 1, (j * JG10_CELL_WIDTH) + 1, JG10_CELL_WIDTH, JG10_CELL_HEIGHT, 3, 3);
                }
            }
        }
        delay = gfxSystemTicks() - delay_start;
        if (delay <= JG10_ANIM_DELAY) {
            gfxSleepMilliseconds(JG10_ANIM_DELAY - delay);
        }
    }
    for (i = 0; i < JG10_FIELD_WIDTH; i++) {
        for (j = 0; j < JG10_FIELD_HEIGHT; j++) {
            if (jg10Field[i][j].sel) {
                jg10Field[i][j].sel = FALSE;
                jg10Field[i][j].num = 0;
            }
        }
    }
    //    gwinRedraw(mainWin);
}

static uint8_t jg10_randomer(uint8_t max, uint8_t th)
{
    uint32_t r = randomInt((1 << max) - 1);

    if (r != 0) {
        for (int8_t i = max; i >= 0; i--) {
            if (r >= (uint32_t)(1 << i)) {
                if ((max - i) >= th) {
                    return randomInt(max - i) + 1;
                } else {
                    return randomInt(th) + 1;
                }
            }
        }
    }
    return randomInt(max - 1) + 1;
}

static void movePiecesDown(void)
{
    uint8_t tmp = 0;
    bool_t needToCheck = TRUE;
    while (needToCheck) {
        needToCheck = FALSE;
        for (int8_t y = (JG10_FIELD_HEIGHT - 1); y >= 0; y--) {
            for (uint8_t x = 0; x < JG10_FIELD_WIDTH; x++) {
                if (jg10Field[x][y].num == 0) {
                    // check if there is at least single none empty piece
                    tmp = 0;
                    for (int8_t tmpy = y; tmpy >= 0; tmpy--) {
                        if (jg10Field[x][tmpy].num != 0) {
                            tmp++;
                        }
                    }
                    if (tmp != 0) {
                        for (int8_t tmpy = y; tmpy > 0; tmpy--) {
                            jg10Field[x][tmpy].num = jg10Field[x][tmpy - 1].num;
                        }
                        jg10Field[x][0].num = 0;
                        needToCheck = TRUE;
                    }
                }
            }
        }
    }
    gwinRedraw(mainWin);
    // Add new pieces
    needToCheck = TRUE;
    while (needToCheck) {
        needToCheck = FALSE;
        for (int8_t y = (JG10_FIELD_HEIGHT - 1); y >= 0; y--) {
            for (uint8_t x = 0; x < JG10_FIELD_WIDTH; x++) {
                if (jg10Field[x][y].num == 0) {
                    for (int8_t tmpy = y; tmpy > 0; tmpy--) {
                        jg10Field[x][tmpy].num = jg10Field[x][tmpy - 1].num;
                    }
                    jg10Field[x][0].num = jg10_randomer(jg10MaxVal, 3);
                    needToCheck = TRUE;
                }
            }
        }
        gwinRedraw(mainWin);
        gfxSleepMilliseconds(50);
    }
}

static bool_t checkForPossibleMove(void)
{
    bool_t canMove = FALSE;
    uint16_t i, j;
    for (i = 0; i < JG10_FIELD_WIDTH; i++) {
        for (j = 0; j < JG10_FIELD_HEIGHT; j++) {
            if ((inRange(i, j - 1) && jg10Field[i][j - 1].num == jg10Field[i][j].num) ||
                    (inRange(i - 1, j) && jg10Field[i - 1][j].num == jg10Field[i][j].num) ||
                    (inRange(i, j + 1) && jg10Field[i][j + 1].num == jg10Field[i][j].num) ||
                    (inRange(i + 1, j) && jg10Field[i + 1][j].num == jg10Field[i][j].num)) {
                canMove = TRUE;
                return canMove;
            }
        }
    }
    return canMove;
}

static void printGameOver(void)
{
    gdispFillArea(JG10_TOTAL_FIELD_WIDTH, (gdispGetHeight() / 2) - 10, gdispGetWidth() - JG10_TOTAL_FIELD_WIDTH, 80, Black);
    if (jg10GameOver) {
        gdispDrawString(JG10_TOTAL_FIELD_WIDTH + ((gdispGetWidth() - JG10_TOTAL_FIELD_WIDTH) / 2) - (gdispGetStringWidth("Game Over", font) / 2), gdispGetHeight() / 2, "Game Over", font, White);
        gfxSleepMilliseconds(1000);
    }
}

static void printCongrats(void)
{
    char pps_str[3];
    gdispFillArea(JG10_TOTAL_FIELD_WIDTH, (gdispGetHeight() / 2) - 10, gdispGetWidth() - JG10_TOTAL_FIELD_WIDTH, 80, Black);
    gdispDrawString(JG10_TOTAL_FIELD_WIDTH + ((gdispGetWidth() - JG10_TOTAL_FIELD_WIDTH) / 2) - (gdispGetStringWidth("Congrats, now try to get", font) / 2), gdispGetHeight() / 2, "Congrats, now try to get", font, White);
    uitoa(jg10MaxVal + 1, pps_str, sizeof(pps_str));
    gdispDrawString(JG10_TOTAL_FIELD_WIDTH + ((gdispGetWidth() - JG10_TOTAL_FIELD_WIDTH) / 2) - (gdispGetStringWidth(pps_str, font) / 2), (gdispGetHeight() / 2) + 20, pps_str, font, Red);
}

static DECLARE_THREAD_FUNCTION(thdJg10, msg)
{
    (void)msg;
    uint16_t x, y;
    while (!jg10GameOver) {
        srand(gfxSystemTicks());
        ginputGetMouseStatus(0, &ev);
        if (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
            x = ev.x / JG10_CELL_WIDTH;
            y = ev.y / JG10_CELL_HEIGHT;
            if (x < JG10_FIELD_WIDTH && y < JG10_FIELD_HEIGHT) {
                while (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
                    // Wait until release
                    ginputGetMouseStatus(0, &ev);
                }
                if (!jg10Field[x][y].sel) {
                    // Check if we have at least two
                    if ((inRange(x, y - 1) && jg10Field[x][y - 1].num == jg10Field[x][y].num) ||
                            (inRange(x - 1, y) && jg10Field[x - 1][y].num == jg10Field[x][y].num) ||
                            (inRange(x, y + 1) && jg10Field[x][y + 1].num == jg10Field[x][y].num) ||
                            (inRange(x + 1, y) && jg10Field[x + 1][y].num == jg10Field[x][y].num)) {
                        gwinSetVisible(Jg10SelectWidget, FALSE);
                        clean_SelCheck();
                        jg10Field[x][y].check = TRUE;
                        gwinSetVisible(Jg10SelectWidget, TRUE);
                    }
                } else {
                    // already selected section clicked...
                    jg10Field[x][y].num++;
                    if (jg10Field[x][y].num > jg10MaxVal) {
                        jg10MaxVal = jg10Field[x][y].num;
                        if (jg10MaxVal >= 10) {
                            printCongrats();
                        }
                        if (jg10MaxVal == 20) {
                            // Just in case someone got so far :D I cannot imaginge though
                            jg10GameOver = TRUE;
                            printGameOver();
                        }
                    }
                    jg10Field[x][y].sel = FALSE;
                    gwinSetVisible(Jg10SelectWidget, FALSE);
                    remove_Selected();
                    movePiecesDown();
                    if (checkForPossibleMove()) {
                        clean_SelCheck();
                        //gwinRedraw(mainWin);
                    } else {
                        jg10GameOver = TRUE;
                        printGameOver();
                    }
                }
            }
        }
    }
    THREAD_RETURN(0);
}

static void initField(void)
{
    jg10MaxVal = 4;
    for (uint8_t x = 0; x < JG10_FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < JG10_FIELD_HEIGHT; y++) {
            jg10Field[x][y].num = randomInt(jg10MaxVal) + 1;
            //jg10Field[x][y].num = 1;      // good for animation testing
            //jg10Field[x][y].num = x+x+5;    // good to get high score fast
            //jg10Field[x][y].num = x+y+5;  // good demo to check out pieces :D
            jg10Field[x][y].check = FALSE;
            jg10Field[x][y].sel = FALSE;
        }
    }
    jg10GameOver = FALSE;
    printGameOver();
}

static void mainWinDraw(GWidgetObject *gw, void *param)
{
    (void)param;

    for (uint8_t x = 0; x < JG10_FIELD_WIDTH; x++) {
        for (uint8_t y = 0; y < JG10_FIELD_HEIGHT; y++) {
            gdispGImageDraw(gw->g.display, &jg10Image[jg10Field[x][y].num], (x * JG10_CELL_HEIGHT) + 1, (y * JG10_CELL_WIDTH) + 1, JG10_CELL_WIDTH, JG10_CELL_HEIGHT, 3, 3);
        }
    }
}

static void jg10SelectionWidget_Draw(GWidgetObject *gw, void *param)
{
    int16_t x, y;
    bool_t needToCheck = TRUE;

    (void)param;

    while (needToCheck) {
        needToCheck = FALSE;
        for (x = 0; x < JG10_FIELD_WIDTH; x++) {
            for (y = 0; y < JG10_FIELD_HEIGHT; y++) {
                if (jg10Field[x][y].check && !jg10Field[x][y].sel) {
                    jg10Field[x][y].sel = TRUE;
                    jg10Field[x][y].check = FALSE;
                    // Up
                    if (inRange(x, y - 1) && !jg10Field[x][y - 1].sel && (jg10Field[x][y - 1].num == jg10Field[x][y].num)) {
                        jg10Field[x][y - 1].check = TRUE;
                        needToCheck = TRUE;
                    } else if (!inRange(x, y - 1) || (inRange(x, y - 1) && !jg10Field[x][y - 1].sel)) {
                        // We need longer line if this is wide corner inside shape
                        if (inRange(x + 1, y) && inRange(x + 1, y - 1) && (jg10Field[x][y].num == jg10Field[x + 1][y].num) && (jg10Field[x][y].num == jg10Field[x + 1][y - 1].num)) {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) + 1, (y * JG10_CELL_HEIGHT) + 1, JG10_CELL_WIDTH + 2, 2, JG10_SEL_COLOR);
                        } else {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) + 1, (y * JG10_CELL_HEIGHT) + 1, JG10_CELL_WIDTH, 2, JG10_SEL_COLOR);
                        }
                    }
                    // Down
                    if (inRange(x, y + 1) && !jg10Field[x][y + 1].sel && (jg10Field[x][y + 1].num == jg10Field[x][y].num)) {
                        jg10Field[x][y + 1].check = TRUE;
                        needToCheck = TRUE;
                    } else if (!inRange(x, y + 1) || (inRange(x, y + 1) && !jg10Field[x][y + 1].sel)) {
                        // We need longer line if this is wide corner inside shape
                        if (inRange(x - 1, y) && inRange(x - 1, y + 1) && (jg10Field[x][y].num == jg10Field[x - 1][y].num) && (jg10Field[x][y].num == jg10Field[x - 1][y + 1].num)) {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) - 1, ((y + 1) * JG10_CELL_HEIGHT) - 1, JG10_CELL_WIDTH + 2, 2, JG10_SEL_COLOR);
                        } else {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) + 1, ((y + 1) * JG10_CELL_HEIGHT) - 1, JG10_CELL_WIDTH, 2, JG10_SEL_COLOR);
                        }
                    }
                    // Left
                    if (inRange(x - 1, y) && !jg10Field[x - 1][y].sel && (jg10Field[x - 1][y].num == jg10Field[x][y].num)) {
                        jg10Field[x - 1][y].check = TRUE;
                        needToCheck = TRUE;
                    } else if (!inRange(x - 1, y) || (inRange(x - 1, y) && !jg10Field[x - 1][y].sel)) {
                        // We need longer line if this is wide corner inside shape
                        if (inRange(x, y - 1) && inRange(x - 1, y - 1) && (jg10Field[x][y].num == jg10Field[x][y - 1].num) && (jg10Field[x][y].num == jg10Field[x - 1][y - 1].num)) {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) + 1, (y * JG10_CELL_HEIGHT) - 1, 2, JG10_CELL_HEIGHT + 2, JG10_SEL_COLOR);
                        } else {
                            gdispGFillArea(gw->g.display, (x * JG10_CELL_WIDTH) + 1, (y * JG10_CELL_HEIGHT) + 1, 2, JG10_CELL_HEIGHT, JG10_SEL_COLOR);
                        }
                    }
                    // Right
                    if (inRange(x + 1, y) && !jg10Field[x + 1][y].sel && (jg10Field[x + 1][y].num == jg10Field[x][y].num)) {
                        jg10Field[x + 1][y].check = TRUE;
                        needToCheck = TRUE;
                    } else if (!inRange(x + 1, y) || (inRange(x + 1, y) && !jg10Field[x + 1][y].sel)) {
                        // We need longer line if this is wide corner inside shape
                        if (inRange(x, y + 1) && inRange(x + 1, y + 1) && (jg10Field[x][y].num == jg10Field[x][y + 1].num) && (jg10Field[x][y].num == jg10Field[x + 1][y + 1].num)) {
                            gdispGFillArea(gw->g.display, ((x + 1) * JG10_CELL_WIDTH) - 1, (y * JG10_CELL_HEIGHT) + 1, 2, JG10_CELL_HEIGHT + 2, JG10_SEL_COLOR);
                        } else {
                            gdispGFillArea(gw->g.display, ((x + 1) * JG10_CELL_WIDTH) - 1, (y * JG10_CELL_HEIGHT) + 1, 2, JG10_CELL_HEIGHT, JG10_SEL_COLOR);
                        }
                    }
                }
            }
        }
    }
}

static const gwidgetVMT jg10SelectionWidgetVMT = {
    {
        "jg10SelectionWidget",    // The classname
        sizeof(jg10WidgetObject), // The object size
        _gwidgetDestroy,          // The destroy routine
        _gwidgetRedraw,           // The redraw routine
        0,                        // The after-clear routine
    },
    jg10SelectionWidget_Draw, // The default drawing routine
#if GINPUT_NEED_MOUSE
    {
        0, // Process mouse down events
        0, // Process mouse up events
        0, // Process mouse move events
    },
#endif
#if GINPUT_NEED_KEYBOARD || GWIN_NEED_KEYBOARD
    {
        0 // Process keyboard events
    },
#endif
#if GINPUT_NEED_TOGGLE
    {
        0, // Toggle role
        0, // Assign Toggles
        0, // Get Toggles
        0, // Process toggle off events
        0, // Process toggle on events
    },
#endif
#if GINPUT_NEED_DIAL
    {
        0, // No dial roles
        0, // Assign Dials
        0, // Get Dials
        0, // Process dial move events
    },
#endif
};

GHandle jg10SelectionWidgetGCreate(GDisplay *g, jg10WidgetObject *wo, GWidgetInit *pInit)
{
    if (!(wo = (jg10WidgetObject *)_gwidgetCreate(g, &wo->w, pInit, &jg10SelectionWidgetVMT))) {
        return 0;
    }
    gwinSetVisible((GHandle)wo, pInit->g.show);
    return (GHandle)wo;
}

static void createMainWin(void)
{
    GWidgetInit wi;
    gwinWidgetClearInit(&wi);
    // Container - mainWin
    wi.g.show = FALSE;
    wi.g.x = 0;
    wi.g.y = 0;
    wi.g.width = gdispGetWidth();
    wi.g.height = gdispGetHeight();
    wi.g.parent = 0;
    wi.text = "Container";
    wi.customDraw = mainWinDraw;
    wi.customParam = 0;
    wi.customStyle = 0;
    mainWin = gwinContainerCreate(0, &wi, 0);

    // create selection widget
    wi.g.show = FALSE;
    wi.g.x = 0;
    wi.g.y = 0;
    wi.g.width = 272;
    wi.g.height = 480;
    wi.g.parent = mainWin;
    wi.customDraw = jg10SelectionWidget_Draw;
    Jg10SelectWidget = jg10SelectionWidgetCreate(0, &wi);
}

void guiCreate(void)
{
    GWidgetInit wi;
    gwinWidgetClearInit(&wi);
    createMainWin();
    gwinHide(mainWin);
    gwinShow(mainWin);
}

void jg10Start(void)
{
#if JG10_SHOW_SPLASH
    gtimerStop(&jg10SplashBlink);
    gdispClear(Black);
#endif
    initField();
    guiCreate();
    gfxThreadCreate(0, 1024, NORMAL_PRIORITY, thdJg10, 0);
    while (!jg10GameOver) {
        gfxSleepMilliseconds(100);
    }
}

void jg10Init(void)
{
    initRng();
    for (uint8_t i = 0; i < JG10_MAX_COUNT; i++) {
        gdispImageOpenFile(&jg10Image[i], jg10Graph[i]);
        gdispImageCache(&jg10Image[i]);
    }
    for (uint8_t i = 0; i < JG10_ANIM_IMAGES; i++) {
        gdispImageOpenFile(&jg10ImageAnim[i], jg10GraphAnim[i]);
        gdispImageCache(&jg10ImageAnim[i]);
    }
    font = gdispOpenFont("DejaVuSans16_aa");
}

#if JG10_SHOW_SPLASH
static void jg10SplashBlinker(void *arg)
{
    (void)arg;
    jg10SplashTxtVisible = !jg10SplashTxtVisible;
    if (jg10SplashTxtVisible) {
        gdispImageOpenFile(&jg10SplashImage, "splashtxt.bmp");
    } else {
        gdispImageOpenFile(&jg10SplashImage, "splashclr.bmp");
    }
    gdispImageDraw(&jg10SplashImage, (gdispGetWidth() / 2) - 150 + 106, (gdispGetHeight() / 2) - 100 + 168, 89, 9, 0, 0);
    gdispImageClose(&jg10SplashImage);
}

void jg10ShowSplash(void)
{
    gdispImageOpenFile(&jg10SplashImage, "splash.bmp");
    gdispImageDraw(&jg10SplashImage, (gdispGetWidth() / 2) - 150, (gdispGetHeight() / 2) - 100, 300, 200, 0, 0);
    gdispImageClose(&jg10SplashImage);
    gtimerStart(&jg10SplashBlink, jg10SplashBlinker, 0, TRUE, 400);
}
#endif