#ifndef _JOYLINK_H_
#define _JOYLINK_H_

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

#include "joylink_log.h"
#include "auth/uECC.h"
#include "joylink_dev.h"
#include "joylink_ret_code.h"

#define JL_MAX_PACKET_LEN		    (1400)
#define JL_MAX_IP_LEN               (20)
#define JL_MAX_MAC_LEN              (128)
#define JL_MAX_UUID_LEN             (10)
#define JL_MAX_FEEDID_LEN           (33)
#define JL_MAX_KEY_BIN_LEN          (21)
#define JL_MAX_KEY_STR_LEN          (21*2+1)
#define JL_MAX_DBG_LEN              (128)
#define JL_MAX_OPT_LEN              (128)
#define JL_MAX_SERVER_HB_LOST       (10)
#define JL_MAX_SERVER_NUM           (5)
#define JL_MAX_SERVER_NAME_LEN      (128)
#define JL_MAX_SUB_DEV              (4)
#define JL_MAX_VERSION_NAME_LEN    	(100)
#define JL_MAX_URL_LEN             	(100)
#define JL_MAX_MSG_LEN 				(1024)
#define JL_MAX_STATUS_DESC_LEN      (100)

#define JL_SERVER_ST_INIT           (0)
#define JL_SERVER_ST_AUTH           (1)
#define JL_SERVER_ST_WORK           (2)

#define JL_BZCODE_GET_SNAPSHOT      (1004)
#define JL_BZCODE_CTRL              (1002)
#define JL_BZCODE_UNBIND            (1060)

#pragma pack(1)
typedef struct {
	unsigned int magic;
	unsigned int len;
	unsigned int enctype;
	unsigned char checksum;
}JLCommonHeader_t;

typedef struct {
	unsigned int type;
	unsigned char cmd[2];
}JLCmdHeader_t;

typedef struct {
	unsigned int	magic;
	unsigned short	optlen;
	unsigned short	payloadlen;

	unsigned char	version;
	unsigned char	type;
	unsigned char	total;
	unsigned char	index;

	unsigned char	enctype;
	unsigned char	reserved;
	unsigned short	crc;
}JLPacketHead_t;
#pragma pack()

#define JL_MAX_PHEAD_LEN		    (sizeof(JLPacketHead_t))
#define JL_MAX_PAYLOAD_LEN		    (JL_MAX_PACKET_LEN - JL_MAX_PHEAD_LEN - 20) 
#define JL_MAX_AES_EXTERN           (20) 

typedef enum {
	PT_UNKNOWN =        0,
	PT_SCAN =           1,
	PT_WRITE_ACCESSKEY = 2,
	PT_JSONCONTROL =    3,
	PT_SCRIPTCONTROL =  4,

	PT_OTA_ORDER =      7,
	PT_OTA_UPLOAD =     8,
	PT_AUTH =           9,
	PT_BEAT =           10,
	PT_SERVERCONTROL =  11,
	PT_UPLOAD =         12,

	PT_SUB_AUTH =       102,
	PT_SUB_LAN_JSON =   103,
	PT_SUB_LAN_SCRIPT = 104,
	PT_SUB_ADD =        105,
	PT_SUB_HB  =        110,
	PT_SUB_CLOUD_CTRL = 111,
	PT_SUB_UPLOAD =     112,
	PT_SUB_UNBIND =     113
}E_PacketType;

typedef enum {
	ET_NOTHING = 0,
	ET_PSKAES = 1,
	ET_ECDH = 2,
	ET_ACCESSKEYAES = 3,
	ET_SESSIONKEYAES = 4
}E_EncType;

typedef struct {
	int version;	
	E_PacketType type;
}JLPacketParam_t;

typedef enum _tran_type{
    E_SUBDEV_TRANS_TYPE_WIFI = 0,
    E_SUBDEV_TRANS_TYPE_ZIGBEE = 1,
    E_SUBDEV_TRANS_TYPE_BLE = 2,
    E_SUBDEV_TRANS_TYPE_433 = 3
}E_JLSubDevTransType_t;

typedef enum _cmd_type{
    E_CMD_TYPE_JSON = 0,
    E_CMD_TYPE_LUA_SCRIPT = 1
}E_JLCMDType_t;

typedef enum _lan_ctrl_type{
    E_LAN_CTRL_DISABLE = 0,
    E_LAN_CTRL_ENABLE = 1
}E_JLLanCtrlType_t;

typedef struct {
	int	isUsed;
	short version;
	char ip[JL_MAX_IP_LEN];
	int port;		

	char mac[JL_MAX_MAC_LEN];
	char uuid[JL_MAX_UUID_LEN];
	int lancon;			    // 1 suport Lan 
	int cmd_tran_type;			// (0:bin, 1:Lua, 2:Js)
    int devtype;
    int protocol;           // 0:WIFI 1:zigbee 2:bluetooth 3:433

	char feedid[JL_MAX_FEEDID_LEN];
	char accesskey[33];
    char localkey[33];

	uint8_t pubkeyC[JL_MAX_KEY_BIN_LEN];
	char pubkeyS[JL_MAX_KEY_STR_LEN];

	char devdbg[JL_MAX_DBG_LEN];
	char servropt[JL_MAX_OPT_LEN];

	uint8_t sharedkey[uECC_BYTES];
	uint8_t sessionKey[33];

    char joylink_server[JL_MAX_SERVER_NAME_LEN];
    int server_port;

    char CID[10];
    char firmwareVersion[10];
    char modelCode[66];

    unsigned int crc32;
}JLPInfo_t;

typedef struct {
    JLPInfo_t jlp;
    WIFICtrl_t wifi;

    int server_socket;
    int lan_socket;
    int server_st;	 // 0:Socket init 1:Auth 2:HB
    int hb_lost_count;
    short local_port;

    uint8_t dev_detail[JL_MAX_PACKET_LEN];
    uint8_t send_buff[JL_MAX_PACKET_LEN];

    /**
     *only for package bigger than 1400
     */
    char *send_p;
    int payload_total;
}JLDevice_t;

typedef enum _dev_type{
    E_JLDEV_TYPE_NORMAL = 0,
    E_JLDEV_TYPE_GW = 1,
    E_JLDEV_TYPE_SUB = 2
}E_JLDevType_t;

typedef enum _dev_state{
    E_JLDEV_ST_NO_CONF = 0,
    E_JLDEV_ST_CONFD = 1
}E_JLDevState_t;

typedef struct {
    JLPInfo_t jlp;
    E_JLDevType_t type;
    int state;
    char msg[250];
    void *data;
}JLDevInfo_t;

typedef struct __dev_enable{
    char feedid[34];
    char accesskey[34];
    char localkey[34];
    char productuuid[10];
    char lancon;
    char cmd_tran_type;
    char joylink_server[JL_MAX_SERVER_NAME_LEN];
    int server_port;
    char opt[JL_MAX_SERVER_NAME_LEN];
}DevEnable_t;

typedef enum __scan_type{
    E_SCAN_TYPE_ALL = 0,
    E_SCAN_TYPE_WAITCONF = 1,
    E_SCAN_TYPE_CONFIGED = 2
}E_ScanType_t;

typedef enum _protocol_rec_st{
    E_PT_REV_ST_MAGIC = 0,
    E_PT_REV_ST_HEAD = 1,
    E_PT_REV_ST_DATA = 2,
    E_PT_REV_ST_END = 3
}E_PT_REV_ST_t;

typedef struct __lan_scan{
    char uuid[JL_MAX_UUID_LEN];
    E_ScanType_t type;
}DevScan_t;

typedef struct {
    unsigned int timestamp;
	unsigned int random_unm;
}JLAuth_t;

typedef struct {
	unsigned int timestamp;
	unsigned int random_unm;
	unsigned char session_key[];
}JLAuthRsp_t;

typedef struct {
	unsigned int timestamp; 
	unsigned short verion;
	unsigned short rssi;
}JLHearBeat_t;

typedef struct {
	unsigned int timestamp;
	unsigned int code;
}JLHearBeatRst_t;

typedef struct {
	unsigned int timestamp;
	unsigned int biz_code;
	unsigned int serial;
    char feedid[JL_MAX_FEEDID_LEN];
	unsigned char cmd[];
}JLContrl_t;

typedef struct {
	unsigned int timestamp;
	unsigned int biz_code;
	unsigned int serial;
	unsigned char resp_length[4];
	unsigned char resp[1];
	unsigned char streams[1];
}JLContrlRsp_t;

typedef struct {
    unsigned int timestamp;
    unsigned char data[2];
}JLDataUpload_t;

typedef struct {
	unsigned int timestamp;
	unsigned int code;
}JLDataUploadRsp_t;

typedef struct{
	int serial;
    char feedid[JL_MAX_FEEDID_LEN];
    char productuuid[JL_MAX_UUID_LEN];
    int version;
    char versionname[JL_MAX_VERSION_NAME_LEN];
    unsigned int crc32;
    char url[JL_MAX_URL_LEN];
}JLOtaOrder_t;

typedef struct{
    char feedid[JL_MAX_FEEDID_LEN];
    char productuuid[JL_MAX_UUID_LEN];
    int status;
    char status_desc[JL_MAX_STATUS_DESC_LEN];
    int progress;
}JLOtaUpload_t;

typedef struct {
	int code;
	char msg[JL_MAX_MSG_LEN];
}JLOtaUploadRsp_t;

void joylink_server_ota_status_upload_req(JLOtaUpload_t *otaUpload);

extern JLDevice_t  *_g_pdev;

typedef struct sockaddr SOCKADDR;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
