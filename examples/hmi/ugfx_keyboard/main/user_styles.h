#ifndef _USER_STYLES_H
#define _USER_STYLES_H

#include "iot_ugfx.h"

// Custom Style: BlackBerry
const GWidgetStyle BlackBerry = {
    HTML2COLOR(0x282828), // background
    HTML2COLOR(0x0092cc), // focus

    // Enabled color set
    {
        HTML2COLOR(0xf0f0f0), // text
        HTML2COLOR(0xc5c5c5), // edge
        HTML2COLOR(0x191919), // fill
        HTML2COLOR(0x0092cc), // progress (inactive area)
    },

    // Disabled color set
    {
        HTML2COLOR(0xf0f0f0), // text
        HTML2COLOR(0xc5c5c5), // edge
        HTML2COLOR(0x191919), // fill
        HTML2COLOR(0x008000), // progress (active area)
    },

    // Pressed color set
    {
        HTML2COLOR(0xf0f0f0), // text
        HTML2COLOR(0xc5c5c5), // edge
        HTML2COLOR(0x0092cc), // fill
        HTML2COLOR(0x0092cc), // progress (active area)
    }
};

#endif /* _USER_STYLES_H */
