/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */

#include "Adafruit_GFX_AS.h"

//Default font file
#include "fonts/glcdfont.h"

//Debug purpose
#include "esp_log.h"

extern "C" {
#include "crypto/common.h"      //Add missing cpp gaurds
}

#define abs(x) ((x)<0 ? -(x) : (x))
#define swap(x, y) do { typeof(x) temp##x##y = x; x = y; y = temp##x##y; } while (0)

Adafruit_GFX_AS::Adafruit_GFX_AS(int16_t w, int16_t h): WIDTH(w), HEIGHT(h)
{
    _width    = WIDTH;
    _height   = HEIGHT;
    rotation  = 0;
    cursor_y  = cursor_x    = 0;
    textsize  = 1;
    textcolor = textbgcolor = 0xFFFF;
    wrap      = true;
    gfxFont   = NULL;
}

Adafruit_GFX_AS::~Adafruit_GFX_AS()
{

}

// Draw a circle outline
void Adafruit_GFX_AS::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    drawPixel(x0, y0 + r, color);
    drawPixel(x0, y0 - r, color);
    drawPixel(x0 + r, y0, color);
    drawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 - y, y0 - x, color);
    }
}

void Adafruit_GFX_AS::drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
    int16_t f     = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x     = 0;
    int16_t y     = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f     += ddF_y;
        }
        x++;
        ddF_x += 2;
        f     += ddF_x;
        if (cornername & 0x4) {
            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 + y, y0 + x, color);
        }
        if (cornername & 0x2) {
            drawPixel(x0 + x, y0 - y, color);
            drawPixel(x0 + y, y0 - x, color);
        }
        if (cornername & 0x8) {
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 - x, y0 + y, color);
        }
        if (cornername & 0x1) {
            drawPixel(x0 - y, y0 - x, color);
            drawPixel(x0 - x, y0 - y, color);
        }
    }
}

void Adafruit_GFX_AS::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    drawFastVLine(x0, y0 - r, 2 * r + 1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void Adafruit_GFX_AS::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
    int16_t f     = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x     = 0;
    int16_t y     = r;

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f     += ddF_y;
        }
        x++;
        ddF_x += 2;
        f     += ddF_x;

        if (cornername & 0x1) {
            drawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
            drawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
        }
        if (cornername & 0x2) {
            drawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
            drawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
        }
    }
}

// Bresenham's algorithm - thx wikpedia
void Adafruit_GFX_AS::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (; x0 <= x1; x0++) {
        if (steep) {
            drawPixel(y0, x0, color);
        } else {
            drawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void Adafruit_GFX_AS::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void Adafruit_GFX_AS::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    // Update in subclasses if desired!
    // drawLine(x, y, x, y + h - 1, color);
}

void Adafruit_GFX_AS::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    // Update in subclasses if desired!
    // drawLine(x, y, x + w - 1, y, color);
}

void Adafruit_GFX_AS::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    // Update in subclasses if desired!
    for (int16_t i = x; i < x + w; i++) {
        drawFastVLine(i, y, h, color);
    }
}

void Adafruit_GFX_AS::fillScreen(uint16_t color)
{
    fillRect(0, 0, _width, _height, color);
}

void Adafruit_GFX_AS::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    drawFastHLine(x + r    , y        , w - 2 * r, color); // Top
    drawFastHLine(x + r    , y + h - 1, w - 2 * r, color); // Bottom
    drawFastVLine(x        , y + r    , h - 2 * r, color); // Left
    drawFastVLine(x + w - 1, y + r    , h - 2 * r, color); // Right
    // draw four corners
    drawCircleHelper(x + r        , y + r        , r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r        , r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r        , y + h - r - 1, r, 8, color);
}

void Adafruit_GFX_AS::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    fillRect(x + r, y, w - 2 * r, h, color);
    // draw four corners
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fillCircleHelper(x + r        , y + r, r, 2, h - 2 * r - 1, color);
}

void Adafruit_GFX_AS::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void Adafruit_GFX_AS::fillTriangle (int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    int16_t a, b, y, last;
    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1) {
        swap(y0, y1); swap(x0, x1);
    }
    if (y1 > y2) {
        swap(y2, y1); swap(x2, x1);
    }
    if (y0 > y1) {
        swap(y0, y1); swap(x0, x1);
    }

    if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a) {
            a = x1;
        } else if (x1 > b) {
            b = x1;
        }
        if (x2 < a) {
            a = x2;
        } else if (x2 > b) {
            b = x2;
        }
        drawFastHLine(a, y0, b - a + 1, color);
        return;
    }
    int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1,
    sa   = 0,
    sb   = 0;

    /*For upper part of triangle, find scanline crossings for segments
    0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    is included here (and second loop will be skipped, avoiding a /0
    error there), otherwise scanline y1 is skipped here and handled
    in the second loop...which also avoids a /0 error here if y0=y1
    (flat-topped triangle)*/
    if (y1 == y2) {
        last = y1;   // Include y1 scanline
    } else {
        last = y1 - 1; // Skip it
    }

    for (y = y0; y <= last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) {
            swap(a, b);
        }
        drawFastHLine(a, y, b - a + 1, color);
    }

    /* For lower part of triangle, find scanline crossings for segments
    0-2 and 1-2.  This loop is skipped if y1=y2*/
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b) {
            swap(a, b);
        }
        drawFastHLine(a, y, b - a + 1, color);
    }
}

void Adafruit_GFX_AS::drawBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h)
{
    /*Update in subclass*/
    for (uint16_t j = 0; j < h; j++) {
        for (uint16_t i = 0; i < w; i++) {
            drawPixel(x + i, y + j, bitmap[j * w + i]);
        }
    }
}

void Adafruit_GFX_AS::setTextCursor(int16_t x, int16_t y) {
    cursor_x = x;
    cursor_y = y;
}

uint16_t Adafruit_GFX_AS::getTextCursorX() const 
{
    return cursor_x;
}

uint16_t Adafruit_GFX_AS::getTextCursorY() const 
{
    return cursor_y;
}

void Adafruit_GFX_AS::setTextSize(uint8_t s) 
{
    textsize = (s > 0) ? s : 1;
}

void Adafruit_GFX_AS::setTextColor(uint16_t c)
{
    /* For 'transparent' background, we'll set the bg to the same as fg instead of using a flag*/    
    textcolor = textbgcolor = c;
}

void Adafruit_GFX_AS::setTextColor(uint16_t c, uint16_t b)
{
    textcolor   = c;
    textbgcolor = b;
}

void Adafruit_GFX_AS::setFontStyle(const GFXfont *f)
{
    if(f) {            // Font struct pointer passed in?
        if(!gfxFont) { // And no current font struct?
            // Switching from classic to new font behavior.
            // Move cursor pos down 6 pixels so it's on baseline.
            cursor_y += 6;
        }
    } else if(gfxFont) { // NULL passed.  Current font struct defined?
        // Switching from new to classic font behavior.
        // Move cursor pos up 6 pixels so it's at top-left of char.
        cursor_y -= 6;
    }
    gfxFont = (GFXfont *)f;
}

void Adafruit_GFX_AS::charBounds(char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx, int16_t *maxy)
{
    if(gfxFont) {
        if(c == '\n') { // Newline?
            *x  = 0;    // Reset x to zero, advance y by one line
            *y += textsize * (uint8_t)gfxFont->yAdvance;
        } else if(c != '\r') { // Not a carriage return; is normal char
            uint8_t first = gfxFont->first,
                    last  = gfxFont->last;
            if((c >= first) && (c <= last)) { // Char present in this font?
                GFXglyph *glyph = &(((GFXglyph *)&gfxFont->glyph))[c - first];
                uint8_t gw = glyph->width,
                        gh = glyph->height,
                        xa = glyph->xAdvance;
                int8_t  xo = glyph->xOffset,
                        yo = glyph->yOffset;
                if(wrap && ((*x+(((int16_t)xo+gw)*textsize)) > _width)) {
                    *x  = 0; // Reset x to zero, advance y by one line
                    *y += textsize * (uint8_t)gfxFont->yAdvance;
                }
                int16_t ts = (int16_t)textsize,
                        x1 = *x + xo * ts,
                        y1 = *y + yo * ts,
                        x2 = x1 + gw * ts - 1,
                        y2 = y1 + gh * ts - 1;
                if(x1 < *minx) *minx = x1;
                if(y1 < *miny) *miny = y1;
                if(x2 > *maxx) *maxx = x2;
                if(y2 > *maxy) *maxy = y2;
                *x += xa * ts;
            }
        }

    } 

    else { // Default font
        if(c == '\n') {                     // Newline?
            *x  = 0;                        // Reset x to zero,
            *y += textsize * 8;             // advance y one line
            // min/max x/y unchaged -- that waits for next 'normal' character
        } else if(c != '\r') {  // Normal char; ignore carriage returns
            if(wrap && ((*x + textsize * 6) > _width)) { // Off right?
                *x  = 0;                    // Reset x to zero,
                *y += textsize * 8;         // advance y one line
            }
            int x2 = *x + textsize * 6 - 1, // Lower-right pixel of char
                y2 = *y + textsize * 8 - 1;
            if(x2 > *maxx) *maxx = x2;      // Track max x, y
            if(y2 > *maxy) *maxy = y2;
            if(*x < *minx) *minx = *x;      // Track min x, y
            if(*y < *miny) *miny = *y;
            *x += textsize * 6;             // Advance x one char
        }
    }
}

// Pass string and a cursor position, returns UL corner and W,H.
void Adafruit_GFX_AS::getTextBounds(char *str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
{
    uint8_t c; // Current character

    *x1 = x;
    *y1 = y;
    *w  = *h = 0;

    int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;

    while((c = *str++))
        charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);

    if(maxx >= minx) {
        *x1 = minx;
        *w  = maxx - minx + 1;
    }
    if(maxy >= miny) {
        *y1 = miny;
        *h  = maxy - miny + 1;
    }
}

uint8_t Adafruit_GFX_AS::getRotation(void)
{
    return rotation;
}

void Adafruit_GFX_AS::setRotation(uint8_t x)
{
    rotation = (x & 3);
    switch (rotation) {
    case 0:
    case 2: _width  = WIDTH;
            _height = HEIGHT;
            break;
    case 1:
    case 3: _width  = HEIGHT;
            _height = WIDTH;
            break;
    }
}

int16_t Adafruit_GFX_AS::width(void)
{
    return _width;
}

int16_t Adafruit_GFX_AS::height(void)
{
    return _height;
}

void Adafruit_GFX_AS::invertDisplay(bool i)
{
    // Do nothing, must be subclassed if supported
}

void Adafruit_GFX_AS::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    // Do nothing, definition virtual void over rides to subclass
}

void Adafruit_GFX_AS::drawBitmapFont(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint16_t *bitmap)
{
    // Do nothing, definition virtual void over rides to subclass
}

int Adafruit_GFX_AS::drawString(const char *string, uint16_t x, uint16_t y)
{
    uint16_t xPlus = x;
    setTextCursor(xPlus, y);
    while (*string) {
        xPlus = write_char(*string);        // write_char string char-by-char                 
        setTextCursor(xPlus, y);       // increment cursor
        string++;                      // Move cursor right
    }
    return xPlus;
}

int Adafruit_GFX_AS::write_char(uint8_t c)
{
    if(!gfxFont) { // 'Classic' built-in font
        if(c == '\n') {                        // Newline?
            cursor_x  = 0;                     // Reset x to zero,
            cursor_y += textsize * 8;          // advance y one line
        } 
        else if(c != '\r') {                   // Ignore carriage returns
            if(wrap && ((cursor_x + textsize * 6) > _width)) { // Off right?
                cursor_x  = 0;                 // Reset x to zero,
                cursor_y += textsize * 8;      // advance y one line
            }
            drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
            cursor_x += textsize * 6;          // Advance x one char
        }
    } 
    
    else {
        if(c == '\n') {
            cursor_x  = 0;
            cursor_y += (int16_t)textsize * (uint8_t)gfxFont->yAdvance;
        } 
        else if(c != '\r') {
            uint8_t first = gfxFont->first;
            if((c >= first) && (c <= (uint8_t)gfxFont->last)) {

                GFXglyph *glyph = &(((GFXglyph *)gfxFont->glyph))[c - first];
                uint8_t   w     = glyph->width,
                          h     = glyph->height;

                if((w > 0) && (h > 0)) {                 // Is there an associated bitmap?
                    int16_t xo = (int8_t)glyph->xOffset; // sic
                    if(wrap && ((cursor_x + textsize * (xo + w)) > _width)) {
                        cursor_x  = 0;
                        cursor_y += (int16_t)textsize * (uint8_t)gfxFont->yAdvance;
                    }       
                    drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
                }
                cursor_x += (uint8_t)glyph->xAdvance * (int16_t)textsize;
            }
        }
    }
    return cursor_x;
}

void Adafruit_GFX_AS::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
    uint16_t swapped_foregnd_color = SWAPBYTES(color);  //SWAP earlier, or use SPI LSB first mode
    uint16_t swapped_backgnd_color = SWAPBYTES(bg);
        
    if(!gfxFont) { // 'Classic' built-in font

        if((x >= _width)            || // Clip right
           (y >= _height)           || // Clip bottom
           ((x + 6 * size - 1) < 0) || // Clip left
           ((y + 8 * size - 1) < 0))   // Clip top
            return;

        if(!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior

        //Check if bg!=color which is a flag for transperant backgrounds in our case
        if(color != bg) {
            //Make a premade bmp canvas and push the entire char canvas to the screen
            uint16_t bmp[8][5];     
            for (uint8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
                uint8_t line = font[c * 5 + i];
                for (uint8_t j = 0; j < 8; j++, line >>= 1) {
                    if(line & 1) {
                        if(size == 1) {
                            bmp[j][i] = swapped_foregnd_color;                        
                            //drawPixel(x+i, y+j, color); //The Old Adafruit way
                        }
                        else {
                            fillRect(x+i*size, y+j*size, size, size, color); //not as fast... set font size = 1 for speed
                        }
                    }
                    else {
                        bmp[j][i] = swapped_backgnd_color;
                    }                
                }
            }
			
            if(size == 1) {
	            uint16_t bmp_arr[40];
	            for (uint8_t cnt = 0; cnt < 40; cnt++) {
	                bmp_arr[cnt] = (bmp[(uint8_t)(cnt / 5)][(uint8_t)(cnt % 5)]); //5*8 Matrix to 40 element Array
	            }
	            drawBitmapFont(x, y, 5, 8, bmp_arr); //Speeds up fonts instead of using drawPixel
            }
        }

        //If user wants transperant background, check for transperancy flag which is (color == bg)
        else if(color == bg) {
            //Can't speedup fonts since no pushing pre-made canvas not possible, the fonts printed by this loop will be slow
            for (uint8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
                uint8_t line = font[c * 5 + i];
                for (uint8_t j = 0; j < 8; j++, line >>= 1) {
                    if(line & 1) {
                        if(size == 1) {                 
                            drawPixel(x+i, y+j, color); //The Old Adafruit way
                        }
                        else {
                            fillRect(x+i*size, y+j*size, size, size, color); //not as fast... set font size = 1 for speed
                        }
                    }         
                }
            }
        }
    } 
    else { 
        // Custom font
        // Character is assumed previously filtered by write_char() to eliminate
        // newlines, returns, non-printable characters, etc.  Calling
        // drawChar() directly with 'bad' characters of font may cause mayhem!

        c -= (uint8_t)gfxFont->first;
        GFXglyph *glyph  = &(((GFXglyph *)(gfxFont->glyph))[c]);
        uint8_t  *bitmap = (uint8_t *)(gfxFont->bitmap);

        uint16_t bo = glyph->bitmapOffset;
        uint8_t  w  = glyph->width,
                 h  = glyph->height;
        int8_t   xo = glyph->xOffset,
                 yo = glyph->yOffset;
        uint8_t  xx, yy, bits = 0, bit = 0;
        int16_t  xo16 = 0, yo16 = 0;

        if(size > 1) {
            xo16 = xo;
            yo16 = yo;
        }
        if(color != bg) {
            uint16_t bmp[w+5][h+5]; //w*h plus some extra space
            for(yy=0; yy<h; yy++) {
                for(xx=0; xx<w; xx++) {
                    if(!(bit++ & 7)) {
                        bits = bitmap[bo++];
                    }
                    
                    if(bits & 0x80) {
                        if(size == 1) {
                            //drawPixel(x+xo+xx, y+yo+yy, color); //Old adafruit way
                            bmp[yy][xx] = swapped_foregnd_color;
                        } 
                        else {
                            fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size, size, size, color);
                        }
                    }
                    
                    if(!(bits & 0x80)) {
                        bmp[yy][xx] = swapped_backgnd_color;
                    }
                    bits <<= 1;
                }
            }
			
            if (size == 1) {
	            uint16_t bmp_arr[w*h];
	            for (uint8_t cnt = 0; cnt < w*h; cnt++) {
	                bmp_arr[cnt] = (bmp[(uint8_t)(cnt / w)][(uint8_t)(cnt % w)]);
	            }
	            drawBitmapFont(x+xo, y+yo, w, h, bmp_arr); //Speeds up fonts instead of using drawPixel
        	}
		}
        
        else if(color == bg) {
            //Unimplemented: Transparent bg for custom fonts
            /*
            for(yy=0; yy<h; yy++) {
                for(xx=0; xx<w; xx++) {
                    if(!(bit++ & 7)) {
                        bits = bitmap[bo++];
                    }
                    if(bits & 0x80) {
                        if(size == 1) {
                            drawPixel(x+xo+xx, y+yo+yy, color); //Old adafruit way
                        } 
                        else {
                            fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size, size, size, color);
                        }
                    }
                    bits <<= 1;
                }
            }*/
            ESP_LOGE("LCD", "Custom fonts with transparent bg not supported yet\n");
        }
    } // End classic vs custom font
}

int Adafruit_GFX_AS::drawNumber(int long_num, uint16_t poX, uint16_t poY)
{
    char tmp[10];
    if (long_num < 0) {
        snprintf(tmp, sizeof(tmp), "%d", long_num);
    } else {
        snprintf(tmp, sizeof(tmp), "%u", long_num);
    }
    return drawString(tmp, poX, poY);
}

int Adafruit_GFX_AS::drawFloat(float floatNumber, uint8_t decimal, uint16_t poX, uint16_t poY)
{
    unsigned int temp = 0;
    float decy = 0.0;
    float rounding = 0.5;
    float eep = 0.000001;
    uint16_t xPlus = 0;

    if (floatNumber - 0.0 < eep) {
        xPlus       = drawString("-", poX, poY);
        floatNumber = -floatNumber;
        poX         = xPlus;
    }

    for (unsigned char i = 0; i < decimal; ++i) {
        rounding /= 10.0;
    }
    
    floatNumber += rounding;
    temp         = (long)floatNumber;
    xPlus        = drawNumber(temp, poX, poY);
    poX          = xPlus;

    if (decimal > 0) {
        xPlus = drawString(".", poX, poY);
        poX   = xPlus;                                       /* Move cursor right   */
    } else {
        return poX;
    }

    decy = floatNumber - temp;
    for (unsigned char i = 0; i < decimal; i++) {
        decy *= 10;                                         /* For the next decimal*/
        temp  = decy;                                        /* Get the decimal     */
        xPlus = drawNumber(temp, poX, poY);
        poX   = xPlus;                                       /* Move cursor right   */
        decy -= temp;
    }
    return poX;
}
