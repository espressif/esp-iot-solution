/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   This function implements the API defined in <linux/decompress/generic.h>.
 *
 * This wrapper will automatically choose single-call or multi-call mode
 * of the native XZ decoder API. The single-call mode can be used only when
 * both input and output buffers are available as a single chunk, i.e. when
 * fill() and flush() won't be used.
 *
 * @param[in]  in pointer to the input buffer to be decompressed
 * @param[in]  in_size size of the input buffer
 * @param[in]  fill pointer to the function used to read compressed data
 * @param[in]  flush pointer to the function used to write decompressed data
 * @param[out] out pointer to the out buffer to store the decompresssed data
 * @param[out] in_used length of the data has been used to decompressed
 * @param[in]  error pointer to the function to output the error message
 * @return
 *         - 0 on success
 *         - -1 on error
 *
 */
int xz_decompress(unsigned char *in, int in_size,
	int (*fill)(void *dest, unsigned int size),
	int (*flush)(void *src, unsigned int size),
	unsigned char *out, int *in_used,
	void (*error)(const char *x));

#ifdef __cplusplus
}
#endif
