/*
 * Copyright  (C) Matthew P. Grosvenor, 2012, All Rights Reserved
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <byteswap.h>
#include <ctype.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include <sys/mman.h>

#include "camio/selectors/camio_selector.h"
#include "camio/camio_util.h"
#include "camio/camio_errors.h"
#include "camio/istreams/camio_istream.h"
#include "camio/ostreams/camio_ostream.h"
#include "camio/ostreams/camio_ostream_raw.h"
#include "camio/dag/dagapi.h"
#include "camio/options/camio_options.h"


#include <time.h>
#include <sys/time.h>
#include <math.h>

//
//typedef struct {
//    uint64_t timestamp;     //DAG timestamps are 32.32bit fixed point numbers.
//
//    union {
//        struct{
//            uint32_t length : 16;       //Length of the packet (up to 65kB (as per dag card wlen))
//            uint32_t dropped: 8;        //Number of dropped packets behind this one
//            uint32_t type   : 8;        //Type of the packet
//            uint32_t port;              //Port on the card that the packet was in
//        } value_port_type_dropped_len;
//        uint64_t raw_port_type_dropped_len;  //Raw 64bit value so I can do the & myself.
//    };
//
//    uint64_t hash;                      //64bit unique identifier for a given packet
//
//} __attribute__((__packed__)) sample_t;


typedef struct {
	uint64_t raw[10]; //80 byte ERF record
} __attribute__((__packed__)) sample_t;



typedef struct {
	uint64_t timestamp;     		//DAG timestamps are 32.32bit nano secs since 1970
	uint32_t raw_type_dropped_len;  //Raw 32bit value so I can do the & myself.
	uint64_t hash;                  //64bit unique identifier for a given packet
} __attribute__((__packed__)) sample_out_t;

static struct options_t {
	char* dag_in;
	uint64_t samples;
	uint64_t timeout_secs;
}  options;




static size_t bytes = 0;

//Allocate memory for sampling return NULL on failure
sample_t* alloc_sample_mem(size_t samples){
	if(sizeof(sample_t) != 10 * sizeof(uint64_t)){
		printf("Error sample_t is not 80B (%lu)\n",  sizeof(sample_t));
		exit(-1);
	}

	//Round up to the nearest integer page size
	bytes = (sizeof(sample_t) * samples) % getpagesize() == 0 ? (sizeof(sample_t) * samples) :  (((sizeof(sample_t) * samples) / getpagesize()) + 1) * getpagesize();
	printf("Rounded up from %luB to %luB (%luMB)\n", samples * sizeof(sample_t), bytes, bytes / 1024 / 1024);
	//sample_t* result = malloc(bytes);
	sample_t* result = valloc(bytes);
	if(result){
		bzero(result,bytes);
	}

	if(result){
		if(mlock(result,bytes)){
			printf("mlock failed!\n");
		}
	}


	printf("Touching memory...");
	int i = 0;
	for(; i < bytes; i+= 4096){
		((char*)result)[i] = 0;
	}
	printf("Done.\n");

	//Let linux know that we'll be done with these pages directly after writing to them.
	if(result){
		madvise(result, bytes, MADV_SEQUENTIAL);
	}

	return result;
}


static camio_istream_t* dagcap = NULL;
static sample_t* sample_space = NULL;
static uint64_t samples_total = 0;
static uint64_t samples_[4] = { 0, 0, 0, 0 };
static uint64_t sample_time = 0;
static uint64_t sampling_started_ns;
static uint64_t sampling_ended;
//static char dag_card = 0;


#define SECS2NS (1000 * 1000 * 1000)
static inline uint64_t fixed_32_32_to_nanos(dag_record_t* data){

    if(data->flags.reserved){ //HACK! Use the reserved flag to indicate if ExaNIC nanos timestamp was used
	    return data->ts;
	}

    const uint64_t fixed = data->ts;
    uint64_t subsecs = ((fixed & 0xFFFFFFFF) * SECS2NS) >> 32;
	uint64_t seconds = (fixed >> 32) * SECS2NS;


	//Deal with rounding
	if(unlikely( 0x4 & fixed)){
		subsecs += 1;
	}

	if(unlikely( (0x2 & fixed) || (0x1 & fixed) )){
		subsecs += 1;
	}

	return seconds + subsecs;
}

static void term2(int signum){
	printf("Handling exit, please wait...\n");
}


static void term(int signum){
	signal(SIGTERM, term2);
	signal(SIGINT, term2);
	signal(SIGQUIT, term2);

	printf("Handling exit...\n");
	if(dagcap){ dagcap->delete(dagcap); }

	if(sample_space){
		sampling_ended = sample_time;

		munlock(sample_space,bytes);
		char sample_file_a[20] = "blob:/tmp/dag_cap_A";
		char sample_file_b[20] = "blob:/tmp/dag_cap_B";
        char sample_file_c[20] = "blob:/tmp/dag_cap_C";
        char sample_file_d[20] = "blob:/tmp/dag_cap_D";


		printf("Shutting down. Outputting %lu samples (A=%lu B=%lu,C=%lu,D=%lu))\n", samples_total, samples_[0], samples_[1], samples_[2], samples_[3] );

		camio_ostream_t* out_a = camio_ostream_new(sample_file_a,NULL);
		camio_ostream_t* out_b = camio_ostream_new(sample_file_b,NULL);
        camio_ostream_t* out_c = camio_ostream_new(sample_file_c,NULL);
        camio_ostream_t* out_d = camio_ostream_new(sample_file_d,NULL);

		camio_ostream_t* outs[4] = { out_a, out_b, out_c, out_d };

		int port = 0;
		if(samples_total){
			sample_t* sample = sample_space;
			printf("Started  = %lu\n", sampling_started_ns );

			int64_t samples_out_a = 0;
			int64_t samples_out_b = 0;
			int64_t samples_out_c = 0;
			int64_t samples_out_d = 0;

			size_t i        = 0;
			for(; i < samples_total; i++, sample++){
				dag_record_t* data = (dag_record_t*)sample;
				port = data->flags.iface;

				samples_out_a += port == 0 ? 1 : 0;
				samples_out_b += port == 1 ? 1 : 0;
				samples_out_c += port == 2 ? 1 : 0;
				samples_out_d += port == 3 ? 1 : 0;

				data->rlen = ntohs(sizeof(sample_t));

				outs[port]->assign_write(outs[port],(uint8_t*)data, sizeof(sample_t));
				outs[port]->end_write(outs[port], sizeof(sample_t));
			}

			printf("Wrote out %lu samples for A and %lu samples for B, %lu samples for C and %lu samples for D\n", samples_out_a, samples_out_b,samples_out_c, samples_out_d);

			outs[0]->delete(outs[0]);
			outs[1]->delete(outs[1]);
			outs[1]->delete(outs[2]);
			outs[1]->delete(outs[3]);
		}
	}

	exit(0);
}




#ifndef NDEBUG
#define dprintf(format, ... )  printf (format, ##__VA_ARGS__)
#else
#define dprintf(format, ... )
#endif


static inline void dump_erf(dag_record_t* record){
#ifndef NDEBUG
    dprintf("ERF [");

	dprintf("ts:0x%016lu ", fixed_32_32_to_nanos(record) );
	dprintf("tp:0x%02x ", record->type);

	const char* vlen  = record->flags.vlen      ? "|V" : "";
	const char* trunc = record->flags.trunc     ? "|T" : "";
	const char* rxerr = record->flags.rxerror   ? "|R" : "";
	const char* dserr = record->flags.dserror   ? "|E" : "";
	const char* dir   = record->flags.direction ? "|D" : "";

	dprintf("fl[%02u%s%s%s%s%s][0x%02x] ", record->flags.iface, vlen, trunc, rxerr, dserr, dir, *(uint8_t*)(&record->flags));

	dprintf("rl:%u ", ntohs(record->rlen));
	dprintf("lc:%u ", ntohs(record->lctr));
	dprintf("wl:%u]\n", ntohs(record->wlen));
#endif
}




int main(int argc, char** argv){

	camio_options_short_description("dag_capture_ng");
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'i', "input",      "The DAG/ExaNIC input card to listen on e.g exa:exanic0 or dag:/dev/dag0 [dag:/dev/dag0]", CAMIO_STRING, &options.dag_in, "dag:/dev/dag0");
	camio_options_add(CAMIO_OPTION_OPTIONAL, 's', "samples",     "Number of samples to capture [1000000]", CAMIO_UINT64, &options.samples, 1000000 );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 't', "timeout",     "Time in seconds to listen for samples, 0 = unlimited [10]", CAMIO_UINT64, &options.timeout_secs, 1200);
	camio_options_parse(argc, argv);

	printf("Timeout=%lusecs", options.timeout_secs);

	//Lock all memory into RAM, this will make the process very RAM hungry, but that's ok.
	//we want to avoid being hit with paging requests
	//mlockall( MCL_CURRENT | MCL_FUTURE);
	//Alloc a chunk of memory to record to
	sample_space = alloc_sample_mem(options.samples);
	if(!sample_space){
		printf("Error, could not allocate memory for %lu packet samples\n", options.samples);
		exit(-1);
	}


	if(signal(SIGTERM, term) == SIG_ERR){
		printf("Could not attach SIGTERM signal handler %s\n", strerror(errno));
	}
	if(signal(SIGINT, term) == SIG_ERR){
		printf("Could not attach SIGINT signal handler %s\n", strerror(errno));
	}
	if(signal(SIGQUIT, term) == SIG_ERR){
		printf("Could not attach SIGQUIT signal handler %s\n", strerror(errno));
	}

	//Inputs
	dagcap = camio_istream_new(options.dag_in, NULL);

	dag_record_t* data = NULL;
	int run = 1;
	int first = 1;
	int len = 0;

	sample_t* sample 	 = sample_space;
	uint64_t end_time_ns = options.timeout_secs * SECS2NS; //Nasty, put this here and wait for the first sample to come by, avoids multiplication in the critical loop


	dprintf("Using debug dump\n");
	while(run){
		len = dagcap->start_read(dagcap,(uint8_t**)(&data));

		dump_erf(data);

		if(unlikely(len == 0)){
			dagcap->end_read(dagcap, NULL);
			dprintf("len=0");
			continue;
		}

//		if(unlikely(data->flags.iface > 1)){
//			printf("## Warning! Packet from an unknown port on the card (%u)\n", data->flags.iface);
//			continue;
//		}


		samples_[data->flags.iface]++;
		samples_total++; //Got a new sample

		if(unlikely(ntohs(data->wlen) == 0)){
			dagcap->end_read(dagcap, NULL);
			continue;
		}


		if(unlikely(first)){
            sampling_started_ns = fixed_32_32_to_nanos(data);
			end_time_ns += sampling_started_ns; //Nasty, add the first sample to the timeout nanos to get an end time
			first = 0;
		}

		data->rlen = htons(80); //Make this a valid erf packet

		//Loop unroll the assignment.
		sample->raw[0] = ((int64_t*)data)[0];
		sample->raw[1] = ((int64_t*)data)[1];
		sample->raw[2] = ((int64_t*)data)[2];
		sample->raw[3] = ((int64_t*)data)[3];
		sample->raw[4] = ((int64_t*)data)[4];
		sample->raw[5] = ((int64_t*)data)[5];
		sample->raw[6] = ((int64_t*)data)[6];
		sample->raw[7] = ((int64_t*)data)[7];
		sample->raw[8] = ((int64_t*)data)[8];
		sample->raw[9] = ((int64_t*)data)[9];


		dagcap->end_read(dagcap, NULL);
		sample++;

		if(unlikely(samples_total >= options.samples)){
			break;
		}


		if(unlikely(options.timeout_secs && fixed_32_32_to_nanos(data) > end_time_ns)){
			printf("Timeout after %lu seconds\n", options.timeout_secs);
			break;
		}

	}

	term(0);

	//Unreachable
	return 0;
}

