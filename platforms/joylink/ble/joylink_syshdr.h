//
//  joylink_syshdr.h
//
// @brief system header , specailly for Device ,which has no operation system and necessary lib
//
//

#ifndef _JL_SYSHDR_H_
#define _JL_SYSHDR_H_
//#define JL_ONLY_BCM
#define JL_INT_32 
#ifdef __cplusplus
extern "C" {
#endif

    /* stdint.h subset */
#ifndef JL_MISS_STDINT_H
    #include <stdint.h>
#else
    /* You will need to modify these to match the word size of your platform. */
    typedef signed char int8_t;
    typedef unsigned char uint8_t;
    typedef signed short int16_t;
    typedef unsigned short uint16_t;
    
#ifdef JL_INT_32 
    typedef signed int int32_t;
    typedef unsigned int uint32_t;
  #else
    typedef signed long int32_t;
    typedef unsigned long uint32_t;
  #endif
#endif

    /* stddef.h subset */
#ifndef JL_MISS_STDDEF_H
#include <stddef.h>
#else


#ifndef NULL
#define NULL 0
#endif

#endif

    /* stdbool.h subset */
#ifndef JL_MISS_STDBOOL_H
#include <stdbool.h>
#else

#ifndef __cplusplus
typedef int bool;
#define false 0
#define true 1
#endif

#endif

    /* stdlib.h subset */
#ifndef JL_MISS_STDLIB_H
#include <stdlib.h>
#else
#endif

#ifndef JL_MISS_STRING_H
#include <string.h>
//for Broadcom build
#ifdef JL_ONLY_BCM
#include "spar_utils.h"
#endif
#else
static size_t strlen( const char * s )
{
	size_t rc = 0;
	while ( s[rc] )
	{
	    ++rc;
	}
	return rc;
}

static void * memcpy( void *s1, const void *s2, size_t n )
{
	char * dest = (char *) s1;
	const char * src = (const char *) s2;
	while ( n-- )
	{
	    *dest++ = *src++;
	}
	return s1;
}

static void * memset( void * s, int c, size_t n )
{
	unsigned char * p = (unsigned char *) s;
	while ( n-- )
	{
	    *p++ = (unsigned char) c;
	}
	return s;
}

static  int memcmp (const void *s1, const void *s2, size_t n){
	if (!n)
	    return(0);
	while ( --n && *(char *)s1 == *(char *)s2)
	{
	    s1 = (char *)s1 + 1;
	    s2 = (char *)s2 + 1;
	}
	return( *((unsigned char *)s1) - *((unsigned char *)s2) );
}
#endif


#ifdef __cplusplus
}
#endif
#endif
