
#ifndef __HV_PLATFORM_H__
#define __HV_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform.h"
#include "esp_alink.h"

/** @defgroup group_product product
 *  @{
 */

#define PRODUCT_SN_LEN          (64 + 1)
#define PRODUCT_MODEL_LEN       (80 + 1)
#define PRODUCT_KEY_LEN         (20 + 1)
#define PRODUCT_SECRET_LEN      (40 + 1)
#define PRODUCT_UUID_LEN        (32 + 1)
#define PRODUCT_VERSION_LEN     (16 + 1)
#define PRODUCT_NAME_LEN        (32 + 1)
#define PRODUCT_ASR_APP_KEY_LEN (64)

#define PRODUCT_CID_LEN         (64 + 1)
typedef struct alink_product {
    /* optional */
    char name[PRODUCT_NAME_LEN];
    char model[PRODUCT_MODEL_LEN];
    char version[PRODUCT_NAME_LEN];
    char type[PRODUCT_NAME_LEN];
    char category[PRODUCT_NAME_LEN];
    char manufacturer[PRODUCT_NAME_LEN];
    char cid[PRODUCT_CID_LEN];
    char sn[PRODUCT_SN_LEN];
    char key[PRODUCT_KEY_LEN];
    char secret[PRODUCT_SECRET_LEN];
    char key_sandbox[PRODUCT_KEY_LEN];
    char secret_sandbox[PRODUCT_SECRET_LEN];
} alink_product_t;

/**
 * @brief Get the product version string.
 *
 * @param[in] version_str @n Buffer for using to store version string.
 * @return The version string.
 * @see None.
 * @note
 */
alink_err_t product_get(_OUT_ void *product_info);

/**
 * @brief Get the product version string.
 *
 * @param[in] version_str @n Buffer for using to store version string.
 * @return The version string.
 * @see None.
 * @note
 */
alink_err_t product_set(_IN_ const void *product_info);



/**
 * @brief Get the product version string.
 *
 * @param[in] version_str @n Buffer for using to store version string.
 * @return The version string.
 * @see None.
 * @note
 */
char *product_get_version(char version_str[PRODUCT_VERSION_LEN]);

/**
 * @brief Get product name string.
 *
 * @param[out] name_str @n Buffer for using to store name string.
 * @return A pointer to the start address of name_str.
 * @see None.
 * @note None.
 */
char *product_get_name(char name_str[PRODUCT_NAME_LEN]);

/**
 * @brief Get product SN string.
 *
 * @param[out] sn_str @n Buffer for using to store SN string.
 * @return A pointer to the start address of sn_str.
 * @see None.
 * @note None.
 */
char *product_get_sn(char sn_str[PRODUCT_SN_LEN]);

/**
 * @brief Get product model string.
 *
 * @param[out] model_str @n Buffer for using to store model string.
 * @return A pointer to the start address of model_str.
 * @see None.
 * @note None.
 */
char *product_get_model(char model_str[PRODUCT_MODEL_LEN]);

/**
 * @brief Get product key string.
 *
 * @param[out] key_str @n Buffer for using to store key string.
 * @return A pointer to the start address of key_str.
 * @see None.
 * @note None.
 */
char *product_get_key(char key_str[PRODUCT_KEY_LEN]);

/**
 * @brief Get product secret string.
 *
 * @param[out] secret_str @n Buffer for using to store secret string.
 * @return A pointer to the start address of secret_str.
 * @see None.
 * @note None.
 */
char *product_get_secret(char secret_str[PRODUCT_SECRET_LEN]);

/**
 * @brief Get product debug key string.
 *
 * @param[out] key_str @n Buffer for using to store debug key string.
 * @return A pointer to the start address of key_str.
 * @see None.
 * @note None.
 */
char *product_get_debug_key(char key_str[PRODUCT_KEY_LEN]);

/**
 * @brief Get product debug secret string.
 *
 * @param[out] secret_str @n Buffer for using to store debug secret string.
 * @return A pointer to the start address of secret_str.
 * @see None.
 * @note None.
 */
char *product_get_debug_secret(char secret_str[PRODUCT_SECRET_LEN]);


char *product_get_asr_appkey(char app_key[PRODUCT_ASR_APP_KEY_LEN]);


char *product_get_audio_format();


/** @} */// end of group_product

#ifdef __cplusplus
}
#endif
#endif
