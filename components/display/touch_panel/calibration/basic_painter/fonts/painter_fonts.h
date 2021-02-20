// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __PAINTER_FONTS_H
#define __PAINTER_FONTS_H

#include <stdint.h>

typedef struct {
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
}font_t;

extern const font_t Font24;
extern const font_t Font20;
extern const font_t Font16;
extern const font_t Font12;
extern const font_t Font8;

#endif /* __PAINTER_FONTS_H */
