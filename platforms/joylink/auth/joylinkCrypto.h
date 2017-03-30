//
//  joylinkCrypto.h
//  JoyLink
//
//  Created by broadlink on 15/6/16.
//  Copyright (c) 2015年 broadlink. All rights reserved.
//

#ifndef JoyLink_joylinkCrypto_h
#define JoyLink_joylinkCrypto_h

#include "joylinkSetting.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ARC4 crypt, 若有用户自己实现, 则宏定义为1
#define HARDWARE_ARC4   0
extern int joylinkEnc1Crypt(UINT8 *key, UINT32 keyLen, UINT8 *data, UINT32 len);

#define JOYLINK_ENC2_ENCRYPT     1
#define JOYLINK_ENC2_DECRYPT     0

//#define JOYLINK_ERR_ENC2_INVALID_KEY_LENGTH                -0x0020  /**< Invalid key length. */
//#define JOYLINK_ERR_ENC2_INVALID_INPUT_LENGTH              -0x0022  /**< Invalid data input length. */

//AES crypt, 若用户自己实现，则宏定义为1
#define HARDWARE_AES    0
extern int joylinkEnc2Crypt(UINT8 *key, UINT32 keyLen, UINT8 *iv, UINT8 *data, UINT32 *len, UINT32 maxLen, int isPKCS5, int type);

//MD5 crypt, 若用户自己实现，则宏定义为1
#define HARDWARE_MD5    1
extern void joylinkENC3(UINT8 *input, UINT32 inputlen, UINT8 output[16]);


#define JOYLINK_ENC4_KEY_BIT     1024
#define JOYLINK_ENC4_ENCRYPT     1
#define JOYLINK_ENC4_DECRYPT     0
//RSA crypt, 需要由用户自己实现,
#define HARDWARE_RSA    1
extern int joylinkEnc4Crypt(UINT8 *input, UINT32 inputLen, UINT8 *output, UINT32 maxLen, UINT8 *key, int type);

#ifdef __cplusplus
}
#endif

#endif
