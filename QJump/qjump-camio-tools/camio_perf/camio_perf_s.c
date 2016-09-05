/*
 * Copyright  (C) Matthew P. Grosvenor, 2012, All Rights Reserved
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <byteswap.h>
#include <math.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "camio/selectors/camio_selector.h"
#include "camio/camio_util.h"
#include "camio/camio_errors.h"
#include "camio/istreams/camio_istream.h"
#include "camio/ostreams/camio_ostream.h"
#include "camio/ostreams/camio_ostream_raw.h"

uint64_t packets = 0;
camio_istream_t* in = NULL;
struct timeval t_start, t_end;
size_t total_len = 0;

/* Subtract the `struct timeval' values X and Y,
        storing the result in RESULT.
        Return 1 if the difference is negative, otherwise 0. */
//Copied from http://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
int timeval_subtract (struct timeval* result,struct timeval* x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
          tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}


void term(int signum){

    gettimeofday(&t_end,0);

    struct timeval tv;
    timeval_subtract(&tv,&t_end, &t_start);

    double seconds =  tv.tv_sec + (double)tv.tv_usec / 1000.0 / 1000.0;

    printf("camio_perf captured %lu packets in %lf seconds (bw=%fMbs)\n", packets, seconds, ((total_len * 8) / seconds) / 1000.0 / 1000.0 );


    if(in){ in->delete(in); }
    exit(0);
}



int main(int argc, char** argv){

    signal(SIGTERM, term);
    signal(SIGINT, term);

//    if(argc < 2){
//        printf("Please supply the destination ip/port\n");
//        exit(-1);
//    }


    //Input
    in = camio_istream_new("udp:10.10.1.1:2000", NULL);

    size_t len = 0;

    uint8_t* data;

    int first = 1;


    while( ( len = in->start_read(in, &data )) ){
        if(unlikely(first)){
            gettimeofday(&t_start,0);
            first = 0;
        }
        in->end_read(in, NULL);
        packets++;
        total_len += len;
    }



    in->delete(in);

    return 0;
}


























//    const eth_ip_head_t* eth_ip_head = (const eth_ip_head_t*)rx_lan_buff;
//    //printf("0x%04X\n",ntohs(eth_ip_head->unpack.eth_type));
//    switch(ntohs(eth_ip_head->unpack.eth_type)){
//        case 0x0806: printf(""); return -1;
//        case 0x0800: printf(""); break;
//        default:
//            //prin//    const eth_ip_head_t* eth_ip_head = (const eth_ip_head_t*)rx_lan_buff;
//    //printf("0x%04X\n",ntohs(eth_ip_head->unpack.eth_type));
//    switch(ntohs(eth_ip_head->unpack.eth_type)){
//        case 0x0806: printf(""); return -1;
//        case 0x0800: printf(""); break;
//        default:
//            //printf("Unknown 0x%04X\n",ntohs(eth_ip_head->unpack.eth_type) );
//            return -1;
//    }
////    printf("version:%x, ", eth_ip_head->unpack.ver );
////    printf("hdr len:%x, ", eth_ip_head->unpack.ihl );
////    printf("dscp:%x, ", eth_ip_head->unpack.dscp );
////    printf("ecn: %x", eth_ip_head->unpack.ecn );
////    printf("length:%x, ", ntohs(eth_ip_head->unpack.total_len) );
////    printf("id:%x, ", ntohs(eth_ip_head->unpack.id) );
////    printf("flags:%x, ", eth_ip_head->unpack.flags );
////    printf("frag off:%x, ", ntohs(eth_ip_head->unpack.frag_off ));
////    printf("ttl:%x, ", eth_ip_head->unpack.ttl);
////    printf("protocol:%x, ", eth_ip_head->unpack.protocol);
////    printf("hdr csum:%x, ", ntohs(eth_ip_head->unpack.hdr_csum));
//    printf("src_ip = 0x%08X (%d.%d.%d.%d) dst_ip= 0x%08x (%d.%d.%d.%d) ",
//            eth_ip_head->unpack.src_ip,
//            eth_ip_head->unpack.src_ip_raw[0],eth_ip_head->unpack.src_ip_raw[1],eth_ip_head->unpack.src_ip_raw[2],eth_ip_head->unpack.src_ip_raw[3],
//            eth_ip_head->unpack.dst_ip,
//            eth_ip_head->unpack.dst_ip_raw[0],eth_ip_head->unpack.dst_ip_raw[1],eth_ip_head->unpack.dst_ip_raw[2],eth_ip_head->unpack.dst_ip_raw[3]
//    );
//    printf("[0x%016lX][0x%016lX][0x%016lX]\n",(eth_ip_head->raw.value[1] & 0x0000FFFF00000000),(eth_ip_head->raw.value[3] & 0xFFFF000000000000), eth_ip_head->raw.value[4] & 0x000000000000FFFF);
//
//
//
//
//    return -1;
//tf("Unknown 0x%04X\n",ntohs(eth_ip_head->unpack.eth_type) );
//            return -1;
//    }
////    printf("version:%x, ", eth_ip_head->unpack.ver );
////    printf("hdr len:%x, ", eth_ip_head->unpack.ihl );
////    printf("dscp:%x, ", eth_ip_head->unpack.dscp );
////    printf("ecn: %x", eth_ip_head->unpack.ecn );
////    printf("length:%x, ", ntohs(eth_ip_head->unpack.total_len) );
////    printf("id:%x, ", ntohs(eth_ip_head->unpack.id) );
////    printf("flags:%x, ", eth_ip_head->unpack.flags );
////    printf("frag off:%x, ", ntohs(eth_ip_head->unpack.frag_off ));
////    printf("ttl:%x, ", eth_ip_head->unpack.ttl);
////    printf("protocol:%x, ", eth_ip_head->unpack.protocol);
////    printf("hdr csum:%x, ", ntohs(eth_ip_head->unpack.hdr_csum));
//    printf("src_ip = 0x%08X (%d.%d.%d.%d) dst_ip= 0x%08x (%d.%d.%d.%d) ",
//            eth_ip_head->unpack.src_ip,
//            eth_ip_head->unpack.src_ip_raw[0],eth_ip_head->unpack.src_ip_raw[1],eth_ip_head->unpack.src_ip_raw[2],eth_ip_head->unpack.src_ip_raw[3],
//            eth_ip_head->unpack.dst_ip,
//            eth_ip_head->unpack.dst_ip_raw[0],eth_ip_head->unpack.dst_ip_raw[1],eth_ip_head->unpack.dst_ip_raw[2],eth_ip_head->unpack.dst_ip_raw[3]
//    );
//    printf("[0x%016lX][0x%016lX][0x%016lX]\n",(eth_ip_head->raw.value[1] & 0x0000FFFF00000000),(eth_ip_head->raw.value[3] & 0xFFFF000000000000), eth_ip_head->raw.value[4] & 0x000000000000FFFF);
//
//
//
//
//    return -1;


