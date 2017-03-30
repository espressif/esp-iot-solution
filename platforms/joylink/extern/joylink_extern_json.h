/* --------------------------------------------------
 * @file: joylink_extern_json.h
 *
 * @brief: 
 *
 * @version: 1.0
 *
 * @date: 10/26/2015 02:21:59 PM
 *
 * --------------------------------------------------
 */

#ifndef __JOYLINK_EXTERN_JSON__
#define __JOYLINK_EXTERN_JSON__

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */
int 
jl_parse_24g(WIFICtrl_t *wctr, char * pMsg);

int 
jl_parse_wan_speed(WIFICtrl_t *wctr, char * pMsg);

int 
jl_parse_jlp(JLPInfo_t *jlp, char * pMsg);

int 
joylink_parse_client_list(SubDev_t *sdev, int max, char * pMsg);

char * 
joylink_package_client_list_(SubDev_t *sdev, int count);

/**
 * brief: 
 * NOTE: If return is not NULL, 
 * must free it, after use.
 *
 * @Param: retMsg
 * @Param: retCode
 * @Param: wci
 * @Param: devlist
 *
 * @Returns: char * 
 */
char * 
packageGWInfo(const char *retMsg, 
        const int retCode, 
        const WIFICtrl_t *wci, 
        const char *devlist);

int 
parse_wifictrl_ssid(WIFIInfo_t *pw, char * pMsg);

int 
joylink_dev_parse_wifictrl_from_server(WIFICtrl_t *wctr, const char * pMsg);

int 
joylink_dev_parse_wifi_ctrl(WIFICtrl_t *wctr, const char * pMsg);

int
packageWIFIInfo(const WIFIInfo_t *wc, 
        const int max, 
        char * out_buff, 
        int g24);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
