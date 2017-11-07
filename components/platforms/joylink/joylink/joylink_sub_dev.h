#ifndef __JOYLINK_SUB_DEV_H__
#define __JOYLINK_SUB_DEV_H__

#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(1)
typedef struct {
	unsigned int 	timestamp;
	unsigned int	code;
}JLDataSubHbRsp_t;

typedef struct {
	unsigned int timestamp;
	unsigned char feedid[JL_MAX_FEEDID_LEN];
	unsigned int code;
}JLDataSubUploadRsp_t;

typedef struct {
	unsigned short	version;
	unsigned short	rssi;
	unsigned char 	feedid[JL_MAX_FEEDID_LEN];
	unsigned char state;
}JLSubDevInfo_t;

typedef struct {
	unsigned int timestamp; 
	unsigned short	verion;
	unsigned short	rssi;
	unsigned short num;
	JLSubDevInfo_t sdevinfo[];
}JLSubHearBeat_t;

typedef struct {
	unsigned int timestamp;
	unsigned int biz_code;
	unsigned int	 serial;
    char feedid [JL_MAX_FEEDID_LEN];
	unsigned char *cmd;
}JLSubContrl_t;

typedef struct {
	unsigned int	timestamp;
	unsigned int    biz_code;
	unsigned int	serial;
	unsigned short  num;
	JLSubDevInfo_t 	*info;
}JLSubUnbind_t;
#pragma pack()

E_JLRetCode_t 
joylink_proc_lan_sub_add(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen);

E_JLRetCode_t 
joylink_proc_lan_sub_auth(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen);

E_JLRetCode_t 
joylink_proc_lan_sub_script_ctrl(uint8_t *src, int src_len, struct sockaddr_in *sin_recv, socklen_t addrlen);

int
joylink_packet_server_sub_hb_req(void);

//yn
int 
joylink_subdev_script_ctrl(uint8_t* recPainText, JLSubContrl_t *ctr, unsigned short payloadlen);
//yn
int 
joylink_packet_subdev_script_ctrl_rsp(char *data, int max, JLSubContrl_t *ctrl);

int
joylink_subdev_unbind(uint8_t* recPainText, JLSubUnbind_t* unbind);

int
joylink_packet_subdev_unbind_rsp(char* data, int max, JLSubUnbind_t* unbind);
#ifdef __cplusplus
}
#endif

#endif /* __SUB_DEV_H__*/
