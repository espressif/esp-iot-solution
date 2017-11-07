/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
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
#ifndef JSON_PARSER_H
#define JSON_PARSER_H
#ifdef __cplusplus
extern "C" {
#endif
/**
The descriptions of the json value node type
**/
enum JSONTYPE {
    JNONE = -1,
    JSTRING = 0,
    JOBJECT,
    JARRAY,
    JNUMBER,
    JTYPEMAX
};

/**
The error codes produced by the JSON parsers
**/
enum JSON_PARSE_CODE {
    JSON_PARSE_ERR,
    JSON_PARSE_OK,
    JSON_PARSE_FINISH
};

/**
The return codes produced by the JSON parsers
**/
enum JSON_PARSE_RESULT {
    JSON_RESULT_ERR = -1,
    JSON_RESULT_OK
};

#define JSON_DEBUG 0
typedef int (*json_parse_cb)(char* p_cName, int iNameLen, char* p_cValue, int iValueLen, int iValueType, void* p_Result);

/**
* @brief Parse the JSON string, and iterate through all keys and values,
* then handle the keys and values by callback function.
*
* @param[in]  p_cJsonStr @n  The JSON string
* @param[in]  iStrLen    @n  The JSON string length
* @param[in]  pfnCB      @n  Callback function
* @param[out] p_CBData   @n  User data
* @return JSON_RESULT_OK success, JSON_RESULT_ERR failed
* @see None.
* @note None.
**/
int json_parse_name_value(char* p_cJsonStr, int iStrLen, json_parse_cb pfnCB, void* p_CBData);

/**
* @brief Get the value by a specified key from a json string
*
* @param[in]  p_cJsonStr   @n the JSON string
* @param[in]  iStrLen      @n the JSON string length
* @param[in]  p_cName      @n the specified key string
* @param[out] p_iValueLen  @n the value length
* @param[out] p_iValueType @n the value type
* @return A pointer to the value
* @see None.
* @note None.
**/
char* json_get_value_by_name(char* p_cJsonStr, int iStrLen, char* p_cName, int* p_iValueLen, int* p_iValueType);

/**
* @brief Get the length of a json string
*
* @param[in]  json_str @n The JSON string
* @param[in]  str_len  @n The JSON string length
* @returns Array size
* @see None.
* @note None.
**/
int json_get_array_size(char *json_str, int str_len);

/**
 * @brief Get the JSON object point associate with a given type.
 *
 * @param[in] type @n The object type
 * @param[in] str  @n The JSON string
 * @returns The json object point with the given field type.
 * @see None.
 * @note None.
 */
char* json_get_object(int type, char* str);
char* json_get_next_object(int type, char* str, char** key, int* key_len, char** val, int* val_len, int* val_type);
/**
 * @brief retrieve each key&value pair from the json string
 *
 * @param[in]  str   @n Json string to revolve
 * @param[in]  pos   @n cursor
 * @param[out] key   @n pointer to the next Key object
 * @param[out] klen  @n Key object length
 * @param[out] val   @n pointer to the next Value object
 * @param[out] vlen  @n Value object length
 * @param[out] vtype @n Value object type(digital, string, object, array)
 * @see None.
 * @note None.
 */
#define json_object_for_each_kv(str, pos, key, klen, val, vlen, vtype) \
    for (pos = json_get_object(JOBJECT, str); \
         pos!=0 && *pos!=0 && (pos=json_get_next_object(JOBJECT, pos, &key, &klen, &val, &vlen, &vtype))!=0; )

/**
 * @brief retrieve each entry from the json array
 *
 * @param[in]  str   @n Json array to revolve
 * @param[in]  pos   @n cursor
 * @param[out] entry @n pointer to the next entry from the array
 * @param[out] len   @n entry length
 * @param[out] type  @n entry type(digital, string, object, array)
 * @see None.
 * @note None.
 */
#define json_array_for_each_entry(str, pos, entry, len, type) \
    for (pos = json_get_object(JARRAY, str); \
         pos!=0 && *pos!=0 && (pos=json_get_next_object(JARRAY, ++pos, 0, 0, &entry, &len, &type))!=0; )

#ifdef __cplusplus
}
#endif
#endif
