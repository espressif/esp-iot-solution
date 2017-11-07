/* --------------------------------------------------
 * @brief: 
 *
 * @version: 1.0
 *
 * @date: 10/08/2015 09:28:27 AM
 *
 * @author: yangzhongxuan , yangzhongxuan@gmail.com
 * --------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#include "joylink_json.h"
#include "joylink_log.h"

int 
parse_wifictrl_ssid(WIFIInfo_t *pw, char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == pw){
        return ret;
    }
    printf("entry %s\r\n",__func__);
    log_debug("json_org:%s", pMsg);
    cJSON * pV = cJSON_Parse(pMsg);
    if(NULL == pV){
      log_error("--->:ERROR: pJson is NULL\n");
      goto RET;
    }

    cJSON * pSubOpt= cJSON_GetObjectItem(pV, "disabled");
    if(NULL != pSubOpt){
        pw->disabled = pSubOpt->valueint; 
    }

    pSubOpt= cJSON_GetObjectItem(pV, "hidden");
    if(NULL != pSubOpt){
        pw->hidden = pSubOpt->valueint; 
    }

    pSubOpt= cJSON_GetObjectItem(pV, "ssid");
    if(NULL != pSubOpt){
        strcpy(pw->ssid, pSubOpt->valuestring);
    }

    pSubOpt= cJSON_GetObjectItem(pV, "encryption");
    if(NULL != pSubOpt){
        strcpy(pw->encryption, pSubOpt->valuestring);
    }

    pSubOpt= cJSON_GetObjectItem(pV, "key");
    if(NULL != pSubOpt){
        strcpy(pw->key, pSubOpt->valuestring);
    }
    cJSON_Delete(pV);
    ret = 0;
RET:
    return ret;
}

int 
joylink_dev_parse_wifictrl_from_server(WIFICtrl_t *wctr, const char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == wctr){
        return ret;
    }
    printf("entry %s\r\n",__func__);
    log_debug("json_org:%s", pMsg);
    cJSON * pSub;
    cJSON * pJson = cJSON_Parse(pMsg);
    WIFIInfo_t *pw = NULL;

    if(NULL == pJson){
      log_error("--->:ERROR: pMsg is NULL\n");
      goto RET;
    }

    char tmp_str[64];
    cJSON *pStreams = cJSON_GetObjectItem(pJson, "streams");
    if(NULL != pStreams){
        int iSize = cJSON_GetArraySize(pStreams);
        int iCnt;
        for( iCnt = 0; iCnt < iSize; iCnt++){
            pSub = cJSON_GetArrayItem(pStreams, iCnt);
            if(NULL == pSub){
                continue;
            }

            cJSON *pSId = cJSON_GetObjectItem(pSub, "stream_id");
            if(NULL != pSId){
                pw = NULL;
                cJSON *pV = cJSON_GetObjectItem(pSub, "current_value");
                if(NULL == pV){
                    continue; 
                } 
                log_debug("------:%s", pV->valuestring);

                if(!strcmp(pSId->valuestring, "wifi_24g_info")){
                    pw = &wctr->_24g; 
                }else if(!strcmp(pSId->valuestring, "wifi_5g_info")){
                    pw = &wctr->_5g; 
                }else if(!strcmp(pSId->valuestring, "wifi_24g")){
                    strcpy(tmp_str, pV->valuestring);
                    wctr->_24g.on = atoi(tmp_str);
                }else if(!strcmp(pSId->valuestring, "wifi_5g")){
                    strcpy(tmp_str, pV->valuestring);
                    wctr->_5g.on = atoi(tmp_str);
                }

                if(NULL != pw){
                    parse_wifictrl_ssid(pw, pV->valuestring);
                }

                log_debug("24g on:%d", wctr->_24g.on);
                char *dout = cJSON_Print(pSub);
                if(NULL != dout){
                    log_debug("org streams:%s", dout);
                    free(dout);
                }
            }//for

        }   
    }                                                                                                         
    cJSON_Delete(pJson);
RET:
    return ret;
}

int 
joylink_dev_parse_wifi_ctrl(WIFICtrl_t *wctr, const char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == wctr){
        return ret;
    }
    log_debug("json_org:%s", pMsg);
    cJSON * pSub;
    cJSON * pJson = cJSON_Parse(pMsg);
    WIFIInfo_t *pw = NULL;
printf("entry %s\r\n",__func__);
    if(NULL == pJson){
      log_error("--->:ERROR: pMsg is NULL\n");
      goto RET;
    }

    char tmp_str[64];
    cJSON *pStreams = cJSON_GetObjectItem(pJson, "streams");
    if(NULL != pStreams){
        int iSize = cJSON_GetArraySize(pStreams);
        int iCnt;
        for( iCnt = 0; iCnt < iSize; iCnt++){
            pSub = cJSON_GetArrayItem(pStreams, iCnt);
            if(NULL == pSub){
                continue;
            }

            cJSON *pSId = cJSON_GetObjectItem(pSub, "stream_id");
            if(NULL != pSId){
                pw = NULL;
                cJSON *pV = cJSON_GetObjectItem(pSub, "current_value");
                if(NULL == pV){
                    continue; 
                } 

                if(!strcmp(pSId->valuestring, "wifi_24g_info")){
                    pw = &wctr->_24g; 
                }else if(!strcmp(pSId->valuestring, "wifi_5g_info")){
                    pw = &wctr->_5g; 
                }else if(!strcmp(pSId->valuestring, "wifi_24g")){
                    strcpy(tmp_str, pV->valuestring);
                    wctr->_24g.on = atoi(tmp_str);
                }else if(!strcmp(pSId->valuestring, "wifi_5g")){
                    strcpy(tmp_str, pV->valuestring);
                    wctr->_5g.on = atoi(tmp_str);
                }

                if(NULL != pw){
                    cJSON * pSubOpt= cJSON_GetObjectItem(pV, "disabled");
                    if(NULL != pSubOpt){
                        pw->disabled = pSubOpt->valueint; 
                    }

                    pSubOpt= cJSON_GetObjectItem(pV, "hidden");
                    if(NULL != pSubOpt){
                        pw->hidden = pSubOpt->valueint; 
                    }

                    pSubOpt= cJSON_GetObjectItem(pV, "ssid");
                    if(NULL != pSubOpt){
                        strcpy(pw->ssid, pSubOpt->valuestring);
                    }

                    pSubOpt= cJSON_GetObjectItem(pV, "encryption");
                    if(NULL != pSubOpt){
                        strcpy(pw->encryption, pSubOpt->valuestring);
                    }

                    pSubOpt= cJSON_GetObjectItem(pV, "key");
                    if(NULL != pSubOpt){
                        strcpy(pw->key, pSubOpt->valuestring);
                    }

                }

                log_debug("24g on:%d", wctr->_24g.on);
                char *dout = cJSON_Print(pSub);
                if(NULL != dout){
                    log_debug("org streams:%s", dout);
                    free(dout);
                }
            }//for

        }   
    }                                                                                                         
    cJSON_Delete(pJson);
RET:
    return ret;
}

int
packageWIFIInfo(const WIFIInfo_t *wc, const int max, char * out_buff, int g24)
{
    int ret = -1;
    if(NULL == wc || NULL == out_buff){
        return ret;
    }
    cJSON *root,*fmt; char *out;
    cJSON *wifi_xg; 
printf("entry %s\r\n",__func__);
    root=cJSON_CreateObject();  
    if(NULL == root){
        goto RET;
    }

    if(g24){
        wifi_xg  = cJSON_CreateString("wifi_24g_info");
    }else{
        wifi_xg  = cJSON_CreateString("wifi_5g_info");
    }

    if(NULL != wifi_xg){
        cJSON_AddItemToObject(root, "stream_id", wifi_xg);
    }
    if(NULL != (fmt=cJSON_CreateObject())){
        cJSON_AddItemToObject(root, "current_value", fmt);
        cJSON_AddStringToObject(fmt,"ssid",     wc->ssid);
        cJSON_AddStringToObject(fmt,"encryption",     wc->encryption);
        cJSON_AddStringToObject(fmt,"key",     wc->key);
        cJSON_AddNumberToObject(fmt,"disable",      wc->disabled);
        cJSON_AddNumberToObject(fmt,"hidden",       wc->hidden);
    }
    
    out=cJSON_Print(root);  
    cJSON_Delete(root); 

    if(NULL != out && strlen(out) < max){
        strcpy(out_buff, out);
        free(out);  
    }else{
        log_error("-->ERROR array break!\n");
    }

    ret = 0;
RET:
    return ret;
}

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
packageGWInfo(const char *retMsg, const int retCode, 
        const WIFICtrl_t *wci, const char *devlist)
{
    if(NULL == retMsg || NULL == wci){
        return NULL;
    }
    cJSON *root, *arrary;
    char *out  = NULL; 
printf("entry %s\r\n",__func__);
    root = cJSON_CreateObject();
    if(NULL == root){
        goto RET;
    }

    arrary = cJSON_CreateArray();
    if(NULL == arrary){
        cJSON_Delete(root);
        goto RET;
    }

    cJSON_AddNumberToObject(root, "code", retCode);

    cJSON_AddItemToObject(root, "streams", arrary);

    char i2str[32];
    bzero(i2str, sizeof(i2str));
    cJSON *w24g;
    cJSON_AddItemToArray(arrary,w24g=cJSON_CreateObject());
    cJSON_AddStringToObject(w24g, "stream_id", "wifi_24g");
    bzero(i2str, sizeof(i2str));
    sprintf(i2str, "%d", wci->_24g.on);
    cJSON_AddStringToObject(w24g, "current_value", i2str);

    cJSON *w5g;
    cJSON_AddItemToArray(arrary,w5g=cJSON_CreateObject());
    cJSON_AddStringToObject(w5g, "stream_id", "wifi_5g");
    bzero(i2str, sizeof(i2str));
    sprintf(i2str, "%d", wci->_5g.on);
    cJSON_AddStringToObject(w5g, "current_value", i2str );

    cJSON *downs;
    cJSON_AddItemToArray(arrary,downs=cJSON_CreateObject());
    cJSON_AddStringToObject(downs, "stream_id", "downspeed");
    bzero(i2str, sizeof(i2str));
    sprintf(i2str, "%7.4f", wci->downspeed/1024.0);
    cJSON_AddStringToObject(downs, "current_value", i2str); 

    out=cJSON_Print(downs);  
    if(NULL != out){
        log_debug("upspeed:%s", out); 
        free(out);
    }

    cJSON *ups;
    cJSON_AddItemToArray(arrary,ups=cJSON_CreateObject());
    cJSON_AddStringToObject(ups, "stream_id", "upspeed");
    bzero(i2str, sizeof(i2str));
    sprintf(i2str, "%7.4f", wci->upspeed/1024.0);
    cJSON_AddStringToObject(ups, "current_value",i2str);  

    out=cJSON_Print(ups);  
    if(NULL != out){
        log_debug("upspeed:%s", out); 
        free(out);
    }

    char w24gbuff[512] = {0};
    packageWIFIInfo(&wci->_24g, sizeof(w24gbuff), w24gbuff, WIFI_INFO_24G);
    cJSON *w24g_info = cJSON_Parse(w24gbuff);
    if(w24g_info){
        cJSON_AddItemToArray(arrary,w24g_info);
    }

    char w5gbuff[512] = {0};
    packageWIFIInfo(&wci->_5g, sizeof(w5gbuff), w5gbuff, WIFI_INFO_5G);
    cJSON *w5g_info = cJSON_Parse(w5gbuff);
    if(w5g_info){
        cJSON_AddItemToArray(arrary,w5g_info);
    }

    if(NULL != devlist){
        cJSON *devs_ob;
        cJSON_AddItemToArray(arrary,devs_ob=cJSON_CreateObject());
        cJSON_AddStringToObject(devs_ob, "stream_id", "devlist");
        cJSON_AddStringToObject(devs_ob, "current_value",devlist);  
    }

    out=cJSON_Print(root);  
    cJSON_Delete(root); 
RET:
    return out;
}

int 
jl_parse_24g(WIFICtrl_t *wctr, char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == wctr){
        return ret;
    }
    cJSON *pVal;
    cJSON * pJson = cJSON_Parse(pMsg);
    char strv[64];
    int on_off;
printf("entry %s\r\n",__func__);
    if(NULL == pJson){
      log_error("--->:ERROR: pMsg is NULL\n");
      goto RET;
    }

    pVal = cJSON_GetObjectItem(pJson, "enabled");
    if(NULL != pVal){
        strcpy(strv, pVal->valuestring);
        on_off = atoi(strv);
        wctr->_24g.on = on_off? 0:1; 
    }

    pVal = cJSON_GetObjectItem(pJson, "ssid");
    if(NULL != pVal){
        strcpy(wctr->_24g.ssid, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "type");
    if(NULL != pVal){
        strcpy(strv, pVal->valuestring);
        if(!strcmp(strv, "0")){
            strcpy(wctr->_24g.encryption, "none");
        }else{
            strcpy(wctr->_24g.encryption, "mixed-psk");
        }
    }

    pVal = cJSON_GetObjectItem(pJson, "passphrase");
    if(NULL != pVal){
        strcpy(wctr->_24g.key, pVal->valuestring);
    }

    ret = 0;
    cJSON_Delete(pJson);
RET:
    return ret;
}

int 
jl_parse_wan_speed(WIFICtrl_t *wctr, char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == wctr){
        return ret;
    }
    cJSON *pVal;
    cJSON * pJson = cJSON_Parse(pMsg);
    char strv[128];
printf("entry %s\r\n",__func__);
    if(NULL == pJson){
      log_error("--->:ERROR: pjson is NULL\n");
      goto RET;
    }

    log_debug("org wlanspeed:%s", pMsg);
    pVal = cJSON_GetObjectItem(pJson, "dlspeed");
    if(NULL != pVal){
        strcpy(strv, pVal->valuestring);
        log_debug("dlspeed:%s", strv);
        wctr->downspeed = atoi(strv);
    }

    pVal = cJSON_GetObjectItem(pJson, "ulspeed");
    if(NULL != pVal){
        strcpy(strv, pVal->valuestring);
        log_debug("ulspeed:%s", strv);
        wctr->upspeed = atoi(strv);
    }
    cJSON_Delete(pJson);
    ret  = 0;
RET:
    return ret;
}

int 
jl_parse_jlp(JLPInfo_t *jlp, char * pMsg)
{
    int ret = -1;
    if(NULL == pMsg || NULL == jlp){
        return ret;
    }
    printf("entry %s\r\n",__func__);
    cJSON *pVal;
    cJSON * pJson = cJSON_Parse(pMsg);

    if(NULL == pJson){
      log_error("--->:ERROR: pMsg is NULL\n");
      goto RET;
    }

    pVal = cJSON_GetObjectItem(pJson, "uuid");
    if(NULL != pVal){
        strcpy(jlp->uuid, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "feedid");
    if(NULL != pVal){
        strcpy(jlp->feedid, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "accesskey");
    if(NULL != pVal){
        strcpy(jlp->accesskey, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "localkey");
    if(NULL != pVal){
        strcpy(jlp->localkey, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "macaddr");
    if(NULL != pVal){
        strcpy(jlp->mac, pVal->valuestring);
    }

    pVal = cJSON_GetObjectItem(pJson, "version");
    if(NULL != pVal){
        jlp->version = pVal->valueint;
    }

    cJSON_Delete(pJson);
    ret = 0;
RET:
    return ret;
}

int 
joylink_parse_client_list(SubDev_t *sdev, int max, char * pMsg)
{
    int ret = -1;
    if(NULL == sdev || NULL == pMsg){
        return ret;
    }
    printf("entry %s\r\n",__func__);
    cJSON *pVal, *pSub;
    cJSON * pJson = cJSON_Parse(pMsg);

    if(NULL == pJson){
      log_error("--->:ERROR: pJson is NULL json data error\n");
      goto RET;
    }

    int iSize = cJSON_GetArraySize(pJson);
    int iCnt, count = 0;
    char *out;
    for( iCnt = 0; iCnt < iSize; iCnt++){
        pSub = cJSON_GetArrayItem(pJson, iCnt);
        if(NULL == pSub){
            continue;
        }

        pVal= cJSON_GetObjectItem(pSub, "hostname");
        if(NULL != pVal){
            strcpy(sdev[count].device_name, pVal->valuestring);
        }

        pVal= cJSON_GetObjectItem(pSub, "mac");
        if(NULL != pVal){
            strcpy(sdev[count].mac, pVal->valuestring);
        }

        pVal= cJSON_GetObjectItem(pSub, "ip");
        if(NULL != pVal){
            strcpy(sdev[count].ip, pVal->valuestring);
        }

        pVal= cJSON_GetObjectItem(pSub, "lastonlinetime");
        if(NULL != pVal){
            strcpy(sdev[count].lastest_online_time, pVal->valuestring);
        }

        pVal= cJSON_GetObjectItem(pSub, "linktype");
        if(NULL != pVal){
            sdev[count].type = pVal->valueint;
        }

        pVal= cJSON_GetObjectItem(pSub, "dlspeed");
        if(NULL != pVal){
            sdev[count].down = pVal->valueint;
        }

        pVal= cJSON_GetObjectItem(pSub, "ulspeed");
        if(NULL != pVal){
            sdev[count].up = pVal->valueint;
        }

        out = cJSON_Print(pSub); 
        if(NULL != out){
            log_debug("dev info:%s", out);
            free(out);
        }

        log_debug("uspeed:%d, dspeed:%d", sdev[count].up, sdev[count].down);
        count ++;
    }
    cJSON_Delete(pJson);
    ret = count;

RET:
    return ret;
}

char * 
joylink_package_client_list_(SubDev_t *sdev, int count)
{
    if(NULL == sdev){
        return NULL;
    }
    printf("entry %s\r\n",__func__);
    cJSON *arrary, *js_devs[JL_MAX_SUB_DEV];
    char *out = NULL; 
    unsigned int i; 
    int sub_count = count > JL_MAX_SUB_DEV ? JL_MAX_SUB_DEV:count; 
    char i2str[10];

    if(NULL == (arrary = cJSON_CreateArray())){
        goto RET;
    }

    for(i = 0; i < sub_count; i ++){
        js_devs[i] =cJSON_CreateObject();
        if(NULL != js_devs[i]){
            cJSON_AddItemToArray(arrary, js_devs[i]);
            cJSON_AddStringToObject(js_devs[i], "mac", sdev[i].mac);
            sprintf(i2str, "%7.4f", sdev[i].up/1024.0);
            cJSON_AddStringToObject(js_devs[i], "up", i2str);
            sprintf(i2str, "%7.4f", sdev[i].down/1024.0);
            cJSON_AddStringToObject(js_devs[i], "down", i2str);
            cJSON_AddStringToObject(js_devs[i], "ip", sdev[i].ip);
            cJSON_AddStringToObject(js_devs[i], "device_name", sdev[i].device_name );
            cJSON_AddStringToObject(js_devs[i], "lastest_online_time", sdev[i].lastest_online_time);
        }
    }

    out=cJSON_Print(arrary);  
    cJSON_Delete(arrary);
RET:
    return out;
}
