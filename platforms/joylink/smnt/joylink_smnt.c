/*************************************

Copyright (c) 2015-2050, JD Smart All rights reserved. 

*************************************/
#include <stdio.h>
#include <string.h>
#include "joylink_smnt.h"
#define STEP_MULTICAST_HOLD_CHANNEL				5
#define STEP_BROADCAST_HOLE_CHANNEL				4
#define STEP_BROADCAST_ERROR_HOLE_CHANNEL		2


#define PAYLOAD_MIN		(3)
#define PAYLOAD_MAX		(48+1)				


//#define printf_high printf
#define printf_high(...)

static uint8 getCrc(uint8 *ptr, uint8 len);
static void dump8(uint8* p, int split, int len);
static int  payLoadCheck(void);
static void muticastAdd(uint8* pAddr);
static void broadcastAdd(int ascii);

typedef struct _joylinkSmnt
{
	joylink_smnt_type_t jd_smnt_type;
	joylink_smnt_status_t state;	
	uint16 syncFirst;				
	uint8 syncCount;				
	uint8 syncStepPoint;			
	uint8 syncFirst_downlink;		
	uint8 syncCount_downlink;		
	uint8 syncStepPoint_downlink;	
	uint8 directTimerSkip;			
	uint8 broadcastVersion;			
	uint8 muticastVersion;			
	uint8  broadIndex;				
	uint8  broadBuffer[5];			
	uint16 lastLength;			
	uint16 lastUploadSeq;		
	uint16 lastDownSeq;		
	uint8  syncAppMac[6];		
	uint8  syncBssid[6];		
	uint16 syncIsUplink;		
	uint8 chCurrentIndex;		
	uint8 chCurrentProbability;	
	uint8 isProbeReceived;	
	uint8 payload[128];		
	joylinkResult_t result;	
}joylinkSmnt_t;

joylinkSmnt_t* pSmnt = NULL;

static int payLoadCheck(void)
{
	uint8 crc = getCrc(pSmnt->payload + 1, pSmnt->payload[1]+1);
	if ((pSmnt->payload[1] > (PAYLOAD_MIN*2)) &&
		(pSmnt->payload[1] < (PAYLOAD_MAX*2)) &&
        (pSmnt->payload[0] == crc)){

		joylinkResult_t* pRet = &pSmnt->result;
		memcpy(pRet->encData, pSmnt->payload+1, pSmnt->payload[1]+1);
		pRet->type = pSmnt->result.type;
        pSmnt->state = SMART_FINISH;
        return 0;
	}
	return 1;
}


void joylink_cfg_init(unsigned char* pBuffer)
{
	pSmnt = (void*)pBuffer;
    memset(pSmnt, 0, sizeof(joylinkSmnt_t) );
	memset(pSmnt->payload, 0xFF, 128);	
}


int  joylink_cfg_Result(joylinkResult_t* pRet)
{
	pRet->type = 0;
	
	if (pSmnt && (pSmnt->state== SMART_FINISH) ){
		memcpy(pRet, &pSmnt->result, sizeof(joylinkResult_t));
		return 0;
	}
	
	return 1;
}

int joylink_cfg_50msTimer(void)
{
	if (pSmnt->directTimerSkip){
		pSmnt->directTimerSkip--;
		return 100;
	}
	
	if (pSmnt->state == SMART_FINISH){
		printf_high("-------------------->Finished\n");
		pSmnt->directTimerSkip = 10000/50;
		return 100;
	}
	
	if (pSmnt->isProbeReceived >0 ){
		printf_high("-------------------->Probe Stay(CH:%d) %d\n", pSmnt->chCurrentIndex + 1, pSmnt->isProbeReceived);
		pSmnt->isProbeReceived = 0;
		pSmnt->directTimerSkip = 5000 / 50;
		return 100;
	}
	
	if (pSmnt->chCurrentProbability > 0){
		pSmnt->chCurrentProbability--;
		printf_high("------------------->SYNC (CH:%d) %d\n", pSmnt->chCurrentIndex + 1, pSmnt->chCurrentProbability);
		return 100;
	}

	pSmnt->chCurrentIndex = (pSmnt->chCurrentIndex + 1) % 13;
	adp_changeCh(pSmnt->chCurrentIndex +1);
	
	pSmnt->state 			= SMART_CH_LOCKING;
	pSmnt->jd_smnt_type 	= TYPE_JOY_OTHER;
	pSmnt->syncStepPoint = 0;
	pSmnt->syncCount = 0;
	pSmnt->chCurrentProbability = 0;
	printf_high("CH=%d, T=%d\n", pSmnt->chCurrentIndex +1, 50);
	return 100;
}

static void  broadcastAdd(int ascii)
{
	uint8 isFlag = (uint8)((ascii >> 8) & 0x1);
	uint8 is_finishpacket = 0;

	uint8 *broadbuffer=pSmnt->broadBuffer,*broadindex = &(pSmnt->broadIndex);
	if (isFlag)
	{
		*broadindex = 0;
		*broadbuffer = (uint8)ascii;
	}
	else
	{
		*broadindex = *broadindex + 1;
		broadbuffer[*broadindex] = (uint8)ascii;

		if((((pSmnt->payload[1] +2) / 4 + 1) == ( (*broadbuffer) >> 3))  && (pSmnt->payload[1] != 0) && (pSmnt->payload[1] != 0xFF))
		{
			if(*broadindex == 2){
				is_finishpacket = 1;
				*(broadbuffer + 3) = 0;
				*(broadbuffer + 4) = 0;
			}
		}
	
		if (*broadindex >= 4 || is_finishpacket)
		{
			*broadindex = 0;
			uint8 crc = (*broadbuffer) & 0x7;
			uint8 index = (*broadbuffer) >> 3;
			uint8 rCrc = getCrc(broadbuffer + 1, 4) & 0x7;
			
			/*not to check the last pacet crc,It is a patch for the last packet is a lot wrong which maybe leaded by the phone*/
			if (((index>0) && (index<33) && (rCrc == crc)))
			{
				memcpy(pSmnt->payload + (index - 1) * 4, broadbuffer + 1, 4);
					
				printf_high("B(%x=%x)--%02x,%02x,%02x,%02x\n", index, broadbuffer[0], broadbuffer[1], broadbuffer[2], broadbuffer[3], broadbuffer[4]);
				index = payLoadCheck();

				if (pSmnt->chCurrentProbability < 30){ 
					pSmnt->chCurrentProbability += STEP_BROADCAST_HOLE_CHANNEL;
				}
				
				if (index == 0){	
					pSmnt->result.type = 2;
				}
				
				//dump8(pSmnt->payload, 1, 80);
				
			}
			else
			{
				if (pSmnt->chCurrentProbability < 30){ 
					pSmnt->chCurrentProbability += STEP_BROADCAST_ERROR_HOLE_CHANNEL;
				}
			}
		}
		else if (*broadindex == 2)
		{
			uint8 index = broadbuffer[0] >> 3;
			if (index == 0){
				*broadindex = 0;
				uint8 crc = broadbuffer[0] & 0x7;
				uint8 rCrc = getCrc(broadbuffer + 1, 2) & 0x7;
				if (rCrc == crc){
					pSmnt->broadcastVersion = broadbuffer[1];
					printf_high("Version RX:%x\n",pSmnt->broadcastVersion);
				}
			}
		}
	} 
}

/*
Input: Muticast Addr
Output: -1:Unkown Packet, 0:Parse OK, 1:Normal Process
*/
static void muticastAdd(uint8* pAddr)
{
	int8 index = 0;

	/*version data*/
	if((pAddr[3] == 0) && (pAddr[4] == 1) && (pAddr[5]<=3) && (pAddr[5]>0))
		pSmnt->jd_smnt_type = TYPE_JOY_SMNT;
	if (pAddr[3] == 0)
		index = 0 - pAddr[3];
	else if ((pAddr[3] >> 6) == ((pAddr[4] ^ pAddr[5]) & 0x1))
		index = pAddr[3] & 0x3F;
	else
		return;
	
	if (index < PAYLOAD_MAX)		//avoid overstep leaded by error
	{
		uint8 payloadIndex = index - 1;
		if (payloadIndex > 64)
			return;

		if (pSmnt->chCurrentProbability < 20) 
			pSmnt->chCurrentProbability += STEP_MULTICAST_HOLD_CHANNEL;			// Delay CH switch!
		
		printf_high("M%02d(CH=%d)--%02X:(%02X,%02X)\n", index, pSmnt->chCurrentIndex + 1, pAddr[3], pAddr[4], pAddr[5]); 
		pSmnt->payload[payloadIndex * 2]     = pAddr[4];
		pSmnt->payload[payloadIndex * 2 + 1] = pAddr[5];
#ifdef IS_FULL_LOG
		dump8(pSmnt->payload, 1, 80);
#endif
		if (payLoadCheck() == 0){
			pSmnt->result.type = 3;
			return;
		}
	}
	return;
}


joylink_smnt_info_t	joylink_get_smnt_info(void)
{
	joylink_smnt_info_t	smnt_info;
	memset(&smnt_info,0,sizeof(joylink_smnt_info_t));

	smnt_info.joy_smnt_type 	= pSmnt->jd_smnt_type;
	smnt_info.joy_smnt_state	= pSmnt->state;
	memcpy(smnt_info.joy_smnt_bssid,pSmnt->syncBssid,6);
	smnt_info.joy_smnt_channel_fix 	= pSmnt->chCurrentIndex + 1;
	return smnt_info;
}

void joylink_cfg_DataAction(PHEADER_802_11 pHeader, int length)
{
	uint8 isUplink = 1;				
	uint8 packetType = 0;					// 1-multicast packets 2-broadcast packets 0-thers
	uint8 isDifferentAddr = 0;
	uint8 *pDest, *pSrc, *pBssid;
	uint16 lastLength = 0;
	uint16 lastSeq_uplink = 0,lastSeq_downlink = 0;
	static uint8 past_channel = 0xFF;
	
	if (pSmnt == NULL)
		return;
	if ((length > 100) && (pSmnt->state != SMART_CH_LOCKED))	
		return;
	
	if (pHeader->FC.ToDs){
		isUplink = 1;				
		pBssid = pHeader->Addr1;
		pSrc = pHeader->Addr2;
		pDest = pHeader->Addr3;	

		if (!((memcmp(pDest, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0) || (memcmp(pDest, "\x01\x00\x5E", 3) == 0))){
			return;
		}		
		lastSeq_uplink = pSmnt->lastUploadSeq;
		pSmnt->lastUploadSeq = pHeader->Sequence;
	}else{
		pDest = pHeader->Addr1;	
		pBssid = pHeader->Addr2;
		pSrc  = pHeader->Addr3;
		
		isUplink = 0;
		//not broadcast nor multicast package ,return
		if (!((memcmp(pDest, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0) || (memcmp(pDest, "\x01\x00\x5E", 3) == 0))){
			return;
		}
		lastSeq_downlink = pSmnt->lastDownSeq;
		pSmnt->lastDownSeq = pHeader->Sequence;
	}
	lastLength = pSmnt->lastLength;
	pSmnt->lastLength = length;

	if (memcmp(pDest, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0)
	{
		if (pSmnt->state == SMART_CH_LOCKING)
		{
			if(isUplink == 1)
				printf_high("uplink:(%02x-%04d)->length:%2x, =length-synfirst:(0x%02x),synfirst:%2x\n", *((uint8*)pHeader) & 0xFF, pHeader->Sequence, length, (uint8)(length - pSmnt->syncFirst +1),pSmnt->syncFirst);
			else
				printf_high("downlink:(%02x-%04d)->length:%2x, =length-synfirst:(0x%02x),synfirst:%2x\n", *((uint8*)pHeader) & 0xFF, pHeader->Sequence, length, (uint8)(length - pSmnt->syncFirst_downlink +1),pSmnt->syncFirst_downlink);
			
		}
		packetType = 2;
	}
	else if (memcmp(pDest, "\x01\x00\x5E", 3) == 0)
	{
		if (pSmnt->state == SMART_CH_LOCKING)
			printf_high("(%02x-%04d):%02x:%02x:%02x->%d\n", *((uint8*)pHeader) & 0xFF, pHeader->Sequence, pDest[3], pDest[4], pDest[5], (uint8)length);
		packetType = 1;
	}

	if (memcmp(pSrc, pSmnt->syncAppMac, 6) != 0)
	{
		isDifferentAddr = 1;
	}

	if(pSmnt->state == SMART_CH_LOCKING)
	{
		if (packetType == 0) return;
		if ((isUplink==1) && (pHeader->Sequence == lastSeq_uplink)) return;
		if ((isUplink==0) && (pHeader->Sequence == lastSeq_downlink)) return;
		if(!isDifferentAddr)
		{
			if (packetType != 0)
			{
				if (packetType == 1)
				{
					if (((pDest[3] >> 6) == ((pDest[4] ^ pDest[5]) & 0x1)) && (pDest[3] != 0) && ((pDest[3]&0x3F) <=  PAYLOAD_MAX))
					{
						/*if receive multicast right message for two times,lock the channel*/
						if(past_channel == pSmnt->chCurrentIndex + 1)
						{
							past_channel = 0xFF;
							if (pSmnt->chCurrentProbability < 20) 
								pSmnt->chCurrentProbability = 10;

							memcpy(pSmnt->syncBssid, pBssid, 6);
							pSmnt->state = SMART_CH_LOCKED;
						}
						else
						{
							past_channel = pSmnt->chCurrentIndex + 1;
						}

					}
					muticastAdd(pDest); // Internal state machine could delay the ch switching

					return;
				}

				if(isUplink == 1)
				{
					if (lastLength == length)	return;

					int expectLength = 1 + pSmnt->syncFirst + pSmnt->syncCount%4 - (pSmnt->syncStepPoint?4:0);
					int isStep = (pSmnt->syncStepPoint == 0 && length == (expectLength - 4));	

					if ( ( length == expectLength ) || isStep)
					{
						pSmnt->syncCount++;
						pSmnt->chCurrentProbability++;

						if (isStep)		pSmnt->syncStepPoint = pSmnt->syncCount;

						if (pSmnt->syncCount >= 3)	// Achive SYNC count!
						//if (pSmnt->syncCount >= 4)	// Achive SYNC count!
						{
							pSmnt->syncFirst = pSmnt->syncFirst + pSmnt->syncStepPoint - (pSmnt->syncStepPoint ? 4 : 0);	// Save sync world
							memcpy(pSmnt->syncBssid, pBssid, 6);

							pSmnt->state 		= SMART_CH_LOCKED;
							pSmnt->jd_smnt_type = TYPE_JOY_SMNT;

							printf_high("SYNC:(%02X%02X%02X%02X%02X%02X-%02X%02X%02X%02X%02X%02X)------->:CH=%d, WD=%d\n",
								pSrc[0], pSrc[1], pSrc[2], pSrc[3], pSrc[4], pSrc[5],
								pBssid[0], pBssid[1], pBssid[2], pBssid[3], pBssid[4], pBssid[5],
								pSmnt->chCurrentIndex+1, pSmnt->syncFirst);
							
							pSmnt->syncIsUplink = isUplink;
							
							if(pSmnt->chCurrentProbability < 20)
								pSmnt->chCurrentProbability = 20;
							printf_high("--->locked by uplink\n");
						}
						return;
					}
					if (pSmnt->syncCount)
					{
						pSmnt->syncStepPoint = 0;
						pSmnt->syncCount = 0;
						memcpy(pSmnt->syncAppMac, pSrc, 6);
						pSmnt->syncFirst = length;
						printf_high("SYNC LOST\n");
					}
				}
				else
				{
					if (lastLength == length)	return;

					int expectLength = 1 + pSmnt->syncFirst_downlink+ pSmnt->syncCount_downlink%4 - (pSmnt->syncStepPoint_downlink?4:0);
					int isStep = (pSmnt->syncStepPoint_downlink == 0 && length == (expectLength - 4));	

					if ( ( length == expectLength ) || isStep)
					{
						pSmnt->syncCount_downlink++;
						pSmnt->chCurrentProbability++;
						
						if (isStep)			pSmnt->syncStepPoint_downlink = pSmnt->syncCount_downlink;

						if (pSmnt->syncCount_downlink>= 3)	// Achive SYNC count!
						//if (pSmnt->syncCount_downlink>= 4)	// Achive SYNC count!
						{
							pSmnt->syncFirst_downlink = pSmnt->syncFirst_downlink + pSmnt->syncStepPoint_downlink - (pSmnt->syncStepPoint_downlink? 4 : 0);	// Save sync world
							memcpy(pSmnt->syncBssid, pBssid, 6);
							pSmnt->state = SMART_CH_LOCKED;
							printf_high("SYNC:(%02X%02X%02X%02X%02X%02X-%02X%02X%02X%02X%02X%02X)------->:CH=%d, WD=%d\n",
								pSrc[0], pSrc[1], pSrc[2], pSrc[3], pSrc[4], pSrc[5],
								pBssid[0], pBssid[1], pBssid[2], pBssid[3], pBssid[4], pBssid[5],
								pSmnt->chCurrentIndex+1, pSmnt->syncFirst_downlink);

							pSmnt->syncIsUplink = isUplink;
							if(pSmnt->chCurrentProbability < 20)
								pSmnt->chCurrentProbability = 20;
							printf_high("--->locked by downlink\n");
						}
						return;
					}
					//if (pSmnt->syncCount_downlink)	// ��������:������ÿ���յ���ͬAddrʱ����ʼ����������
					{
						pSmnt->syncStepPoint_downlink = 0;
						pSmnt->syncCount_downlink = 0;
						memcpy(pSmnt->syncAppMac, pSrc, 6);
						pSmnt->syncFirst_downlink = length;
						printf_high("SYNC LOST\n");
					}
				}
			} 
			return;	
		}
		memcpy(pSmnt->syncAppMac, pSrc, 6);
		pSmnt->syncFirst = length;
		pSmnt->syncFirst_downlink = length;
		printf_high("Try to SYNC!\n");
		return;
	}
	else if (pSmnt->state == SMART_CH_LOCKED)
	{
		if (isDifferentAddr) return;

		if (packetType == 1){
			muticastAdd(pDest);
			return;
		}
		
		if ( (packetType != 1)&&(memcmp(pDest, pSmnt->syncBssid, 6) != 0) ){
			packetType = 2;
		}
		
		if(isUplink == 1)
		{
			if (length < (pSmnt->syncFirst - 1)) return;
			if ( pHeader->Sequence == lastSeq_uplink) return;
		}
		else
		{
			if (length < (pSmnt->syncFirst_downlink - 1)) return;
			if ( pHeader->Sequence == lastSeq_downlink) return;
		}
		if (packetType == 2)
		{
			int ascii;
			if(isUplink == 1)
				ascii = length - pSmnt->syncFirst + 1;
			else
				ascii = length - pSmnt->syncFirst_downlink + 1;

			if((((ascii >> 8) & 0x01) == 1) && (((ascii >> 3)&0x1F) == 0))
			{
				;
			}
			else
			{
				broadcastAdd(ascii);
			}
			if (((length + 4 - lastLength) % 4 == 1)&& (length - pSmnt->syncFirst)<4)  // There are SYNC packets even ch locked.
			{
				if (pSmnt->chCurrentProbability < 20) pSmnt->chCurrentProbability++;
			}
		}
	}
	else if (pSmnt->state == SMART_FINISH)
	{
		printf_high("SMART_FINISH-1\n");
	}
	else
	{
		pSmnt->state = SMART_CH_LOCKING;
		memcpy(pSmnt->syncAppMac, pSrc, 6);
		pSmnt->syncFirst = length;
		pSmnt->syncStepPoint = 0;
		pSmnt->syncCount = 0;
		printf_high("Reset All State\n");
	}
	return;
}

static uint8 getCrc(uint8 *ptr, uint8 len)
{
	unsigned char crc;
	unsigned char i;
	crc = 0;
	while (len--)
	{
		crc ^= *ptr++;
		for (i = 0; i < 8; i++)
		{
			if (crc & 0x01)
			{
				crc = (crc >> 1) ^ 0x8C;
			}
			else
				crc >>= 1;
		}
	}
	return crc;
}
static void dump8(uint8* p, int split, int len)
{
	int i;
	char buf[512];
	int index = 0;
	for (i = 0; i < len; i++)
	{
		if (split != 0 && ((i + 1) % split) == 0)
		{
			index += sprintf(buf + index, "%02x,", p[i]);
		}
		else
			index += sprintf(buf + index, "%02x ", p[i]);
	}
	printf_high("Len=%d:%s\n",len, buf);
}

