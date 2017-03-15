//
//  joylinkErrno.h
//  JoyLink
//
//  Created by broadlink on 15/6/14.
//  Copyright (c) 2015年 broadlink. All rights reserved.
//

#ifndef JoyLink_joylinkErrno_h
#define JoyLink_joylinkErrno_h

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
    JOYLINK_SUCCESS                 = 0,
    JOYLINK_BUFFER_SPACE_ERR        = -100,             /*buffer不足*/
    JOYLINK_RECV_LEN_ERR            = -101,             /*接收数据长度有误*/
    JOYLINK_CHECKSUM_ERR            = -102,             /*数据校验失败*/
    JOYLINK_GET_CMD_ERR             = -103,             /*接收命令类型有误*/
    JOYLINK_GET_DEVID_ERR           = -104,             /*设备ID不一致*/
    JOYLINK_DEVID_ERR               = -105,             /*设备ID有误*/
    JOYLINK_TOKEN_ERR               = -106,             /*设备TOKEN有误*/
    JOYLINK_ENCTYPE_ERR             = -107,             /*不支持的加密策略*/
    JOYLINK_MAGIC_ERR               = -108,             /*无效数据包,magic有误*/
    JOYLINK_ENCINFO_ERR             = -109,             /*加密信息有误*/
    
    
    JOYLINK_PARAM_INVALID           = -1000,            /*参数数据有误*/
    JOYLINK_SYSTEM_ERR              = -1001,            /*系统调用错误,例如创建socket失败*/
    JOYLINK_NETWORK_TIMEOUT         = -1002,            /*网络超时*/
    JOYLINK_RECV_DATA_ERR           = -1003,             /*接收到的数据有误*/
    JOYLINK_CANCEL_ERR              = -1004,            /*用户取消操作*/
};

#ifdef __cplusplus
}
#endif
    
#endif
