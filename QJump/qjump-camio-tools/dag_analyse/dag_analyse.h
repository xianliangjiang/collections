// CamIO 2: dag_analyse.h
// Copyright (C) 2015: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef DAG_ANALYSE2_H_
#define DAG_ANALYSE2_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <byteswap.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

#include <openssl/md5.h>

#include "camio/selectors/camio_selector.h"
#include "camio/camio_util.h"
#include "camio/camio_errors.h"
#include "camio/istreams/camio_istream.h"
#include "camio/ostreams/camio_ostream.h"
#include "camio/ostreams/camio_ostream_raw.h"
#include "camio/dag/dagapi.h"
#include "camio/options/camio_options.h"


typedef struct {
    uint64_t timestamp;     //Timestamp in nanos
    union {
        struct{
            uint32_t length : 16;       //Length of the packet (up to 65kB (as per dag card wlen))
            uint32_t dropped: 8;        //Number of dropped packets behind this one
            uint32_t type   : 8;        //Type of the packet

        } value_type_dropped_len;
        uint32_t raw_type_dropped_len;  //Raw 32bit value so I can do the & myself.
    };

    uint64_t hash;                      //64bit unique identifier for a given packet

} __attribute__((__packed__)) sample_t;


typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    uint64_t samples;
    sample_t first_sample;
} __attribute__((__packed__)) dag_cap_head_t;



enum {
    reserved        = 0x00UL,
    tcp             = 0x01UL,
    udp             = 0x02UL,
    icmp            = 0x03UL,
    arp             = 0x04UL,
    ip_with_opts    = 0x05UL,

    ip_unknown      = 0xDDUL,
    eth_unknown     = 0xEEUL,
    unknown         = 0xFFUL,
};

typedef struct {
    uint64_t cap_dag0[10];
    uint64_t matched; //Was a match found?
    uint64_t d0_idx;  //Index of this packet in d0
    uint64_t d1_idx;  //Index of the matched packet in d1 (if matched), otherwise 0xFFF...
    int64_t latency;  //Latency if a match was found
} lat_match_rec_t ;

typedef struct {
    uint64_t pkt_lost_count;
    uint64_t pkt_timestamp_ns;
    uint64_t pkt_type;
    uint64_t pkt_wlen;
    uint64_t pkt_hash;
    uint64_t eth_mac_src;
    uint64_t eth_mac_dst;
    uint64_t eth_has_vlan;
    uint64_t eth_vlan_id;           //Only valid if has_vlan = true
    uint64_t eth_vlan_priority;     //Only valid if has_vlan = true
    uint64_t ip_len;                //Only valid if type == ip,tcp, udp, or icmp
    uint64_t ip_src;                //Only valid if type == ip,tcp, udp, or icmp
    uint64_t ip_dst;                //Only valid if type == ip,tcp, udp, or icmp
    uint64_t port_src;              //Only valid if type == tcp/udp
    uint64_t port_dst;              //Only valid if type == tcp/udp
    uint64_t tcp_seq;               //Only valid if type == tcp
    uint64_t tcp_ack;               //Only valid if type == tcp
    uint64_t tcp_win;               //Only valid if type == tcp
    uint64_t udp_payload8;         //First 8bytes of UDP payload, only valid if type == udp

    //Only for packets with size == 13 * 8B
    uint64_t pkt_has_latency; //Do we have a latency measured packet?
    uint64_t pkt_matched; //Was a match found?
    uint64_t pkt_d0_idx;  //Index of this packet in d0
    uint64_t pkt_d1_idx;  //Index of the matched packet in d1 (if matched), otherwise 0xFFF...
    int64_t pkt_latency;  //Latency if a match was found

} packet_info_t;




#endif /* DAG_ANALYSE2_H_ */
