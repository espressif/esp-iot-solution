#ifndef __AES_H
#define __AES_H

#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define PLATEFRAME_LEXIN	
#ifdef PLATEFRAME_LEXIN
extern int device_aes_encrypt(const uint8_t * key, int keyLength, 
        const uint8_t * iv, const uint8_t *pPlainIn, int plainLength, 
        uint8_t *pEncOut, int maxOutLen);

extern int device_aes_decrypt(const uint8_t * key, int keyLength, const uint8_t * iv, const uint8_t *pEncIn, int encLength, uint8_t *pPlainOut, int maxOutLen);

#else
#define LITTLE_ENDIAN_ORDER

enum {
    AES_ENC_TYPE   = 1,   /* cipher unique type */
    AES_ENCRYPTION = 0,
    AES_DECRYPTION = 1,
    AES_BLOCK_SIZE = 16
};

enum{
	CYASSL_WORD_SIZE  = sizeof(uint32_t)
};


#define ALIGN16

#ifndef NULL
#define NULL 0
#endif

#define JOYLINK_ENC2_ENCRYPT     1
#define JOYLINK_ENC2_DECRYPT     0

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

typedef struct Aes {
    /* AESNI needs key first, rounds 2nd, not sure why yet */
    ALIGN16 uint32_t key[60];
    uint32_t  rounds;

    ALIGN16 uint32_t reg[AES_BLOCK_SIZE / sizeof(uint32_t)];      /* for CBC mode */
    ALIGN16 uint32_t tmp[AES_BLOCK_SIZE / sizeof(uint32_t)];      /* same         */
} Aes;

enum {
    MAX_ERROR_SZ       =  80,   /* max size of error string */
    MAX_CODE_E         = -100,  /* errors -101 - -199 */
    OPEN_RAN_E         = -101,  /* opening random device error */
    READ_RAN_E         = -102,  /* reading random device error */
    WINCRYPT_E         = -103,  /* windows crypt init error */
    CRYPTGEN_E         = -104,  /* windows crypt generation error */
    RAN_BLOCK_E        = -105,  /* reading random device would block */

    MP_INIT_E          = -110,  /* mp_init error state */
    MP_READ_E          = -111,  /* mp_read error state */
    MP_EXPTMOD_E       = -112,  /* mp_exptmod error state */
    MP_TO_E            = -113,  /* mp_to_xxx error state, can't convert */
    MP_SUB_E           = -114,  /* mp_sub error state, can't subtract */
    MP_ADD_E           = -115,  /* mp_add error state, can't add */
    MP_MUL_E           = -116,  /* mp_mul error state, can't multiply */
    MP_MULMOD_E        = -117,  /* mp_mulmod error state, can't multiply mod */
    MP_MOD_E           = -118,  /* mp_mod error state, can't mod */
    MP_INVMOD_E        = -119,  /* mp_invmod error state, can't inv mod */
    MP_CMP_E           = -120,  /* mp_cmp error state */
    MP_ZERO_E          = -121,  /* got a mp zero result, not expected */

    MEMORY_E           = -125,  /* out of memory error */

    RSA_WRONG_TYPE_E   = -130,  /* RSA wrong block type for RSA function */
    RSA_BUFFER_E       = -131,  /* RSA buffer error, output too small or 
                                   input too large */
    BUFFER_E           = -132,  /* output buffer too small or input too large */
    ALGO_ID_E          = -133,  /* setting algo id error */
    PUBLIC_KEY_E       = -134,  /* setting public key error */
    DATE_E             = -135,  /* setting date validity error */
    SUBJECT_E          = -136,  /* setting subject name error */
    ISSUER_E           = -137,  /* setting issuer  name error */
    CA_TRUE_E          = -138,  /* setting CA basic constraint true error */
    EXTENSIONS_E       = -139,  /* setting extensions error */

    ASN_PARSE_E        = -140,  /* ASN parsing error, invalid input */
    ASN_VERSION_E      = -141,  /* ASN version error, invalid number */
    ASN_GETINT_E       = -142,  /* ASN get big int error, invalid data */
    ASN_RSA_KEY_E      = -143,  /* ASN key init error, invalid input */
    ASN_OBJECT_ID_E    = -144,  /* ASN object id error, invalid id */
    ASN_TAG_NULL_E     = -145,  /* ASN tag error, not null */
    ASN_EXPECT_0_E     = -146,  /* ASN expect error, not zero */
    ASN_BITSTR_E       = -147,  /* ASN bit string error, wrong id */
    ASN_UNKNOWN_OID_E  = -148,  /* ASN oid error, unknown sum id */
    ASN_DATE_SZ_E      = -149,  /* ASN date error, bad size */
    ASN_BEFORE_DATE_E  = -150,  /* ASN date error, current date before */
    ASN_AFTER_DATE_E   = -151,  /* ASN date error, current date after */
    ASN_SIG_OID_E      = -152,  /* ASN signature error, mismatched oid */
    ASN_TIME_E         = -153,  /* ASN time error, unknown time type */
    ASN_INPUT_E        = -154,  /* ASN input error, not enough data */
    ASN_SIG_CONFIRM_E  = -155,  /* ASN sig error, confirm failure */
    ASN_SIG_HASH_E     = -156,  /* ASN sig error, unsupported hash type */
    ASN_SIG_KEY_E      = -157,  /* ASN sig error, unsupported key type */
    ASN_DH_KEY_E       = -158,  /* ASN key init error, invalid input */
    ASN_NTRU_KEY_E     = -159,  /* ASN ntru key decode error, invalid input */

    ECC_BAD_ARG_E      = -170,  /* ECC input argument of wrong type */
    ASN_ECC_KEY_E      = -171,  /* ASN ECC bad input */
    ECC_CURVE_OID_E    = -172,  /* Unsupported ECC OID curve type */
    BAD_FUNC_ARG       = -173,  /* Bad function argument provided */
    NOT_COMPILED_IN    = -174,  /* Feature not compiled in */
    UNICODE_SIZE_E     = -175,  /* Unicode password too big */
    NO_PASSWORD        = -176,  /* no password provided by user */
    ALT_NAME_E         = -177,  /* alt name size problem, too big */

    AES_GCM_AUTH_E     = -180,  /* AES-GCM Authentication check failure */
    AES_CCM_AUTH_E     = -181,  /* AES-CCM Authentication check failure */

    CAVIUM_INIT_E      = -182,  /* Cavium Init type error */

    COMPRESS_INIT_E    = -183,  /* Compress init error */
    COMPRESS_E         = -184,  /* Compress error */
    DECOMPRESS_INIT_E  = -185,  /* DeCompress init error */
    DECOMPRESS_E       = -186,  /* DeCompress error */

    BAD_ALIGN_E        = -187,  /* Bad alignment for operation, no alloc */

    MIN_CODE_E         = -200   /* errors -101 - -199 */
};


int device_aes_encrypt(const uint8_t * key, int keyLength, 
        const uint8_t * iv, const uint8_t *pPlainIn, int plainLength, 
        uint8_t *pEncOut, int maxOutLen);

int device_aes_decrypt(const uint8_t * key, int keyLength, const uint8_t * iv, const uint8_t *pEncIn, int encLength, uint8_t *pPlainOut, int maxOutLen);
#endif
#endif


