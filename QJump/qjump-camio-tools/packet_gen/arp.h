/*
 * arp.h
 *
 *  Created on: Jan 21, 2013
 *      Author: mgrosvenor
 */

#ifndef ARP_H_
#define ARP_H_

#include <stdint.h>

typedef struct{
		uint8_t  dst_mac[6];
		uint8_t  src_mac[6];
		uint16_t eth_type;
		uint16_t htype;
		uint16_t ptype;
		uint8_t  plen;
		uint8_t  hlen;
		uint16_t oper;
		uint8_t  sha[6];
		uint32_t spa;
		uint8_t  tha[6];
		uint32_t tpa;
} __attribute__((__packed__)) arp_packet_t ;

#endif /* ARP_H_ */
