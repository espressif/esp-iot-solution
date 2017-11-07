#ifndef _JOYLINK_THUNDECONFIG_TYPES_H_
#define _JOYLINK_THUNDECONFIG_TYPES_H_
#include <stdint.h>
/* */
/*  Some utility macros */
/* */
#ifndef min
#define min(_a, _b)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#define max(_a, _b)     (((_a) > (_b)) ? (_a) : (_b))
#endif
#if 0
/******************************************************************************
 * SYSTEM DEPENDED TYPE DEFINITION
 ******************************************************************************/
typedef char                   int8_t;
typedef short                  int16_t;
typedef int                    int32_t;
typedef long long              int64_t;
typedef unsigned char          uint8_t;
typedef unsigned short         uint16_t;
//typedef unsigned short         uint16le;
typedef unsigned int           uint32_t;
//typedef unsigned int           uint32le;
typedef unsigned long long     uint64_t;
#endif
/* NULL */
#ifndef NULL
#define NULL                ((void *)0)
#endif

/* Boolean Values */
#ifndef FALSE
#define FALSE               (0)
#endif
#ifndef TRUE
#define TRUE                (!FALSE)
#endif

#endif /*! _JOYLINK_THUNDECONFIG_TYPES_H_ */

