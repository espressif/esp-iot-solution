#ifndef _JOYLINK_THUNDERCONFIG_UTILS_H_
#define _JOYLINK_THUNDERCONFIG_UTILS_H_

#include "thunderconfig_types.h"

#define CONFIGED_DEVICE 	0
//JYLK
#define JL_THUNDERCFG_HEAD 					0x4A594C4B
#define JL_THUNDERCFG_HEAD_LEN 				4

#define JL_THUNDERCFG_CID_LEN 				4
#define JL_THUNDERCFG_PUID_LEN 				6
#define JL_THUNDERCFG_MAC_LEN 				6
#define JL_THUNDERCFG_IDENTITY_LEN			32
#define	JL_THUNDERCFG_SSID_LEN				32
#define JL_THUNDERCFG_PWD_LEN 				32
#define JL_THUNDERCFG_DATA_LEN				128

#define JL_THUNDERCFG_PRIVATE_KEY_LEN 		32
#define JL_THUNDERCFG_PUB_KEY_LEN 	    	64
#define JL_THUNDERCFG_COMPRESSED_KEY_LEN 	33

#define JL_THUNDERCFG_INTERVAL_MS			200
#define JL_THUNDERCFG_SCAN_INTERVAL			600
#define JL_THUNDERCFG_RX_INTERVAL			6000

#define JL_THUNDERCFG_RESET_NORMAL			3
#define JL_THUNDERCFG_RESET_DELAY			30

typedef enum {
	JL_FRAME_SUBTYPE_UNKNOWN	= -1,
	JL_FRAME_SUBTYPE_DATA 		= 0,
	JL_FRAME_SUBTYPE_BEACON 	= 8	
} JL_FRAME_SUBTYPE;

typedef enum {
	JL_THUNDERCFG_STATE_TX_REQUEST  = 0,
	JL_THUNDERCFG_STATE_TX_REPLY    = 1,
	JL_THUNDERCFG_STATE_TX_FINISH   = 2,
	JL_THUNDERCFG_STATE_TX_NOTHING  = 3
} JL_THUNDERCFG_STATE_TX;

typedef enum {
	JL_THUNDERCFG_STATE_RX_AP 	   = 0,
	JL_THUNDERCFG_STATE_RX_FINISH  		= 1,
	JL_THUNDERCFG_STATE_RX_NOTHING 		= 2,
	JL_THUNDERCFG_STATE_RX_BROADCAST 	= 3,
} JL_THUNDERCFG_STATE_RX;

typedef struct _jl_frame_beacon {
	uint8_t ssid[JL_THUNDERCFG_SSID_LEN];
	uint8_t ssid_len;
	uint8_t vendor_specific[JL_THUNDERCFG_DATA_LEN];
	uint8_t vendor_specific_len;
} jl_frame_beacon_t;

typedef struct _jl_frame_data {
	uint8_t raw_data[JL_THUNDERCFG_DATA_LEN];
	uint8_t raw_data_len;
} jl_frame_data_t;

typedef struct _jl_device_info {	
	uint8_t cid[JL_THUNDERCFG_CID_LEN];
	uint8_t puid[JL_THUNDERCFG_PUID_LEN];
	uint8_t mac[JL_THUNDERCFG_MAC_LEN];
} jl_device_info_t;

typedef struct _jl_ap_info {	
	uint8_t ssid[JL_THUNDERCFG_SSID_LEN];
	uint8_t ssid_len;
	uint8_t pwd[JL_THUNDERCFG_PWD_LEN];
	uint8_t pwd_len;
} jl_ap_info_t;

typedef struct _jl_key_pair{	
	uint8_t pub_key[JL_THUNDERCFG_PUB_KEY_LEN];	
	uint8_t private_key[JL_THUNDERCFG_PRIVATE_KEY_LEN];
} jl_key_pair_t;

typedef struct _jl_crypt_info {
	jl_key_pair_t local_key_pair;
	uint8_t remote_shared_key[JL_THUNDERCFG_PRIVATE_KEY_LEN];	
} jl_crypt_info_t;

typedef struct _jl_init_param {
jl_device_info_t device_info;
void *switch_channel_cb;
void *config_finish_cb;
void *frame_tx_cb;
void *printf_fuc;
} jl_init_param_t;

typedef struct _jl_thunderconfig {
	uint8_t identity[JL_THUNDERCFG_IDENTITY_LEN];	
	jl_crypt_info_t crypt_info;	
	jl_ap_info_t	ap_info;	
	JL_THUNDERCFG_STATE_TX tx_state;
	JL_THUNDERCFG_STATE_RX rx_state;
	JL_THUNDERCFG_STATE_RX last_rx_state;
	uint8_t rx_counter;
	uint8_t current_channel;
	void *switch_channel_cb;
	void *config_finish_cb;
} jl_thunderconfig_t;

typedef int (*printf_fuc_thunder_t)(char *fmt, ...);


/**
 * Callback function prototype for frame transmitting.
 *
 * @param frame_type The type of frame of 802.11 used.
 * @param data The data to be transmited.
 *
 * @return  Returns 0 if succeed; -1 otherwise.
 *
 * @remark 
 *	 The type of data is jl_beacon_rx_t when frame_type
 *   is JL_FRAME_TYPE_BEACON.
 *
 *	 The type of data is jl_data_rx_t when frame_type
 *   is JL_FRAME_TYPE_DATA.
 */
typedef int (*jl_frame_tx_cb_t)(JL_FRAME_SUBTYPE frame_type, void *frmae);

typedef int (*jl_switch_channel_cb_t)(uint8_t channel);

typedef void (*jl_config_finish_cb_t)(jl_ap_info_t ap_info);

extern printf_fuc_thunder_t thunderconfig_utils_print;

//extern void thunderconfig_utils_print(const char *fmt, ...);

extern void thunderconfig_utils_print_hex(char *tittle, 
										   		  const uint8_t *dest, 
										   		  uint8_t size);

extern int thunderconfig_utils_make_session_key(const uint8_t *r1, 
														const uint8_t *r2, 
														uint8_t *key);

extern int thunderconfig_utils_random(uint8_t *dest, uint8_t size);

extern uint16_t thunderconfig_utils_htons(uint16_t value);

extern uint32_t thunderconfig_utils_htonl(uint32_t value);

extern uint16_t thunderconfig_utils_nstoh(uint16_t value);

extern uint32_t thunderconfig_utils_nltoh(uint32_t value);

extern int thunderconfig_utils_byte2hex(const uint8_t *pbytes,
												int blen, 
									     		uint8_t *o_phex, 
									     		int hlen);

extern int thunderconfig_utils_hex2bytes(const char *phex,
											     int hex_len,
												 uint8_t *o_buf, 
												 int buf_len);

extern void thunderconfig_pringf_init(void *func);


#endif /*!_JOYLINK_THUNDERCONFIG_UTILS_H_*/
