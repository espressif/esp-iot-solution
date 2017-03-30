#ifndef _JOIN_PKG_H
#define _JOIN_PKG_H

#ifdef __LINUX_UB2__
#include <stdint.h>
#endif

#include "joylink.h"

int
joylink_join_pkg_add_data(char *name, int id, JLPacketHead_t *phead, char *data, int len);

char *
joylink_join_pkg_join_data(char *name, int id, int *out_len);

int
joylink_join_pkg_clean_node_by_name_id(char *name, int id);
#endif
