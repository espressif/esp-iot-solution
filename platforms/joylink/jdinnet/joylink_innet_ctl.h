#ifndef _JOYLINK_INNET_CTL_H_
#define _JOYLINK_INNET_CTL_H_
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "joylink_smnt.h"
#include "thunderconfig.h"

#define SIZE_CID 			4
#define SIZE_PUID			6
#define SIZE_MAC			6
#define SIZE_SMNT_KEY		16


typedef struct _joylink_innet_init_param 
{
	uint8_t cid[SIZE_CID];
	uint8_t puid[SIZE_PUID];
	uint8_t mac[SIZE_MAC];
	uint8_t smnt_key[SIZE_SMNT_KEY];
	
	void 	*switch_channel_cb;
	void 	*config_finish_cb;
	void 	*frame_tx_cb;
	void	*printf_func;
} joylink_innet_param_t;

typedef enum 
{
	FRAME_SUBTYPE_UNKNOWN				= JL_FRAME_SUBTYPE_UNKNOWN,
	FRAME_SUBTYPE_RAWDATA 				= JL_FRAME_SUBTYPE_DATA,
	FRAME_SUBTYPE_BEACON 				= JL_FRAME_SUBTYPE_BEACON,
	FRAME_SUBTYPE_UDP_MULTIANDBROAD		= JL_FRAME_SUBTYPE_BEACON + 1	//udp multicast and broadcast data,used for SMNT
} joylink_rx_type_t;

typedef struct
{
	PHEADER_802_11_SMNT head_802_11;
	int 	length;
}joylink_frame_udp_t;

typedef struct 
{	
	uint8 ssid[33];
	uint8 ssid_len;
	uint8 pwd[33];
	uint8 pwd_len;
} joylink_innet_result_t;

typedef void (*joylink_innet_result_callback_t)(joylink_innet_result_t result_info);
typedef int (*joylink_switch_channel_cb_t)(uint8_t channel);

int joylink_innet_init(joylink_innet_param_t init_para);
int joylink_innet_timingcall(void);
int joylink_innet_rx_handler(joylink_rx_type_t frame_type, void *frame);

#endif






