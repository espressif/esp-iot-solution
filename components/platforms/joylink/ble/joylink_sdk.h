#ifndef JoyLink_SDK_h
#define JoyLink_SDK_h
#include "joylink_syshdr.h"

/******************************************************
 *                     Structures
 ******************************************************/
#pragma pack(1)

typedef struct{
	uint8_t count;
	uint8_t num;
	uint8_t keytype;
	uint8_t seq;
}frame_hdr_t;

typedef struct{
	frame_hdr_t hdr;
	uint8_t payload[16];
}jl_frame_t;

typedef  struct 
{
	uint8_t seq;
    uint8_t operation;
    uint16_t len;
    uint8_t content[1];
    //uint16_t crc;
}jl_packet_t;

typedef struct{
	uint16_t tag;
	uint8_t len;
	uint8_t value[1];
}jl_property_t;    //tlv

#pragma pack()

enum
{
	OP_APP_READ = 0x01,
	OP_DEV_RESP_READ = 0x11,
	OP_APP_WRITE_WITHOUT_RESP = 0x02,
	OP_APP_WRITE_WITH_RESP = 0x03,
	OP_DEV_RESP_WRITE = 0x13,
	//OP_DEV_SEND_NON_SAVE = 0x05,	
	//OP_DEV_SEND_SAVE = 0x15,	
	OP_DEV_WRITE_WITHOUT_RESP = 0x16,
	OP_DEV_WRITE_WITH_RESP = 0x17,
	OP_APP_RESP_WRITE = 0x07,
	OP_ACK = 0XFF
};

enum
{
	KEY_TYPE_PLAIN = 0x00,
	KEY_TYPE_PSK = 0x01,
	KEY_TYPE_LOCALKEY = 0x02,
	KEY_TYPE_SEC_ALGORITHM = 0x03,
	KEY_TYPE_ECDH = 0x4
};

int jl_init(uint8_t* zone, uint16_t size, uint8_t* zoneRcv, uint16_t rcv_size);

int jl_rcv_reset(void);

int jl_receive(uint8_t* frame);
int jl_send_seclevel_0(uint16_t length, uint8_t* privData);
int jl_send_seclevel_1(uint16_t length, uint8_t* privData);
int jl_send_seclevel_2(uint16_t length, uint8_t* privData);
int jl_send_seclevel_3(uint16_t length, uint8_t* privData);

int jl_indication_confirm_cb(void);

int jl_save_guid(unsigned char *guid);
int jl_get_guid(unsigned char *guid);
int jl_save_localkey(unsigned char *localkey);

/*test code*/
void test_jl_get_secretkey(unsigned char *l_secretkey);
void test_jl_get_pid(unsigned char *l_pid);
void test_jl_get_mac(unsigned char *l_mac);
int jl_secure_prepare(void);

#define ERR_NUM_UNORDER  (-201)
#define ERR_NUM_OVERFLOW (-202)
#define ERR_UNSUPPORT_KEY (-203)

#define RECEIVE_WAIT 0
#define RECEIVE_END   1
#endif

