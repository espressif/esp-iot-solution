#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#ifndef PLATFORM_ESP32
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#else
#include <sys/socket.h>
#endif

#include "joylink.h"
#include "joylink_utils.h"
#include "joylink_packets.h"
#ifndef PLATFORM_ESP32
#include "joylink_crypt.h"
#else
#include "auth/joylink_crypt.h"
#endif
#include "joylink_json.h"
#include "joylink_extern.h"
#include "joylink_sub_dev.h"
#include "joylink_join_packet.h"
#include "auth/crc.h"

extern int joylink_server_upload_req();

/**
 * brief: 1 get packet head and opt
 *        2 cut big packet as 1024
 *        3 join phead, opt and paload
 *        4 crc
 *        5 send
 */
#define JL_MAX_CUT_PACKET_LEN  (1024)
void
joylink_send_big_pkg(struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len;
    int i;
    int offset = 0;

	JLPacketHead_t *pt = (JLPacketHead_t*)_g_pdev->send_p;
    short res = pt->crc;

    int total = (int)(pt->payloadlen/JL_MAX_CUT_PACKET_LEN) 
        + (pt->payloadlen%JL_MAX_CUT_PACKET_LEN? 1:0);

    for(i = 1; i <= total; i ++){
        if(i == total){
            len = pt->payloadlen%JL_MAX_CUT_PACKET_LEN;
        }else{
            len = JL_MAX_CUT_PACKET_LEN;
        }
        
        offset = 0;
        bzero(_g_pdev->send_buff, sizeof(_g_pdev->send_buff));

        memcpy(_g_pdev->send_buff, _g_pdev->send_p, sizeof(JLPacketHead_t) + pt->optlen);
        offset  = sizeof(JLPacketHead_t) + pt->optlen;

        memcpy(_g_pdev->send_buff + offset, 
                _g_pdev->send_p + offset + JL_MAX_CUT_PACKET_LEN * (i -1), len);

        pt = (JLPacketHead_t*)_g_pdev->send_buff; 
        pt->total = total;
        pt->index = i;
        pt->payloadlen = len;
        pt->crc = CRC16(_g_pdev->send_buff + sizeof(JLPacketHead_t), pt->optlen + pt->payloadlen);
        pt->reserved = (unsigned char)res;

        ret = sendto(_g_pdev->lan_socket, 
                _g_pdev->send_buff,
                len + sizeof(JLPacketHead_t) + pt->optlen, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error");
        }
        log_info("send to:%s:ret:%d", _g_pdev->jlp.ip, ret);
    }

    if(NULL != _g_pdev->send_p){
        free(_g_pdev->send_p);
        _g_pdev->send_p = NULL;
    }
}

static void
joylink_proc_lan_scan(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len = 0;
    DevScan_t scan;
    bzero(&scan, sizeof(scan));

    if(E_RET_OK != joylink_parse_scan(&scan, (const char *)src)){
    }else{
        len = joylink_packet_lan_scan_rsp(&scan);
    }

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        if(len < JL_MAX_PACKET_LEN){
            ret = sendto(_g_pdev->lan_socket, 
                    _g_pdev->send_buff,
                    len, 0,
                    (SOCKADDR*)sin_recv, addrlen);
            if(ret < 0){
                perror("send error");
            }
        }else{
            joylink_send_big_pkg(sin_recv, addrlen);
            log_info("SEND PKG IS TOO BIG:ip:%sret:%d", _g_pdev->jlp.ip, ret);
        }
        log_info("send to:%s:ret:%d", _g_pdev->jlp.ip, ret);
    }else{
        log_error("packet error ret:%d", ret);
    }
}

static void
joylink_proc_lan_write_key(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    DevEnable_t de;
    bzero(&de, sizeof(de));

    joylink_parse_lan_write_key(&de, (const char*)src + 4);

    log_info("-->feedid:%s:accesskey:%s\n", de.feedid, de.accesskey);
    log_info("-->localkey:%s\n", de.localkey);
    log_info("-->joylink_server:%s\n", de.joylink_server);

    strcpy(_g_pdev->jlp.feedid, de.feedid);
    strcpy(_g_pdev->jlp.accesskey, de.accesskey);
    strcpy(_g_pdev->jlp.localkey, de.localkey);

    joylink_util_cut_ip_port(de.joylink_server,
            _g_pdev->jlp.joylink_server,
            &_g_pdev->jlp.server_port);

    joylink_dev_set_attr_jlp(&_g_pdev->jlp);

    int len = joylink_packet_lan_write_key_rsp();
    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, 
                len, 0, (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error ret:%d", ret);
            perror("send error");
        }
    }else{
        log_error("packet error ret:%d", ret);
    }
}

static void
joylink_proc_lan_json_ctrl(uint8_t *json_cmd, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len = 0;
    char data[JL_MAX_PACKET_LEN] = {0};
    char feedid[JL_MAX_FEEDID_LEN] = {0};
    time_t tt = time(NULL);

    joylink_parse_json_ctrl(feedid, (char*)json_cmd);
    ret = joylink_dev_lan_json_ctrl((char *)json_cmd);
    memcpy(data, &tt, 4);
    ret = joylink_dev_get_json_snap_shot(data + 4, sizeof(data) - 4, ret, feedid);

    log_info("rsp data:%s:len:%d", data + 4 , ret);

    len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, ret + 4);

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error(" 1 send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }

    joylink_server_upload_req();
} 

static void
joylink_proc_lan_script_ctrl(uint8_t *src, struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = -1;
    int len = 0;
    char data[JL_MAX_PACKET_LEN] = {0};
    JLContrl_t ctr;
    bzero(&ctr, sizeof(ctr));

    if(-1 == joylink_dev_script_ctrl((const char *)src, &ctr, 0)){
        return;
    }
    ret = joylink_packet_script_ctrl_rsp(data, sizeof(data), &ctr);
    log_info("rsp data:%s:len:%d", data + 12, ret);
    len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, ret);

    if(len > 0 && len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error(" 1 send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }
    joylink_server_upload_req();
}

E_JLRetCode_t 
joylink_proc_lan_rsp_send(uint8_t* data, int len,  struct sockaddr_in *sin_recv, socklen_t addrlen)
{
    int ret = E_RET_ERROR;
    int en_len = 0;
    en_len = joylink_encrypt_lan_basic(_g_pdev->send_buff, JL_MAX_PACKET_LEN,
            ET_ACCESSKEYAES, PT_SCRIPTCONTROL,
            (uint8_t*)_g_pdev->jlp.localkey, (const uint8_t*)data, len);

    if(en_len > 0 && en_len < JL_MAX_PACKET_LEN){ 
        ret = sendto(_g_pdev->lan_socket, _g_pdev->send_buff, en_len, 0,
                (SOCKADDR*)sin_recv, addrlen);

        if(ret < 0){
            log_error("send error ret:%d", ret);
        }else{
            log_info("rsp to:%s:len:%d", _g_pdev->jlp.ip, ret);
        }
    }else{
        log_error("packet error ret:%d", ret);
    }

    joylink_server_upload_req();

    return ret;
}

void
joylink_proc_lan()
{
    int ret;
    uint8_t recBuffer[JL_MAX_PACKET_LEN] = { 0 };
    /*uint8_t recPainText[JL_MAX_PACKET_LEN] = { 0 };*/
    uint8_t recPainBuffer[JL_MAX_PACKET_LEN] = { 0 };
    uint8_t *recPainText = NULL;
    JLPacketHead_t* pHead = NULL;

    struct sockaddr_in sin_recv;
    JLPacketParam_t param;

    bzero(&sin_recv, sizeof(sin_recv));
    bzero(&param, sizeof(param));
    socklen_t addrlen = sizeof(SOCKADDR);

    ret = recvfrom(_g_pdev->lan_socket, recBuffer, 
            sizeof(recBuffer), 0, (SOCKADDR*)&sin_recv, &addrlen);

    if(ret == -1){
        goto RET;
    }

    joylink_util_get_ipstr(&sin_recv, _g_pdev->jlp.ip);

    ret = joylink_dencypt_lan_req(&param, recBuffer, ret, recPainBuffer, JL_MAX_PACKET_LEN);
    if (ret <= 0){
        goto RET;
    }

    pHead = (JLPacketHead_t*)recBuffer;
    if(pHead->total > 1){
        log_info("PACKET WAIT TO JOIN!!!!!!:IP:%s", _g_pdev->jlp.ip);
        if(E_RET_ALL_DATA_HIT ==  joylink_join_pkg_add_data(_g_pdev->jlp.ip,
                    pHead->reserved, pHead, (char*)recPainBuffer + 4, ret - 4)){

            recPainText = (uint8_t*)joylink_join_pkg_join_data(
                    _g_pdev->jlp.ip,
                    pHead->reserved,
                    &ret);
        }else{
            log_info("PACKET WAIT TO JOIN!!!!!!:IP:%s", _g_pdev->jlp.ip);
        }
    }else{
        recPainText = recPainBuffer;
    }

    if(NULL == recPainText){
        goto RET;
    }
    switch (param.type){
        case PT_SCAN:
            if(param.version == 1){
                log_info("PT_SCAN:%s (Scan->Type:%d, Version:%d)", 
                        _g_pdev->jlp.ip, param.type, param.version);
                joylink_proc_lan_scan(recPainText, &sin_recv, addrlen);

            }
            break;
        case PT_WRITE_ACCESSKEY:
            if(param.version == 1){
                log_info("PT_WRITE_ACCESSKEY");
                log_info("-->write key org:%s", recPainText + 4);
                joylink_proc_lan_write_key(recPainText, &sin_recv, addrlen);
            }
            break;
        case PT_JSONCONTROL:
            if(param.version == 1){
                log_info("JSon Control->%s\n", recPainText + 4);
                joylink_proc_lan_json_ctrl(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SCRIPTCONTROL:
            if(param.version == 1){
                log_debug("SCRIPT Control->%s", recPainText + 12);
                joylink_proc_lan_script_ctrl(recPainText, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_AUTH:
            if(param.version == 1){
                log_info("PT_SUB_AUTH");
                log_debug("Control->%s", recPainText + 4);
                joylink_proc_lan_sub_auth(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_LAN_JSON:
            if(param.version == 1){
                log_debug("Control->%s", recPainText + 4);
                joylink_proc_lan_json_ctrl(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        case PT_SUB_LAN_SCRIPT:
            if(param.version == 1){
                log_info("PT_SUB_LAN_SCRIPT");
                log_debug("localkey->%s", _g_pdev->jlp.localkey);
                log_debug("Control->%s", recPainText + 12 + 32);
                joylink_proc_lan_sub_script_ctrl(recPainText, ret,
                            &sin_recv, addrlen);
            }
            break;
        case PT_SUB_ADD:
            if(param.version == 1){
                log_info("PT_SUB_ADD");
                log_debug("Sub add->%s", recPainText + 4);
                joylink_proc_lan_sub_add(recPainText + 4, &sin_recv, addrlen);
            }
            break;
        default:
            break;
    }

RET:
    /*if(NULL != pHead */
            /*&& pHead->total > 1*/
            /*&& NULL != recPainText){*/
            /*free(recPainText);*/
    /*}*/

    return;
}
