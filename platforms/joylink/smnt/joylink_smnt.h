/*************************************

Copyright (c) 2015-2050, JD Smart All rights reserved. 

*************************************/
#ifndef _JOYLINK_SMNT_H_
#define _JOYLINK_SMNT_H_

#include "joylink_smnt_adp.h"

typedef enum 
{
	TYPE_JOY_OTHER = 0,
	TYPE_JOY_SMNT  = 1
}joylink_smnt_type_t;

typedef enum _joylink_smnt_status
{
	SMART_CH_INIT 		=	0x1,
	SMART_CH_LOCKING 	=	0x2,
	SMART_CH_LOCKED 	=	0x4,
	SMART_FINISH 		= 	0x8
}joylink_smnt_status_t;

typedef struct _joylink_smnt_info
{
	joylink_smnt_type_t			joy_smnt_type;				//smnt type TYPE_JOY_OTHER-other type, TYPE_JOY_SMNT - joylink's smnt
	joylink_smnt_status_t 		joy_smnt_state;				//smnt state,
	uint8 						joy_smnt_bssid[6];			//bssid
	uint8 						joy_smnt_channel_fix;		//the current channel
}joylink_smnt_info_t;

typedef struct _joylinkResult
{
	unsigned char type;	                        // 0:NotReady, 1:ControlPacketOK, 2:BroadcastOK, 3:MulticastOK
	unsigned char encData[1+32+32+32];            // length + EncodeData
}joylinkResult_t;

extern void adp_changeCh(int i);
void joylink_cfg_init(unsigned char* pBuffer);                                  // 传入一个1024字节的存储空间
int  joylink_cfg_50msTimer(void);                                               // 50ms定时器回调,如果返回非零,那么这个值作为下一次调用的时间间隔,可能的取值为(50,100,5000,10000)
void joylink_cfg_DataAction(PHEADER_802_11 pHeader, int length);                // 局域网收到数据包
int  joylink_cfg_Result(joylinkResult_t* pRet);					// Get Result

#endif
