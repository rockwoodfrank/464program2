#include "getPDUdata.h"
#include <stdio.h>

// Return the address of the flag in the packet.
u_int8_t *get_flag(u_int8_t *packet)
{
    return &(packet[0]);
}

// Return the address of the length of the sender handle in the packet.
u_int8_t *get_src_len(u_int8_t *packet)
{
    return &(packet[1]);
}

// Return the address of the sender handle in the packet.
u_int8_t *get_src(u_int8_t *packet)
{
    return &(packet[2]);
}

// Return the address of the number of destinations in the packet
u_int8_t *get_num_dests(u_int8_t *packet)
{
    // Find the length of the header
    u_int8_t src_len = *(get_src_len(packet));
    return &(packet[2 + src_len]);
}

// Returns the length of the name of the nth dest handle
// Returns a NULL if n is out of range -indexed at 0
u_int8_t *get_nth_dest_len(u_int8_t *packet, int n)
{
    // Get the number of handles
    u_int8_t num_dests = *(get_num_dests(packet));
    if (n >= num_dests) return NULL;
    int i = 0;
    u_int8_t *iter_pos = &(get_num_dests(packet)[1]);
    while (i < n)
    {
        iter_pos = &(iter_pos[*iter_pos + 1]);
        i++;
    }
    return iter_pos;
}

// Returns the address of the nth dest handle
u_int8_t *get_nth_dest(u_int8_t *packet, int n)
{
    u_int8_t *nth_len = get_nth_dest_len(packet, n);
    if (nth_len == NULL) return NULL;
    return &(nth_len[1]);
}

// Returns the address of the message in the packet
u_int8_t *get_msg(u_int8_t *packet)
{
    if (*(get_flag(packet)) == PDU_BROADCAST || *(get_flag(packet)) == PDU_HANDLES)
    {
        u_int8_t h_len = *(get_src_len(packet));
        return &(get_src(packet)[h_len]);
    }
    else
    {
        u_int8_t max_packet = *(get_num_dests(packet));
        
        u_int8_t *last = get_nth_dest_len(packet, max_packet-1);
        u_int8_t last_h_len = *(last);
        return &(get_nth_dest(packet, max_packet-1)[last_h_len]);
    }
}