#ifndef _JOYLINK_THUNDERCONFIG_H_
#define _JOYLINK_THUNDERCONFIG_H_

#include "thunderconfig_utils.h"

extern int thunderconfig_init(jl_init_param_t init_param);

extern int thunderconfig_destroy();

extern void thunderconfig_switch(uint8_t on);

extern int thunderconfig_50ms_timer();

extern int thunderconfig_rx(JL_FRAME_SUBTYPE frame_type, void *frame);

#endif /*!_JOYLINK_THUNDERCONFIG_H_*/
