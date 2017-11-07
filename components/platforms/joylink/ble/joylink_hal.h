/**************************************************************************************************
  Filename:       joylink_hal.h
  Revised:        $Date: 2015-10-14 
  Revision:       $Revision: 1.0, Zhang Hua

  Description:    This file contains the Joylink profile definitions and
                  prototypes.

  Copyright 2010 - 2013 JD.COM. All rights reserved.

**************************************************************************************************/


#ifndef JOYLINK_HAL_H
#define JOYLINK_HAL_H

#ifdef __cplusplus
extern "C"
{
#endif
typedef unsigned int uint;
typedef unsigned char uchar;
/*********************************************************************
 * INCLUDES
 */
#include "joylink_sdk.h"  
  
extern int jh_send(uint8_t* datum);
extern int jh_load_mac(uint8_t * zone);
extern int jh_load_pid(uint8_t *pid);
extern void jh_logf(const char* format, ...); 
extern int jh_get_guid(unsigned char *guid);
extern int jh_save_guid(unsigned char *guid);
extern int jh_get_locallkey (unsigned char*localkey);
extern int jh_save_locallkey (unsigned char*localkey);
extern int jh_check_ready_indication();
	
/*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* JOYLINK_HAL_H */  













