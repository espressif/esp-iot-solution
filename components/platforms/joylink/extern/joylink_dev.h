#ifndef _JOYLINK_DEV_H_
#define _JOYLINK_DEV_H_

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

#define WIFI_CTRL_ON            (0)
#define WIFI_CTRL_OFF           (1)
#define WIFI_CTRL_DISABLE       (2)

#define WIFI_INFO_24G           (1)
#define WIFI_INFO_5G            (0)

typedef struct __wifi_info{
    char on;
    char disabled;
    char hidden;
    char ssid[254];
    char encryption[254];
    char key[254];
    char at[254];
}WIFIInfo_t;

typedef struct __wifi_ctrl{
    char cmd;
    char online_state;
    int downspeed;
    int upspeed;
    WIFIInfo_t _24g;
    WIFIInfo_t _5g;
}WIFICtrl_t;

typedef struct __dev_info{
    int up[32];
    int down[32];
    char mac[64];
    char ip[64];
    char name[254];
    char last_on_time[254];
}DevInfo_t;

/**
 *SubDev_t
 *Only for tenda test
 */
typedef struct {
    int up;
    int down;
    char mac[32];
    char ip[64];
    char device_name[256];
    char lastest_online_time[64];
    int type;
}SubDev_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
