#ifndef _NODECACHE_H
#define _NODECACHE_H

#include "joylink.h"

typedef struct {
	int isInited;
	uint8_t priKey[uECC_BYTES];
	uint8_t devPubKey[uECC_BYTES * 2];
	uint8_t devPubKeyC[uECC_BYTES + 1];
}JLEccContex_t;

typedef struct{
	uint8_t pubkeyC[JL_MAX_KEY_BIN_LEN];		// ѹ????ʽ?Ĺ?Կ
	uint8_t sharedkey[uECC_BYTES];		// ?豸?Ĺ?????Կ
}JLKey_t;

extern JLEccContex_t __g_ekey;

#endif
