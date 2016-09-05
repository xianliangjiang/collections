/*
 * Copyright  (C) Matthew P. Grosvenor, 2012, All Rights Reserved
 *
 */

#include "dag_analyse.h"
#include <endian.h>

struct options_t {
    char* in;
    char* out;
    uint64_t offset;
    int64_t length;
    int64_t ptype;
    int do_head;
    int csv;
    int bin;
    int stdout;

} options;

static camio_istream_t* in =  NULL;
static camio_ostream_t* out = NULL;

static void term(int signum){
    printf("Terminating\n");
    if(in){ in->delete(in); }
    if(out){ out->delete(out); }
    exit(0);
}


#define SECS2NS (1000 * 1000 * 1000)
static inline uint64_t fixed_32_32_to_nanos(const dag_record_t* record)
{
    if(record->flags.reserved == 1){
        return record->ts;
    }
    const uint64_t fixed = record->ts;

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


//Read a TCP packet and fill out the details into packet_info struct
static inline void parse_tcp(int len, const uint8_t* data, packet_info_t* packet_info)
{
    packet_info->port_src = ntohs(*(uint16_t*)(data + 0));
    packet_info->port_dst = ntohs(*(uint16_t*)(data + 2));
    packet_info->tcp_seq  = ntohl(*(uint32_t*)(data + 4));
    packet_info->tcp_ack  = ntohl(*(uint32_t*)(data + 8));
    packet_info->tcp_win  = ntohs(*(uint16_t*)(data + 14));
}


//Read a UDP packet and fill out the details into packet_info struct
static inline void parse_udp(int len, const uint8_t* data, packet_info_t* packet_info)
{
    packet_info->port_src     = ntohs(*(uint16_t*)(data + 0 ));
    packet_info->port_dst     = ntohs(*(uint16_t*)(data + 2));
    packet_info->udp_payload8 = *(uint64_t*)(data + 8);
}


//Read an IP packet and fill out the details into packet_info struct
static inline void parse_ip(int len, const uint8_t* data, packet_info_t* packet_info)
{

    //IP options
    if(unlikely(data[0] != 0x45)){
        packet_info->pkt_type = ip_with_opts;
        return;
    }

    packet_info->ip_src = ntohl(*(uint32_t*)(data + 12));
    packet_info->ip_dst = ntohl(*(uint32_t*)(data + 16));
    packet_info->ip_len = ntohs(*(uint16_t*)(data + 2));

    //No options
    const uint8_t ip_type = data[9];
    switch(ip_type){
        //TCP
        case 0x06:{
            packet_info->pkt_type = tcp;
            parse_tcp(len - 20, data + 20, packet_info);
            return;
        }
        //UDP
        case 0x11:{
            packet_info->pkt_type = udp;
            parse_udp(len - 20, data + 20, packet_info);
            return;
        }
        //ICMP
        case 0x01:{
            packet_info->pkt_type = icmp;
            return;

        }
        //In the error case, return the packet hash as the packet type
        default:{
            packet_info->pkt_type = ip_unknown;
            return;
        }
    }

}


//Prase and assign ethernet type to the apcket_ifo struct
static inline void parse_ethernet_type(int len, const uint8_t* eth_header_type, packet_info_t* packet_info)
{
    //Get the ethertype
    const uint16_t eth_type = ntohs(*(uint16_t*)(eth_header_type) );
    switch(eth_type){
        //IPV4
        case 0x0800:{
            parse_ip(len -2 , (eth_header_type + 2), packet_info);
            break;
        }

        //ARP
        case 0x0806:{
            packet_info->pkt_type = arp;
            break;
        }

        //VLAN
        case 0x8100:{
            packet_info->eth_has_vlan = 1;
            packet_info->eth_vlan_id =  ntohs((*(uint16_t*)(eth_header_type + 2))) & 0xFFF ;
            packet_info->eth_vlan_priority = (ntohs(*(uint16_t*)(eth_header_type+ 2))  & 0xF000 ) >> 13;
            parse_ethernet_type(len - 4, eth_header_type + 4, packet_info);
            break;
        }

        default:{
            packet_info->pkt_type = eth_unknown;
            break;
        }
    }

}

//Parse and assign ethernet macs to the packet_info struct
static inline void parse_ethernet_macs(int len, const uint8_t* eth_header, packet_info_t* packet_info)
{
    memcpy(&packet_info->eth_mac_dst,eth_header,6);
    memcpy(&packet_info->eth_mac_src,eth_header + 6,6);

    packet_info->eth_mac_dst = be64toh(packet_info->eth_mac_dst) >> 16;
    packet_info->eth_mac_src= be64toh(packet_info->eth_mac_src) >> 16;

}

static inline void parse_packet(const dag_record_t* data, packet_info_t* packet_info)
{
    packet_info->pkt_timestamp_ns = fixed_32_32_to_nanos(data);
    packet_info->pkt_lost_count   = ntohs(data->lctr);
    packet_info->pkt_wlen         = ntohs(data->wlen);

    const uint8_t* eth_header = ((uint8_t*)data) + 18; //ERF record is 16bytes long plus 2 bytes ethernet padding
    const uint16_t len = ntohs(data->rlen) - 18;

    if(unlikely( ntohs(data->wlen) < 14)){
        packet_info->pkt_type = unknown;
        return;
    }

    //Magic packet with latency extensions
    if(ntohs(data->rlen) == 14 * sizeof(uint64_t)){
        packet_info->pkt_has_latency = 1;
        lat_match_rec_t* lat_match_rec = (lat_match_rec_t*)data;
        packet_info->pkt_matched = lat_match_rec->matched;
        packet_info->pkt_d0_idx  = lat_match_rec->d0_idx;
        packet_info->pkt_d1_idx  = lat_match_rec->d1_idx;
        packet_info->pkt_latency = lat_match_rec->latency;

    }

    parse_ethernet_macs(len , eth_header, packet_info);
    parse_ethernet_type(len - 12, eth_header + 12, packet_info); //Skip over the MACs //Will also do IP/UDP/TCP parsing

    //Make the packet hashes
    if(packet_info->pkt_type != udp){
        //Hash is the middle 8B of an MD5 hash of the entire packet
        unsigned char digest[16];
        MD5_CTX context;
        MD5_Init(&context);
        MD5_Update(&context, eth_header, len);
        MD5_Final(digest, &context);
        packet_info->pkt_hash = *(uint64_t*)(digest + 4);
    }
    //This maintains previous behavior, although the above is probably better
    else{
        packet_info->pkt_hash = packet_info->udp_payload8;
    }
}


static inline int generate_txt_head(uint8_t* out, int64_t len, int is_csv)
{
    char sep = is_csv ? ',' : ' ';
    int off = 0;
    //General headers
    off += snprintf((char*)out + off, len - off, "%19s%c", "Time now", sep);
    off += snprintf((char*)out + off, len - off, "%12s%c", "Since start", sep);
    off += snprintf((char*)out + off, len - off, "%12s%c",  "Since last", sep);
    off += snprintf((char*)out + off, len - off, "%4s%c",  "Lost", sep);
    off += snprintf((char*)out + off, len - off, "%4s%c",  "Type", sep);
    off += snprintf((char*)out + off, len - off, "%5s%c",  "Wlen", sep);
    off += snprintf((char*)out + off, len - off, "%12s%c", "Dst MAC", sep);
    off += snprintf((char*)out + off, len - off, "%12s%c", "Src MAC", sep);

    //Vlans
    off += snprintf((char*)out + off, len - off, "%4s%c", "Vlid", sep);
    off += snprintf((char*)out + off, len - off, "%4s%c", "Prty", sep);

    //Latency magic
    off += snprintf((char*)out + off, len - off, "%5s%c", "Match", sep);
    off += snprintf((char*)out + off, len - off, "%4s%c", "Idx0", sep);
    off += snprintf((char*)out + off, len - off, "%4s%c", "Idx1", sep);
    off += snprintf((char*)out + off, len - off, "%7s%c", "Latency", sep);


    off += snprintf((char*)out + off, len - off, "\n");
    return off;

}


static inline int generate_txt_rec(uint8_t* out, int64_t len, uint64_t time_start_ns, packet_info_t* pkt_info, packet_info_t* pkt_info_prev, packet_info_t* pkt_next, int is_csv)
{
    char sep = is_csv ? ',' : ' ';

    const int64_t time_now          = pkt_info->pkt_timestamp_ns;
    const int64_t since_start       = time_now - time_start_ns;
    const int64_t time_diff_prev    = time_now - pkt_info_prev->pkt_timestamp_ns;
    const int64_t lost              = pkt_info->pkt_lost_count;
    const int64_t type              = pkt_info->pkt_type;
    const int64_t wlen              = pkt_info->pkt_wlen;
    const uint64_t mac_dst          = pkt_info->eth_mac_dst;
    const uint64_t mac_src          = pkt_info->eth_mac_src;

    int off = 0;
    //Generic packet info
    off += snprintf((char*)out + off, len - off, "%19lu%c",  time_now, sep);
    off += snprintf((char*)out + off, len - off, "%12lu%c",  since_start, sep);
    off += snprintf((char*)out + off, len - off, "%12lu%c",  time_diff_prev, sep);
    off += snprintf((char*)out + off, len - off, "%4lu%c",   lost, sep);
    off += snprintf((char*)out + off, len - off, "%4lx%c",   type, sep);
    off += snprintf((char*)out + off, len - off, "%5lu%c",   wlen, sep);
    off += snprintf((char*)out + off, len - off, "%012lx%c", mac_dst, sep);
    off += snprintf((char*)out + off, len - off, "%012lx%c", mac_src, sep);

    //Vlan info
    if(pkt_info->eth_has_vlan){
        off += snprintf((char*)out + off, len - off, "%4lu%c", pkt_info->eth_vlan_id, sep);
        off += snprintf((char*)out + off, len - off, "%1lu%c", pkt_info->eth_vlan_priority, sep);
    }
    else{
        off += snprintf((char*)out + off, len - off, "%4s%c", "    ", sep);
        off += snprintf((char*)out + off, len - off, "%4s%c", "    ", sep);
    }

    //Latency magic
    if(pkt_info->pkt_has_latency){
        off += snprintf((char*)out + off, len - off, "%5lu%c", pkt_info->pkt_matched, sep);
        off += snprintf((char*)out + off, len - off, "%4lu%c", pkt_info->pkt_d0_idx, sep);
        if(pkt_info->pkt_matched){
            off += snprintf((char*)out + off, len - off, "%4lu%c", pkt_info->pkt_d1_idx, sep);
            off += snprintf((char*)out + off, len - off, "%7lu%c", pkt_info->pkt_latency, sep);
        }
        else{
            off += snprintf((char*)out + off, len - off, "%4s%c", "    ", sep);
            off += snprintf((char*)out + off, len - off, "%7s%c", "       ", sep);
        }
    }
    else{
        off += snprintf((char*)out + off, len - off, "%5s%c", "     ", sep);
        off += snprintf((char*)out + off, len - off, "%4s%c", "    ", sep);
        off += snprintf((char*)out + off, len - off, "%4s%c", "    ", sep);
        off += snprintf((char*)out + off, len - off, "%7s%c", "       ", sep);

    }

    off += snprintf((char*)out + off, len - off, "\n");
    return off;

}


static inline void generate_bin_head(uint8_t* out, int64_t start_time, int64_t end_time, int64_t samples )
{
    dag_cap_head_t* bin_head = (dag_cap_head_t*)out;
    bin_head->start_time = start_time;
    bin_head->end_time   = end_time;
    bin_head->samples    = samples;
}


static inline int generate_bin_rec(uint8_t* out, int64_t len, packet_info_t* pkt_info )
{
    if(len < sizeof(sample_t)){
        eprintf_exit_simple("Something is very wrong. Buffer length <  minimum record size! What did you break???\n");
    }

    sample_t* sample = (sample_t*)out;
    sample->timestamp = pkt_info->pkt_timestamp_ns;
    sample->value_type_dropped_len.type     = pkt_info->pkt_type;
    sample->value_type_dropped_len.dropped  = pkt_info->pkt_lost_count > 255 ? ~0 : pkt_info->pkt_lost_count ;
    sample->value_type_dropped_len.length   = pkt_info->pkt_type;
    sample->hash = pkt_info->pkt_hash;

    return sizeof(sample_t);

}


int main(int argc, char** argv){
    camio_options_short_description("dag_analyse");
    camio_options_add(CAMIO_OPTION_REQUIRED, 'i', "dag0-in",     "Dag ERF input file name. eg /tmp/dag_cap_A", CAMIO_STRING, &options.in, NULL);
    camio_options_add(CAMIO_OPTION_FLAG    , 's', "std-out",     "Write the output to std-out", CAMIO_BOOL, &options.stdout, 0);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'o', "dag0-out",    "Dag ERF output file name. eg /tmp/dag_cap_A.txt [erf.out]", CAMIO_STRING, &options.out, "erf.out");
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'f', "dag0-off",    "The offset (in records) to jump to in dag0.", CAMIO_UINT64, &options.offset, 0);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'l', "dag0-len",    "The length (in records) to display from dag0. (-1 == all)", CAMIO_INT64, &options.length, 25LL);
    camio_options_add(CAMIO_OPTION_OPTIONAL, 'p', "packet-type", "Filter by packets with the given type. 1=tcp, 2=udp, 3=icmp, -1 == all", CAMIO_INT64, &options.ptype, -1LL);
    camio_options_add(CAMIO_OPTION_FLAG    , 'H', "write-header","Write a header describing each file's columns", CAMIO_BOOL, &options.do_head, 0);
    camio_options_add(CAMIO_OPTION_FLAG    , 'c', "use-csv",     "Write the output in CSV format (default = False)", CAMIO_BOOL, &options.csv, 0);
    camio_options_add(CAMIO_OPTION_FLAG    , 'b', "use-bin",     "Write the output in bin format (default = False)", CAMIO_BOOL, &options.bin, 0);

    camio_options_long_description("Converts dag capture files into ASCII. Reports basic statistics about each file.");
    camio_options_parse(argc, argv);

    if(signal(SIGTERM, term) == SIG_ERR){ printf("Could not attach SIGTERM signal handler %s\n", strerror(errno)); }
    if(signal(SIGINT, term)  == SIG_ERR){ printf("Could not attach SIGINT signal handler %s\n", strerror(errno));  }
    if(signal(SIGQUIT, term) == SIG_ERR){ printf("Could not attach SIGQUIT signal handler %s\n", strerror(errno)); }

    if(options.bin && options.csv){
        eprintf_exit_simple("Error! Cannot enable both CSV and BINary modes at the same time. Please chose one\n");
    }

    char blob_name[256];

    snprintf(blob_name,256,"blob:%s", options.in);
    in = camio_istream_new(blob_name, NULL);

    snprintf(blob_name,256,"blob:%s", options.out);
    out = camio_ostream_new(blob_name, NULL);

    uint8_t* dag_data_head = NULL;
    size_t len = in->start_read(in,(uint8_t**)&dag_data_head);
    printf("Opened %s. Total file size %lu\n", options.in, len);

    if(len < 80){
        eprintf_exit_simple("Not enough data to continue. Check input files\n");
    }

    if(options.length > 0){
        printf("Starting at offset %lu ending at offset %lu\n", options.offset, options.offset + options.length);
    }

    //Advance into the stream, move the data pointer as we go
    //if we get to the end of the stream, bug out.
    dag_record_t* dag_rec = (dag_record_t*)dag_data_head;
    int64_t start_time = fixed_32_32_to_nanos(dag_rec);
    int64_t current_time = start_time;

    int64_t rec_idx = 0;
    for(; rec_idx < options.offset;
            rec_idx++,
            current_time = fixed_32_32_to_nanos(dag_rec),
            dag_rec = (dag_record_t*)((uint8_t*)dag_rec + ntohs(dag_rec->rlen))
    ){
        if((uint8_t*)dag_rec >= (uint8_t*)dag_data_head + len){
            printf("Ran out of samples at record number %li. Exiting.\n", rec_idx);
            term(0);
        }

        if(ntohs(dag_rec->rlen) != 10 * sizeof(uint64_t) && dag_rec->rlen == 14 * sizeof(uint64_t)){
            eprintf_exit_simple("Error at index %li DAG ERF samples must be either 80B or 112B in length (Size is %lu)\n", rec_idx, ntohs(dag_rec->rlen));
        }
    }


    //Output the header if we need it
    uint8_t head_out[1024];

    if(options.do_head){
        int len = generate_txt_head(head_out,1024, options.csv);
        out->assign_write(out,head_out,len);
        out->end_write(out,len);

    }
    else if (options.bin){
        generate_bin_head(head_out,start_time,~0ULL,~0ULL);
        out->assign_write(out,head_out,sizeof(dag_cap_head_t));
        out->end_write(out,sizeof(dag_cap_head_t));

    }

    int64_t bytes = 0;
    int64_t recs_out = 0;
    dag_record_t* dag_rec_prev = dag_rec;
    for(; rec_idx < options.offset + options.length;
            rec_idx++,
            current_time = fixed_32_32_to_nanos(dag_rec),
            dag_rec_prev = dag_rec,
            dag_rec      = (dag_record_t*)((uint8_t*)dag_rec + ntohs(dag_rec->rlen)) ){

        if((uint8_t*)dag_rec >= (uint8_t*)dag_data_head + len){
            printf("Exiting at record number %li. Output %lu records\n", rec_idx, recs_out);
            break;
        }

        if(ntohs(dag_rec->rlen) != 10 * sizeof(uint64_t) && dag_rec->rlen == 14 * sizeof(uint64_t)){
            eprintf_exit_simple("Error at index %li DAG ERF samples must be either 80B or 112B in length (Size is %lu)\n", rec_idx, ntohs(dag_rec->rlen));
        }

        dag_record_t* dag_rec_next = (uint8_t*)dag_rec + ntohs(dag_rec->rlen) < (uint8_t*)dag_data_head + len ?
                    (dag_record_t*)((uint8_t*)dag_rec + ntohs(dag_rec->rlen)) : dag_rec;

        packet_info_t pkt_info = {0};
        packet_info_t pkt_info_prev = {0};
        packet_info_t pkt_info_next = {0};
        parse_packet(dag_rec,&pkt_info);
        parse_packet(dag_rec_prev,&pkt_info_prev);
        parse_packet(dag_rec_next,&pkt_info_next);



        //Filter packets by type
        if(unlikely(options.ptype != -1 && pkt_info.pkt_type != options.ptype)){
            continue;
        }

        bytes += pkt_info.pkt_wlen;

        uint8_t out_rec[1024];
        if(options.bin){
            int len = generate_bin_rec(out_rec, 1024, &pkt_info);
            out->assign_write(out,out_rec,len);
            out->end_write(out,len);
            ++recs_out;
        }
        else{
            int len = generate_txt_rec(out_rec, 1024, start_time, &pkt_info, &pkt_info_prev, &pkt_info_next, options.csv);
            out->assign_write(out,out_rec,len);
            out->end_write(out,len);
            if(options.stdout){
                printf("%s", out_rec);
            }
            ++recs_out;
        }
    }

    const double total_time_nanos = (double)(current_time - start_time);
    const double total_time_secs  = (double)(total_time_nanos) / 1000.0 / 1000.0 / 1000.0;

    double bw_bytes = ((double)bytes / total_time_secs) / 1024.0 / 1024.0 ;
    double bw_bits  = ((double)bytes / total_time_secs) / 1000.0 / 1000.0 * 8;

    printf("Total time = %lfns (%lfsecs) BW = %lfMB/s %lf Mb/s\n", total_time_nanos, total_time_secs, bw_bytes, bw_bits);

    //Finalize the header if we are in binary mode
    if(options.bin){
        int fd = open(options.in, O_WRONLY);
        dag_cap_head_t head;
        generate_bin_head((uint8_t*)&head,start_time,current_time,rec_idx + 1);
        write(fd,&head,sizeof(dag_data_head));
        close(fd);
    }


    term(0);

    //Unreachable
    return 0;
}
