/*
 * The MIT License (MIT)
 *
 * Copyright (c) Henry Gabryjelski
 * Copyright (c) Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef COMPILE_DATE_H
#define COMPILE_DATE_H

// Help enable build to be deterministic.
// Allows Ghostfat to generate 100% reproducible images across compilations.
// Reproducible builds are also important for other reasons.
// See generally, https://reproducible-builds.org/
#ifndef COMPILE_DATE
  #define COMPILE_DATE __DATE__
#endif
#ifndef COMPILE_TIME
  #define COMPILE_TIME __TIME__
#endif

#define COMPILE_YEAR_INT ((( \
  (COMPILE_DATE [ 7u] - '0')  * 10u + \
  (COMPILE_DATE [ 8u] - '0')) * 10u + \
  (COMPILE_DATE [ 9u] - '0')) * 10u + \
  (COMPILE_DATE [10u] - '0'))

#define COMPILE_MONTH_INT  ( \
    (COMPILE_DATE [2u] == 'n' && COMPILE_DATE [1u] == 'a') ?  1u  /*Jan*/ \
  : (COMPILE_DATE [2u] == 'b'                            ) ?  2u  /*Feb*/ \
  : (COMPILE_DATE [2u] == 'r' && COMPILE_DATE [1u] == 'a') ?  3u  /*Mar*/ \
  : (COMPILE_DATE [2u] == 'r'                            ) ?  4u  /*Apr*/ \
  : (COMPILE_DATE [2u] == 'y'                            ) ?  5u  /*May*/ \
  : (COMPILE_DATE [2u] == 'n'                            ) ?  6u  /*Jun*/ \
  : (COMPILE_DATE [2u] == 'l'                            ) ?  7u  /*Jul*/ \
  : (COMPILE_DATE [2u] == 'g'                            ) ?  8u  /*Aug*/ \
  : (COMPILE_DATE [2u] == 'p'                            ) ?  9u  /*Sep*/ \
  : (COMPILE_DATE [2u] == 't'                            ) ? 10u  /*Oct*/ \
  : (COMPILE_DATE [2u] == 'v'                            ) ? 11u  /*Nov*/ \
  :                                                          12u  /*Dec*/ )

#define COMPILE_DAY_INT ( \
   (COMPILE_DATE [4u] == ' ' ? 0 : COMPILE_DATE [4u] - '0') * 10u + \
   (COMPILE_DATE [5u] - '0')                                             \
   )

// __TIME__ expands to an eight-character string constant
// "23:59:01", or (if cannot determine time) "??:??:??" 
#define COMPILE_HOUR_INT ( \
   (COMPILE_TIME [0u] == '?' ? 0 : COMPILE_TIME [0u] - '0') * 10u \
 + (COMPILE_TIME [1u] == '?' ? 0 : COMPILE_TIME [1u] - '0')       )

#define COMPILE_MINUTE_INT ( \
   (COMPILE_TIME [3u] == '?' ? 0 : COMPILE_TIME [3u] - '0') * 10u \
 + (COMPILE_TIME [4u] == '?' ? 0 : COMPILE_TIME [4u] - '0')       )

#define COMPILE_SECONDS_INT ( \
   (COMPILE_TIME [6u] == '?' ? 0 : COMPILE_TIME [6u] - '0') * 10u \
 + (COMPILE_TIME [7u] == '?' ? 0 : COMPILE_TIME [7u] - '0')       )


#define COMPILE_DOS_DATE ( \
	((COMPILE_YEAR_INT  - 1980u) << 9u) | \
	( COMPILE_MONTH_INT          << 5u) | \
	( COMPILE_DAY_INT            << 0u) )

#define COMPILE_DOS_TIME ( \
	( COMPILE_HOUR_INT    << 11u) | \
	( COMPILE_MINUTE_INT  <<  5u) | \
	( COMPILE_SECONDS_INT <<  0u) )

#endif // COMPILE_DATE_H

