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
#include "jg10.h"

void ugfx_justget(void)
{
    GEventMouse ev;
#if !JG10_SHOW_SPLASH
    font_t font;
#endif

    gfxInit();

    ginputGetMouse(0);
    jg10Init();

#if JG10_SHOW_SPLASH
    jg10ShowSplash();
#else
    font = gdispOpenFont("DejaVuSans16_aa");
    gdispDrawString((gdispGetWidth() / 2) - (gdispGetStringWidth("Touch to start!", font) / 2), gdispGetHeight() / 2, "Touch to start!", font, White);
    gdispCloseFont(font);
#endif

    while (TRUE) {
        ginputGetMouseStatus(0, &ev);
        if (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
            while (ev.buttons & GINPUT_MOUSE_BTN_LEFT) {
                // Wait until release
                ginputGetMouseStatus(0, &ev);
            }

#if !JG10_SHOW_SPLASH
            font = gdispOpenFont("DejaVuSans16");
            gdispFillArea((gdispGetWidth() / 2) - (gdispGetStringWidth("Touch to start!", font) / 2), gdispGetHeight() / 2, gdispGetWidth() / 2, 17, Black);
            gdispCloseFont(font);
#endif

            jg10Start();
        }
    }
}
