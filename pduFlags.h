#ifndef __PDU_FLAGS__
#define __PDU_FLAGS__

#define PDU_CONNECT     0x01
#define PDU_ACCEPT      0x02
#define PDU_REJECT      0x03
#define PDU_BROADCAST   0x04
#define PDU_UNICAST     0x05
#define PDU_MULTICAST   0x06
#define PDU_ERROR       0x07
#define PDU_HANDLES     0x10
#define PDU_H_LEN       0x11
#define PDU_H_RESP      0x12
#define PDU_H_TERM      0x13

// Defining the positions of parts of the packet
#define FLAG_LOC        0x0
// The length of a packet telling a client how many handles there are. Should be the flag plus 4 bytes
#define PDU_H_PACKET_LEN 5

#endif