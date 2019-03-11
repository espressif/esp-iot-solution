/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/**
 * @file    src/gos/gos_freertos.h
 * @brief   GOS - Operating System Support header file for FreeRTOS.
 */

#ifndef _GOS_FREERTOS_H
#define _GOS_FREERTOS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "gfx_types.h"

extern portMUX_TYPE lockMux_priv;
/*===========================================================================*/
/* Type definitions                                                          */
/*===========================================================================*/

#define gDelayNone			0
#define gDelayForever		((gDelay)-1)
typedef gU32				gDelay;
typedef portTickType		gTicks;
typedef gI32				gSemcount;
typedef void				gThreadreturn;
typedef portBASE_TYPE		gThreadpriority;

#define gSemMaxCount			((gSemcount)(((unsigned long)((gSemcount)(-1))) >> 1))
#define gThreadpriorityLow		0
#define gThreadpriorityNormal	configMAX_PRIORITIES/2
#define gThreadpriorityHigh		configMAX_PRIORITIES-1

/* FreeRTOS will allocate the stack when creating the thread */
#define GFX_THREAD_STACK(name, sz)			gU8 name[1]
#define GFX_THREAD_FUNCTION(fnName, param)	gThreadreturn fnName(void *param)
#define gfxThreadReturn(retval)				return

typedef xSemaphoreHandle		gSem;
typedef xSemaphoreHandle		gMutex;
typedef xTaskHandle				gThread;

/*===========================================================================*/
/* Function declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

#define gfxHalt(msg)				{while(1);}
#define gfxExit()					{while(1);}
#define gfxAlloc(sz)				pvPortMalloc(sz)
#define gfxFree(ptr)				vPortFree(ptr)
#define gfxYield()					taskYIELD()
#define gfxSystemTicks()			xTaskGetTickCount()
#define gfxMillisecondsToTicks(ms)	((gTicks)((ms) / portTICK_PERIOD_MS))
#define gfxSystemLock()				portENTER_CRITICAL(&lockMux_priv)
#define gfxSystemUnlock()			portEXIT_CRITICAL(&lockMux_priv)

void gfxMutexInit(gMutex* s);
#define gfxMutexDestroy(pmutex)		vSemaphoreDelete(*(pmutex))
#define gfxMutexEnter(pmutex)		xSemaphoreTake(*(pmutex),portMAX_DELAY)
#define gfxMutexExit(pmutex)		xSemaphoreGive(*(pmutex))

void *gfxRealloc(void *ptr, gMemSize oldsz, gMemSize newsz);
void gfxSleepMilliseconds(gDelay ms);
void gfxSleepMicroseconds(gDelay ms);

void gfxSemInit(gSem* psem, gSemcount val, gSemcount limit);
#define gfxSemDestroy(psem)			vSemaphoreDelete(*(psem))
gBool gfxSemWait(gSem* psem, gDelay ms);
gBool gfxSemWaitI(gSem* psem);
void gfxSemSignal(gSem* psem);
void gfxSemSignalI(gSem* psem);
gThread gfxThreadCreate(void *stackarea, gMemSize stacksz, gThreadpriority prio, GFX_THREAD_FUNCTION((*fn),p), void *param);

#define gfxThreadMe()				xTaskGetCurrentTaskHandle()
#if INCLUDE_eTaskGetState == 1
	gThreadreturn gfxThreadWait(gThread thread);
#endif
#define gfxThreadClose(thread)

#ifdef __cplusplus
}
#endif

#endif /* _GOS_CHIBIOS_H */
