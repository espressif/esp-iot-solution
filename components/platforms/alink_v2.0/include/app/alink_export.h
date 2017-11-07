/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */

/* header file for alink wifi device */
#ifndef _ALINK_EXPORT_H_
#define _ALINK_EXPORT_H_

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/** @defgroup wifi_device wifi device API
 *  @{
 */


/** @defgroup wifi_debug debug
 *  @{
 */

/**
 * log level def.
 */
enum ALINK_LOG_LEVEL {
    ALINK_LL_NONE, /**< disable log */
    ALINK_LL_FATAL, /**< fatal error log will output */
    ALINK_LL_ERROR, /**< error + fatal log will output */
    ALINK_LL_WARN, /**< warn + error + fatal log will output(default level) */
    ALINK_LL_INFO, /**< info + warn + error + fatal log will output */
    ALINK_LL_DEBUG, /**< debug + info + warn + error + fatal log will output */
    ALINK_LL_TRACE, /**< trace + debug + info + warn + error + fatal will output */
};

/**
 * @brief log level control
 *
 * @param[in] loglevel
 * @return None.
 * @see enum ALINK_LOG_LEVEL.
 * @note None.
 */
void alink_set_loglevel(enum ALINK_LOG_LEVEL loglevel);

/**
 * @brief enable sandbox mode, for debug
 *
 * @param None.
 * @retval 0 on success, otherwise -1 will return
 * @see None.
 * @note None.
 */
int alink_enable_sandbox_mode(void);

/**
 * @brief enable daily mode, for debug
 *
 * @param None.
 * @retval 0 on success, otherwise -1 will return
 * @see None.
 * @note None.
 */
int alink_enable_daily_mode(const char *server_ip, int port);

/** @} */ //end of debug



/** @defgroup wifi_entry main
 *  @{
 */

/**
 * @brief entry function. start alink gateway service.
 *
 * @param None.
 * @retval 0 on success, otherwise -1 will return
 * @see None.
 * @note None.
 */
int alink_start(void);

#define ALINK_WAIT_FOREVER      (0)
/**
 * @brief waiting alink connect to aliyun server
 *
 * @param[in] timeout_ms: time in milliseconds,
 *              use ALINK_WAIT_FOREVER to wait until aliyun server is connected
 * @retval 0 when connect to server successfully, otherwise -1 will return
 * @see None.
 * @note None.
 */
int alink_wait_connect(int timeout_ms);

/**
 * @brief destroy alink service and free resources
 *
 * @param None.
 * @retval 0 on success, otherwise -1 will return
 * @see None.
 * @note this func will block at most 15 seconds to
 *      stop all alink related process(thread)
 */
int alink_end(void);

/**
 * @brief reset user account binding.
 *
 * @retval 0 on success, -1 when params invalid
 * @see None.
 * @note None.
 */
int alink_factory_reset(void);

/** @} */ //end of entry

/** @defgroup wifi_report report status
 *  @{
 */

/**
 * @brief report alink message, it is a fundamental func.
 *
 * @param[in] method: alink protocol method,
 *          i.e. "postDeviceRawData", "postDeviceData".
 * @param[in] json_buffer: json format buffer, like
 *              {"OnOff":"1", "Light":"80"} for ""postDeviceData",
 *              or {"rawData":"F5F5F5F50100041501","length":"20"} for "postDeviceRawData"
 * @retval 0 when successfully got response from cloud,
 *          otherwise this func will block until timeout and -1 will return
 * @see None.
 * @note when connection with server is unstable, this func will block
 *      until got response from server or timeout.
 */
int alink_report(const char *method, const char *json_buffer);

/**
 * @brief query alink cloud service, like getAlinkTime...
 *
 * @param[in] method: alink protocol method,
 *          i.e. "getAlinkTime", "retrieveDeviceData".
 * @param[in] json_buffer: json format buffer, like {} for "getAlinkTime",
 *              or {"dataKey":"param_probe_num"} for "retrieveDeviceData"
 * @param[in] result_buf: to hold json string return from cloud
 * @param[in/out] buf_len: [in] size of result_buf, [out] strlen of json string
 *              return from cloud
 * @retval 0 when successfully got response from cloud,
 *          otherwise this func will block until timeout and -1 will return
 * @see None.
 * @note when connection with server is unstable, this func will block
 *      until got response from server or timeout.
 */
int alink_query(const char *method, const char *json_buffer,
                char *result_buf, int *buf_len);

/** @} */ //end of report

/** @defgroup wifi_callback    callback
 *  @{
 */

/**
 * alink callback event
 */
enum ALINK_WIFI_CALLBACK {
    /**
     * void callback_cloud_connected(void)
     * @n@n called each time when gateway successfully connect(or reconnect)
     * to aliyun server
     */
    ALINK_CLOUD_CONNECTED = 0,

    /**
     * void callback_cloud_disconnected(void)
     * @n@n called each time when gateway lose connection with aliyun server
     */
    ALINK_CLOUD_DISCONNECTED,
    /**
     * int callback_read_device_status(const char *params)
     * @n@nuccessfully
     */
    ALINK_GET_DEVICE_STATUS,

    /**
     * void callback_write_device_status(const char *params)
     */
    ALINK_SET_DEVICE_STATUS,
    /**
     * int callback_read_device_rawdata(const char *params)
     * @n@nuccessfully
     */
    ALINK_GET_DEVICE_RAWDATA,

    /**
     * void callback_write_device_rawdata(const char *params)
     */
    ALINK_SET_DEVICE_RAWDATA,
};

/**
 * @brief register misc callback
 *
 * @param[in] cb_type: callback type.
 * @param[in] cb_func: callback func pointer, func declaration see related comments.
 *
 * @retval  0 on success, otherwise -1 will return
 * @see enum ALINK_WIFI_CALLBACK.
 * @note None.
 */
int alink_register_callback(unsigned char cb_type, void *cb_func);
/** @} */ //end of callback

/** @defgroup wifi_awss awss
 *  @{
 */

/**
 * @brief start awss service, block method,
 *      block until awss succeed, or timeout(see Note).
 *
 * @return 0 on success, otherwise non-zero value will return
   @verbatim
     = 0: connect AP & DHCP success
     = -1: get ssid & passwd fail
     = -2: connect AP / DHCP fail
   @endverbatim
 * @see None.
 * @note platform_awss_get_timeout_interval_ms() return monitor timeout interval,
 *      AP connection timeout is 30s.
 */
int awss_start(void);

/**
 * @brief calling this func force awss_start exit.
 *
 * @param None.
 * @retval  0 on success, otherwise -1 will return
 * @see None.
 * @note None.
 */
int awss_stop(void);

/** @} */ //end of awss

#ifdef __cplusplus
}
#endif


#endif
