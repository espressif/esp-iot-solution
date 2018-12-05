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

#define N 1024 /* Number of dots */
#define SCALE 8192
#define INCREMENT 512 /* INCREMENT = SCALE / sqrt(N) * 2 */
#define PI2 6.283185307179586476925286766559

#define RED_COLORS (32)
#define GREEN_COLORS (64)
#define BLUE_COLORS (32)

#define background Black

static uint16_t width, height;
int16_t sine[SCALE + (SCALE / 4)];
int16_t *cosi = &sine[SCALE / 4]; /* cos(x) = sin(x+90d)... */
int16_t angleX = SCALE / 8, angleY = 3 * SCALE / 1, angleZ = 3 * SCALE / 4;
int16_t speedX = 1, speedY = 3, speedZ = 2;
int16_t xyz[3][N];
color_t col[N];

void initialize(void)
{
    uint16_t i;

    /* if you change the SCALE*1.25 back to SCALE, the program will
     * occassionally overrun the cosi array -- however this actually
     * produces some interesting effects as the BUBBLES LOSE CONTROL!!!!
     */
    for (i = 0; i < SCALE + (SCALE / 4); i++) {
        //sine[i] = (-SCALE/2) + (int)(sinf(PI2 * i / SCALE) * sinf(PI2 * i / SCALE) * SCALE);
        sine[i] = (int)(sinf(PI2 * i / SCALE) * SCALE * 0.8);
    }
}

void matrix(int16_t xyz[3][N], color_t col[N])
{
    static uint32_t t = 0;
    int16_t x = -SCALE, y = -SCALE;
    uint16_t i, s, d;
    uint8_t red, grn, blu;

    for (i = 0; i < N; i++) {
        xyz[0][i] = x;
        xyz[1][i] = y;

        d = sqrt(x * x + y * y); /* originally a fastsqrt() call */
        s = sine[(t * 30) % SCALE] + SCALE;

        xyz[2][i] = sine[(d + s) % SCALE] * sine[(t * 10) % (SCALE / 2)] / SCALE / 2;

        red = (cosi[xyz[2][i] + SCALE / 2] + SCALE) * (RED_COLORS - 1) / SCALE / 2;
        grn = (cosi[(xyz[2][i] + SCALE / 2 + 2 * SCALE / 3) % SCALE] + SCALE) * (GREEN_COLORS - 1) / SCALE / 2;
        blu = (cosi[(xyz[2][i] + SCALE / 2 + SCALE / 3) % SCALE] + SCALE) * (BLUE_COLORS - 1) / SCALE / 2;
        col[i] = ((red << 11) + (grn << 5) + blu);

        x += INCREMENT;

        if (x >= SCALE) {
            x = -SCALE;
            y += INCREMENT;
        }
    }

    t++;
}

void rotate(int16_t xyz[3][N], uint16_t angleX, uint16_t angleY, uint16_t angleZ)
{
    uint16_t i;
    int16_t tmpX, tmpY;
    int16_t sinx = sine[angleX], cosx = cosi[angleX];
    int16_t siny = sine[angleY], cosy = cosi[angleY];
    int16_t sinz = sine[angleZ], cosz = cosi[angleZ];

    for (i = 0; i < N; i++) {
        tmpX = (xyz[0][i] * cosx - xyz[2][i] * sinx) / SCALE;
        xyz[2][i] = (xyz[0][i] * sinx + xyz[2][i] * cosx) / SCALE;
        xyz[0][i] = tmpX;

        tmpY = (xyz[1][i] * cosy - xyz[2][i] * siny) / SCALE;
        xyz[2][i] = (xyz[1][i] * siny + xyz[2][i] * cosy) / SCALE;
        xyz[1][i] = tmpY;

        tmpX = (xyz[0][i] * cosz - xyz[1][i] * sinz) / SCALE;
        xyz[1][i] = (xyz[0][i] * sinz + xyz[1][i] * cosz) / SCALE;
        xyz[0][i] = tmpX;
    }
}

void draw(int16_t xyz[3][N], color_t col[N])
{
    static uint16_t oldProjX[N] = {0};
    static uint16_t oldProjY[N] = {0};
    static uint8_t oldDotSize[N] = {0};
    uint16_t i, projX, projY, projZ, dotSize;

    for (i = 0; i < N; i++) {
        projZ = SCALE - (xyz[2][i] + SCALE) / 4;
        projX = width / 2 + (xyz[0][i] * projZ / SCALE) / 25;
        projY = height / 2 + (xyz[1][i] * projZ / SCALE) / 25;
        dotSize = 3 - (xyz[2][i] + SCALE) * 2 / SCALE;

        gdispDrawCircle(oldProjX[i], oldProjY[i], oldDotSize[i], background);

        if (projX > dotSize && projY > dotSize && projX < width - dotSize && projY < height - dotSize) {
            gdispDrawCircle(projX, projY, dotSize, col[i]);
            oldProjX[i] = projX;
            oldProjY[i] = projY;
            oldDotSize[i] = dotSize;
        }
    }
}

void ugfx_bubbles()
{
    int pass = 0;

    gfxInit(); // Initialize the uGFX library

    gdispClear(background);

    width = (uint16_t)gdispGetWidth();
    height = (uint16_t)gdispGetHeight();

    initialize();

    while (1) {
        matrix(xyz, col);
        rotate(xyz, angleX, angleY, angleZ);
        draw(xyz, col);

        angleX += speedX;
        angleY += speedY;
        angleZ += speedZ;

        if (pass > 400) {
            speedY = 1;
        }
        if (pass > 800) {
            speedX = 1;
        }
        if (pass > 1200) {
            speedZ = 1;
        }
        pass++;

        if (angleX >= SCALE) {
            angleX -= SCALE;
        } else if (angleX < 0) {
            angleX += SCALE;
        }

        if (angleY >= SCALE) {
            angleY -= SCALE;
        } else if (angleY < 0) {
            angleY += SCALE;
        }

        if (angleZ >= SCALE) {
            angleZ -= SCALE;
        } else if (angleZ < 0) {
            angleZ += SCALE;
        }

        gfxSleepMilliseconds(4); // The F7 is a bit too fast... Slow this down
    }
}
