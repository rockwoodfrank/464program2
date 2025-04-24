#include "getPDUdata.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    u_int8_t test_packet[100] = {PDU_MULTICAST, 6, 'c', 'l', 'i', 'e', 'n', 't', 3, 1, 'a', 2, 'b', 'c', 3, 'd', 'e', 'f', 'h', 'e', 'l', 'l', 'o'};

    assert(*(get_flag(test_packet)) == PDU_MULTICAST);
    assert(*(get_src_len(test_packet)) == 6);
    assert(*(get_src(test_packet)) == 'c');
    assert(*(get_num_dests(test_packet)) == 3);
    printf("%hhx\n", *(get_nth_dest_len(test_packet, 2)));
    printf("%c\n", get_msg(test_packet)[3]);
    return 0;
}