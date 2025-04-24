#ifndef __GETPDUDATA_H__
#define __GETPDUDATA_H__
#include <sys/types.h>
#include <stdlib.h>

#include "pduFlags.h"

// Functions that return points to the locations of key points in the packet
u_int8_t *get_flag(u_int8_t *packet);

u_int8_t *get_src_len(u_int8_t *packet);

u_int8_t *get_src(u_int8_t *packet);

u_int8_t *get_num_dests(u_int8_t *packet);

u_int8_t *get_nth_dest_len(u_int8_t *packet, int n);

u_int8_t *get_nth_dest(u_int8_t *packet, int n);

u_int8_t *get_msg(u_int8_t *packet);

#endif