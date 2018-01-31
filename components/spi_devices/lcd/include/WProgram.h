#ifndef _WPROGRAM_H_
#define _WPROGRAM_H_

#include "string.h"
#include "esp_types.h"
#include "freertos/FreeRTOS.h"

#define abs(x) ((x)<0 ? -(x) : (x))
#define swap(x, y) do { typeof(x) temp##x##y = x; x = y; y = temp##x##y; } while (0)

typedef bool boolean;
typedef char __FlashStringHelper;

class Print{
    public:
        void print(char* s);
};

#endif