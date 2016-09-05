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


typedef struct {
        uint64_t raw[10];
} __attribute__((__packed__)) sample_t;


camio_istream_t* dag1_in             = NULL;
camio_istream_t* dag0_in             = NULL;
camio_ostream_t* output              = NULL;
uint64_t total_matched              = 0;
uint64_t total_dropped_before_match = 0;
uint64_t total_dropped_after_match  = 0;
uint64_t total_out_of_time_opt      = 0;

void term(int signum)
{
    if(dag1_in){ dag1_in->delete(dag1_in); }
    if(dag0_in){ dag0_in->delete(dag0_in); }
    if(output) { output->delete(output); }
    exit(0);
}


#define SECS2NS (1000 * 1000 * 1000)
static inline uint64_t fixed_32_32_to_nanos(const sample_t* sample)
{
    dag_record_t* rec = (dag_record_t*)sample;
    uint64_t fixed    = rec->ts;
    if(rec->flags.reserved == 1){
        return fixed;
    }

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





//Go looking for the closest index that matches a given time in nanos
//A linear search. This will be good when the results are close to each other as they should be
static inline void get_index_linear(sample_t* samples, uint64_t samples_len, uint64_t start, uint64_t nanos, uint64_t* idx_out){
    //Fast case
    if(unlikely(nanos <= fixed_32_32_to_nanos(&samples[0]))){
        *idx_out = 0;
        return;
    }


    //Fast case
    if(unlikely(nanos >= fixed_32_32_to_nanos(&samples[samples_len -1]))){
        *idx_out = samples_len -1;
        return;
    }


    int64_t i               = start;
    const uint64_t time_now = fixed_32_32_to_nanos(&samples[i]);
    const int direction     = (int64_t)nanos - (int64_t)time_now > 0 ? 1 : -1;
    int64_t diff_best       = abs((int64_t)nanos - (int64_t)time_now);
    int64_t index_best      = start;

    for(; i < samples_len && i > -1; i += direction){

        const uint64_t time_now = fixed_32_32_to_nanos(&samples[i]);
        const int64_t diff_now = abs((int64_t)nanos - (int64_t)time_now);

        //Walk towards the goal
        if(diff_now < diff_best){
            diff_best = diff_now;
            index_best = i;
        }

        //We're walking away from the goal. Job done!
        if(diff_now > diff_best){
            *idx_out = index_best;
            return;
        }
    }

}



//Given start_nanos and end nanos
//Find the closest indices that correspond to the start and end times
uint64_t start_idx_cache = 0;    //Make it faster in the common case, remember the last found index
uint64_t end_idx_cache   = 0;
void get_range(sample_t* d1_samples, uint64_t samples_len,  int64_t start_nanos, int64_t end_nanos, uint64_t* start_idx_out, uint64_t* end_idx_out){

    get_index_linear(d1_samples,samples_len,start_idx_cache,start_nanos,start_idx_out);
    get_index_linear(d1_samples,samples_len,end_idx_cache,end_nanos,end_idx_out);

    start_idx_cache = *start_idx_out;
    end_idx_cache   = *end_idx_out;
}


typedef struct {
    sample_t cap_dag0;
    uint64_t matched; //Was a match found?
    uint64_t d0_idx;  //Index of this packet in d0
    uint64_t d1_idx;  //Index of the matched packet in d1 (if matched), otherwise 0xFFF...
    int64_t latency;  //Latency if a match was found
} binary_out_t ;


enum { MATCHED = 1, NO_MATCH = 0};
static inline void output_match(
        int matched, sample_t* dag0_sample, sample_t* dag1_sample,
        uint32_t d0_idx, uint32_t d1_idx, camio_ostream_t* out
)
{
    binary_out_t bin_out;
    memcpy(bin_out.cap_dag0.raw, dag0_sample, sizeof(sample_t));
    ((dag_record_t*)(bin_out.cap_dag0.raw))->rlen = htons(sizeof(bin_out)); //Adjust the ERF to include our additional data
    bin_out.d0_idx   = d0_idx;
    bin_out.matched  = matched  ? 1 : 0;
    bin_out.latency  = matched  ? fixed_32_32_to_nanos(dag1_sample) - fixed_32_32_to_nanos(dag0_sample): ~0;
    bin_out.d1_idx   = matched  ? d1_idx : ~0;

    out->assign_write(out, (uint8_t*)&bin_out, sizeof(binary_out_t));
    out->end_write(out,sizeof(binary_out_t));
}

int packet_compare(sample_t* dag0, sample_t* dag1)
{
    return memcmp( (uint8_t*)dag0 + 18, (uint8_t*)dag1 + 18, sizeof(sample_t) - 18);
}


void do_join(sample_t* dag0_data, sample_t* dag1_data,
             uint64_t dag0_len, uint64_t dag1_len,
             uint64_t dag_off, int64_t dag_len, uint64_t window_nanos,
             int vverbose,  camio_ostream_t* out){

    sample_t* dag1_samples      = dag1_data;
    sample_t* dag0_samples      = dag0_data;
    const uint64_t dag0_samples_len = dag0_len / sizeof(sample_t);
    const uint64_t dag1_samples_len = dag1_len / sizeof(sample_t);

    //Sanity check and adjust the len and offset values so that we don't overflow
    dag_len = dag_len < 0 ? dag0_samples_len : dag_len;
    dag_len = MIN(dag_off + dag_len, dag0_samples_len);
    dag_off = MIN(dag_off, dag0_samples_len);

    //End time for dag1, do this now so that we don't have to do the conversion every iteration
    uint64_t dag1_end_ns = fixed_32_32_to_nanos(&dag1_samples[dag1_samples_len -1]);

    //This is the main join loop. We loop over entries in dag0
    //looking for matches in dag1
    uint64_t d0i = dag_off;
    for(; d0i < dag_len ; d0i++){

        if(unlikely(vverbose)){
            if(unlikely(d0i % 100000 == 0)){
                printf("Processed %lu so far...\n", d0i);
            }
        }

        //Some minor optimizations
        //Is the dag0 time, compensated for the sampling window, greater than the
        //dag1 end time. If this is the case, there can be no more matches
        uint64_t dag0_ts_ns = fixed_32_32_to_nanos(&dag0_samples[d0i]);
        if(unlikely(dag0_ts_ns - window_nanos > dag1_end_ns)){
            total_out_of_time_opt++;
            continue;
        }


        //This is basically a transform from dag0 time space into dag1 index space
        //We are looking for a packet plus or minus the current time in d0
        //to be found at an index in d1. get_range() does the conversion.
        //As an optimisation, only start looking minus 250ns. We know that the
        //DAG cards are accurate +/- 100ns. If A packet is more than 250ns in the past
        //the world is broken.
        const uint64_t dag0_time      = fixed_32_32_to_nanos(&dag0_samples[d0i]);
        const int64_t dag0_start_time = dag0_time - 250;
        const int64_t dag0_end_time   = dag0_time + window_nanos;
        uint64_t dag1_start_idx       = 0;
        uint64_t dag1_end_idx         = ~0;
        get_range(dag1_samples,dag1_samples_len,dag0_start_time,dag0_end_time,&dag1_start_idx,&dag1_end_idx);

        //Now we do the join. We have a packet from dag0 at index d0i,
        //we have a range of candidate indexes in dag1 between dag1_start_idx and
        //dag1_end_index. Go looking for a match inside them.
        uint64_t d1i = dag1_start_idx;
        for(; d1i <= dag1_end_idx; d1i++){

            //Found a hash match
            if(packet_compare(&dag1_samples[d1i], &dag0_samples[d0i]) == 0){
                output_match(MATCHED,&dag0_samples[d0i],&dag1_samples[d1i], d0i, d1i, out );
                total_matched++;
                break;
            }
        }

        //We exhausted the range available. There must be no match :-(
        if(d1i > dag1_end_idx){
            //Don't bother outputting a dropped packet until we've found at least one match
            if(total_matched > 1){
                output_match(NO_MATCH,&dag0_samples[d0i],NULL, d0i, ~0, out );
                total_dropped_after_match++;
            }
            else{
                total_dropped_before_match++;
            }
        }
    }

}


typedef struct {
    char* dag1_in;
    char* dag0_in;
    char* output;
    uint64_t offset;
    int64_t length;
    uint64_t window;
    int do_head;
    int csv;
    int binary;
    uint64_t ptype;
    int verbose;
    int vverbose;
} options_t;

int main(int argc, char** argv){
    options_t options = {0};

    camio_options_short_description("dag_join");
    camio_options_add(CAMIO_OPTION_REQUIRED, 'i', "dag0",        "The first input filename. eg /tmp/dag1.blob", CAMIO_STRING, &options.dag0_in, NULL);
    camio_options_add(CAMIO_OPTION_REQUIRED, 'I', "dag1",        "The second input filename. eg /tmp/dag0.blob", CAMIO_STRING, &options.dag1_in, NULL);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'o', "output",      "The dag1 output file in log format. eg log:/tmp/dag1_out.log", CAMIO_STRING, &options.output, "erf-join.out");
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'f', "offset",      "The offset (in records) to jump to for comparison.", CAMIO_UINT64, &options.offset, 0LL);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'l', "length",      "The length in records to process", CAMIO_INT64, &options.length, -1LL);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'w', "window",      "Number of microseconds to keep looking for a match (2500us)", CAMIO_UINT64, &options.window, 2500LL);
    camio_options_add(CAMIO_OPTION_FLAG    , 'v', "verbose",     "Output feedback", CAMIO_BOOL, &options.verbose, 0);
    camio_options_add(CAMIO_OPTION_FLAG    , 'V', "vverbose",    "Output more feedback", CAMIO_BOOL, &options.vverbose, 0);
    camio_options_long_description("Looks for all packets in dag0 that also that appear in dag1.\n"
                                  "Reports packets that are in dag0 that are not in dag1.\n "
                                  "Calculates the latency between cap-in1 and cap-in0.\n Outputs as a\n"
                                  "binary, in ssv and csv formats. Use -f and -l to control the\n"
                                  "offset and length to use from cap-in0. This makes the application\n"
                                  "trivial to parallelize.\n");
    camio_options_parse(argc, argv);

    if(signal(SIGTERM, term) == SIG_ERR){ printf("Could not attach SIGTERM signal handler %s\n", strerror(errno)); }
    if(signal(SIGINT, term)  == SIG_ERR){ printf("Could not attach SIGINT signal handler %s\n",  strerror(errno)); }
    if(signal(SIGQUIT, term) == SIG_ERR){ printf("Could not attach SIGQUIT signal handler %s\n", strerror(errno)); }

    //dag1
    char blob_name[1024];

    snprintf(blob_name,1024,"blob:%s", options.dag0_in);
    dag0_in = camio_istream_new(blob_name,NULL);
    snprintf(blob_name,1024,"blob:%s", options.dag1_in);
    dag1_in = camio_istream_new(blob_name,NULL);
    snprintf(blob_name,1024,"blob:%s", options.output);
    output  = camio_ostream_new(blob_name,NULL);

    sample_t* dag0_data;
    int64_t dag0_len = 0;
    sample_t* dag1_data;
    int64_t dag1_len = 0;

    dag0_len = dag0_in->start_read(dag0_in,(uint8_t**)&dag0_data);

    if(!dag0_len){
        eprintf_exit_simple("Error: Read 0 bytes from dag0. Check that your data is ok?");
    }


    dag1_len = dag1_in->start_read(dag1_in,(uint8_t**)&dag1_data);
    if(!dag1_len){
        eprintf_exit_simple("Error: Read 0 bytes from dag1. Check that your data is ok?");
    }

    if(options.verbose || options.vverbose){
        printf("Opened, dag0 (%lf MBytes, %lu samples)\n", (double)dag0_len / 1024.0 / 1024.0, dag0_len / sizeof(sample_t));
        printf("Opened, dag1 (%lf MBytes, %lu samples)\n", (double)dag1_len / 1024.0 / 1024.0 , dag1_len / sizeof(sample_t));
    }

    do_join(dag0_data, dag1_data, dag0_len, dag1_len,options.offset,options.length, options.window * 1000, options.vverbose, output);

    if(options.verbose || options.vverbose){
        const uint64_t start = options.offset;
        const uint64_t end   = options.offset + options.length -1;
        printf("[%10lu-%10lu] Matched Total:            %lu\n", start,end,total_matched);
        printf("[%10lu-%10lu] Dropped Before 1st Match: %lu\n", start,end,total_dropped_before_match);
        printf("[%10lu-%10lu] Dropped After 1st Match:  %lu\n", start,end,total_dropped_after_match);
        printf("[%10lu-%10lu] Dropped Out of Time:      %lu\n", start,end,total_out_of_time_opt);
    }

    term(0);

    //Unreachable
    return 0;
}


