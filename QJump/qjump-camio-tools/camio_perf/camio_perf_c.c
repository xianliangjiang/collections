/*
 * Copyright  (C) Matthew P. Grosvenor, 2012, All Rights Reserved
 *
 */
//Link against the maths library
//#LINKFLAGS=-lm


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
#include "camio/options/camio_options.h"



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



camio_ostream_t* out = NULL;

uint64_t seq_num = 0;
struct timeval t_start, t_end;
uint64_t size = 0;

void term(int signum){
    gettimeofday(&t_end,0);


    struct timeval tv;
    timeval_subtract(&tv,&t_end, &t_start);
    double seconds =  tv.tv_sec + (double)tv.tv_usec / 1000.0 / 1000.0;

    printf("camio_perf sent %lu packets in %lf seconds (bw=%fMbs), pps=%f\n", seq_num, seconds, ((seq_num * size * 8) / seconds) / 1000.0 / 1000.0, seq_num / seconds );
    if(out){ out->delete(out); }
    exit(0);
}


#define SEC2US (1000 * 1000)
uint64_t cpu_frequency_hz    = 0;


static __inline__  uint64_t get_cycles(void)
{
     uint32_t a, d;
     asm volatile("rdtsc" : "=a" (a), "=d" (d));

     return (((uint64_t)a) | (((uint64_t)d) << 32));
}


void delay(uint64_t usecs)
{
    //Don't sleep if we don't have to
    if(!usecs) return;

    //Use the system sleep if it's a long time
    if(usecs > 1000){
        usleep(usecs);
        return;
    }

    //Busy wait
    uint64_t delay_cycles = (usecs * cpu_frequency_hz) /  SEC2US;
    uint64_t end_cycles = get_cycles() + delay_cycles;
    while(get_cycles() < end_cycles){
        asm("pause");
    }
}


typedef struct {
    char* ip;
    double cpu;
    double delay_nanos;
    uint64_t size;
    uint64_t seq;
    uint64_t pacing;
} options_t;


int main(int argc, char** argv){

    signal(SIGTERM, term);
    signal(SIGINT, term);

    options_t options = {0};
    camio_options_short_description("camio_perf");
    camio_options_add(CAMIO_OPTION_REQUIRED, 'i', "ip-port",     "IP address and port in camio style eg udp:127.0.0.1:2000",  CAMIO_STRING, &options.ip );

    camio_options_add(CAMIO_OPTION_OPTIONAL, 's', "size",        "Size each packet should be", CAMIO_UINT64, &options.size, 8ULL );
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'q', "seq-start",   "Starting number for sequences", CAMIO_UINT64, &options.seq, 0ULL );
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'p', "pacing",      "Time in us between pacing intervals", CAMIO_UINT64, &options.pacing, 0ULL );
    camio_options_long_description("Send packets as fast as possible to --ip-port.  Wait --delay nanoseconds between sends");
    camio_options_parse(argc, argv);

    options.size = options.size < sizeof(uint64_t) ? sizeof(uint64_t) : options.size; 
    size = options.size;
    printf("Allocating %luB for each packet\n", options.size);
    uint64_t* data = malloc(options.size);


    uint64_t ts_start_us  = 0;
    uint64_t ts_end_us    = 0;
    uint64_t start_cycles = 0;
    uint64_t end_cycles   = 0;


    struct timeval ts;
    printf("Calculating CPU speed...\n");
    gettimeofday(&ts,NULL);
    ts_start_us = (ts.tv_usec + ts.tv_sec * SEC2US );
    start_cycles = get_cycles();
    while(1){
        gettimeofday(&ts,NULL);
        ts_end_us = (ts.tv_usec + ts.tv_sec * SEC2US );
        if((ts_end_us - ts_start_us) >= 2 * SEC2US){
            end_cycles = get_cycles();
            break;
        }
    }
    cpu_frequency_hz = (end_cycles - start_cycles) / 2;
    printf("CPU is running at %lu cyles per second\n", cpu_frequency_hz);



    //Output
    out = camio_ostream_new(options.ip, NULL);

    gettimeofday(&t_start,0);

    while(likely(1)){
        out->assign_write(out, (uint8_t*)data, size );
        out->end_write(out, size);
        data[0] = seq_num++;

        delay(options.pacing);

        //Report stats
        if(seq_num && seq_num % 10000 == 0){
            gettimeofday(&t_end,0);
            struct timeval tv;
            timeval_subtract(&tv,&t_end, &t_start);
            double seconds =  tv.tv_sec + (double)tv.tv_usec / 1000.0 / 1000.0;

            printf("camio_perf sent %lu packets in %lf seconds (bw=%fMbs), pps=%f\n", seq_num, seconds, ((seq_num * size * 8) / seconds) / 1000.0 / 1000.0, seq_num / seconds );
        }
    }

    term(0);

    return 0; //Unreachable
}

