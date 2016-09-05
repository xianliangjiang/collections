/*
 * r2r2_control.h
 *
 *  Created on: Jan 21, 2013
 *      Author: mgrosvenor
 */

#ifndef R2R2_CONTROL_H_
#define R2R2_CONTROL_H_


struct r2d2_control {
    char dst_mac[6];
    char src_mac[6];
    uint16_t ether_type;
    uint64_t host_id;
    uint64_t seq;
    char payload[28];
} __attribute__ ((packed));

typedef struct r2d2_control r2d2_packet;


#endif /* R2R2_CONTROL_H_ */
