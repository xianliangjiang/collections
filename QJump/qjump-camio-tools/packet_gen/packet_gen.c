//Include the maths library
//#LINKFLAGS=-lm

/*
 * Copyright  (C) Matthew P. Grosvenor, 2012, All Rights Reserved
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <ifaddrs.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <float.h>

#include "camio/istreams/camio_istream_log.h"
#include "camio/ostreams/camio_ostream_log.h"
#include "camio/istreams/camio_istream_netmap.h"
#include "camio/ostreams/camio_ostream_netmap.h"
#include "camio/options/camio_options.h"
#include "camio/camio_types.h"
#include "camio/camio_util.h"
#include "camio/camio_errors.h"

#include "r2r2_control.h"
#include "arp.h"


struct packet_gen_opt_t{
	char* iface;
	uint64_t len;
	uint64_t src_ip;
	uint64_t dst_ip;
	uint64_t dst_mac;
	uint64_t init_seq;
	int use_seq;

	char* clock;
	char* listener;
	char* selector;
	uint64_t burst;
	int64_t pid;
	uint64_t offset;
	int64_t num;

	int rdelay;
	uint64_t timeout;
	uint64_t burst_window;

	int64_t vlan_id;
	int64_t vlan_prioirty;
	int64_t prio_packet;

	int distagg_client;
	int distagg_server;

} options ;

typedef struct {
	uint8_t  ihl        : 4;
	uint8_t  ver        : 4;
	uint8_t  ecn        : 2;
	uint8_t  dscp       : 6;
	uint16_t total_len;
	uint16_t id;
	uint16_t frag_off_flags;
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t hdr_csum;
	union{
		uint8_t src_ip_raw[4];
		uint32_t src_ip;
	};
	union{
		uint8_t dst_ip_raw[4];
		uint32_t dst_ip;
	};
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t udp_len;
	uint16_t udp_csum;

} __attribute__((__packed__)) ip_udp_packet_t;



//A single packed structure containing an entire packet laid out in memory
typedef struct{
	union{
		struct{
			uint64_t value[6];
		} __attribute__((__packed__)) raw;

		struct{
			uint8_t  dst_mac_raw[6];
			uint8_t  src_mac_raw[6];
			uint16_t eth_type;
			ip_udp_packet_t ip_udp_packet;
		} __attribute__((__packed__)) unpack;

	};
} __attribute__((__packed__)) eth_ip_udp_head_t ;


typedef struct {
	uint16_t tpid;
	uint16_t pcp_dei_vid ;
} __attribute__((__packed__)) vlan_tag_t;


//A single packed structure containing an entire VLAN-tagged packet laid out in memory
typedef struct{
	union{
		struct{
			uint64_t value[6];
		} __attribute__((__packed__)) raw;

		struct{
			uint8_t  dst_mac_raw[6];
			uint8_t  src_mac_raw[6];
			vlan_tag_t vlan_tag;
			uint16_t eth_type;
			ip_udp_packet_t ip_udp_packet;
		} __attribute__((__packed__)) unpack;

	};
} __attribute__((__packed__)) eth_vlan_ip_udp_head_t ;




arp_packet_t arp_packet;
uint64_t src_mac;

int source_hwaddr(const char *ifname, uint64_t* mac)
{
	struct ifaddrs *ifaphead, *ifap;
	int l = sizeof(ifap->ifa_name);

	if (getifaddrs(&ifaphead) != 0) {
		eprintf_exit_simple( "getifaddrs %s failed", ifname);
		return (-1);
	}

	for (ifap = ifaphead; ifap; ifap = ifap->ifa_next) {
		struct sockaddr_ll *sll = (struct sockaddr_ll *)ifap->ifa_addr;

		if (!sll || sll->sll_family != AF_PACKET){
			continue;
		}

		if (strncmp(ifap->ifa_name, ifname, l) != 0){
			continue;
		}

		memcpy(mac, sll->sll_addr, sizeof(uint64_t));
		char buf[256];
		sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", ((uint8_t*)mac)[0], ((uint8_t*)mac)[1], ((uint8_t*)mac)[2], ((uint8_t*)mac)[3], ((uint8_t*)mac)[4], ((uint8_t*)mac)[5]);
		printf("Source MAC address: %s\n",buf);
		break;
	}

	freeifaddrs(ifaphead);
	return ifap ? 0 : 1;
}

/* Compute the checksum of the given ip header. */
static uint16_t checksum(const void *data, uint16_t len, uint32_t sum)
{
	const uint8_t *addr = data;
	uint32_t i;

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (len & ~1U); i += 2) {
		sum += (u_int16_t)ntohs(*((u_int16_t *)(addr + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < len) {
		sum += addr[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return sum;
}

static u_int16_t
wrapsum(u_int32_t sum)
{
	sum = ~sum & 0xFFFF;
	return (htons(sum));
}

void preprare_ip_packet(ip_udp_packet_t* ip_udp_packet, size_t len)
{
	ip_udp_packet->ihl              = 5;
	ip_udp_packet->ver              = IPVERSION;
	ip_udp_packet->ecn              = 0;
	ip_udp_packet->dscp             = 0;
	ip_udp_packet->total_len        = htons(len - 14); //Hack this is wrong for non vlan packet
	ip_udp_packet->id               = 0;
	ip_udp_packet->frag_off_flags   = htons(IP_DF);
	ip_udp_packet->ttl              = MAXTTL;
	ip_udp_packet->protocol         = IPPROTO_UDP;
	ip_udp_packet->hdr_csum         = 0;
	ip_udp_packet->src_ip           = htonl(0x0A0A0000UL + options.src_ip);  //eg 10.10.0.x, x = 106
	ip_udp_packet->dst_ip           = htonl(0x0A0A0000UL + options.dst_ip);
	ip_udp_packet->hdr_csum         = wrapsum(checksum(ip_udp_packet, 20, 0));
	ip_udp_packet->src_port         = htons(2000);
	ip_udp_packet->dst_port         = htons(2000);
	ip_udp_packet->udp_len          = htons(len - 14 - 20);
	ip_udp_packet->udp_csum         = 0x0000;

	bzero(&ip_udp_packet->udp_csum + 1,len - 14 - 20);
	*((uint64_t*)(&ip_udp_packet->udp_csum + 1)) = options.init_seq;

}


void prepare_packet(char* iface, eth_ip_udp_head_t* packet, size_t len){
	//Get the source mac address
	uint64_t src_mac;
	source_hwaddr(iface,&src_mac);
	uint64_t dst_mac = options.dst_mac;
	uint64_t dst_mac_be =  (((dst_mac >> 0 * 8) & 0xFF) << (5 * 8)) +
			(((dst_mac >> 1 * 8) & 0xFF) << (4 * 8)) +
			(((dst_mac >> 2 * 8) & 0xFF) << (3 * 8)) +
			(((dst_mac >> 3 * 8) & 0xFF) << (2 * 8)) +
			(((dst_mac >> 4 * 8) & 0xFF) << (1 * 8)) +
			(((dst_mac >> 5 * 8) & 0xFF) << (0 * 8)) ;

	memcpy(packet->unpack.src_mac_raw,&src_mac,6);
	memcpy(packet->unpack.dst_mac_raw,&dst_mac_be,6);
	packet->unpack.eth_type = htons(ETHERTYPE_IP);
	len -= 4; //Make space for an ethernet checksum
	preprare_ip_packet(&packet->unpack.ip_udp_packet,len);

}

void prepare_vlan_packet(char* iface, eth_vlan_ip_udp_head_t* packet, size_t len){
	//Get the source mac address
	uint64_t src_mac;
	source_hwaddr(iface,&src_mac);
	uint64_t dst_mac = options.dst_mac;
	uint64_t dst_mac_be =  (((dst_mac >> 0 * 8) & 0xFF) << (5 * 8)) +
			(((dst_mac >> 1 * 8) & 0xFF) << (4 * 8)) +
			(((dst_mac >> 2 * 8) & 0xFF) << (3 * 8)) +
			(((dst_mac >> 3 * 8) & 0xFF) << (2 * 8)) +
			(((dst_mac >> 4 * 8) & 0xFF) << (1 * 8)) +
			(((dst_mac >> 5 * 8) & 0xFF) << (0 * 8)) ;

	memcpy(packet->unpack.src_mac_raw,&src_mac,6);
	memcpy(packet->unpack.dst_mac_raw,&dst_mac_be,6);
	packet->unpack.vlan_tag.tpid = htons(0x8100);
	//options.vlan_id = 0xDEA;
	//options.vlan_prioirty     = 0x0;
	//options.dei     = 1;
	packet->unpack.vlan_tag.pcp_dei_vid = htons(options.vlan_prioirty << 13| 0 << 12 | options.vlan_id ); //options.vlan_id;
	//packet->unpack.vlan_tag.pcp = 0x0; //options.vlan_prioirty;
	//packet->unpack.vlan_tag.dei = 1;
	packet->unpack.eth_type = htons(ETHERTYPE_IP);
	len -= 4; //Make space for an ethernet checksum
	preprare_ip_packet(&packet->unpack.ip_udp_packet,len - 4);

}



uint64_t pppcount = 0;
uint64_t total_count = 0; 
void update_packet(eth_ip_udp_head_t* packet){
	if(likely(options.use_seq)){
		*((uint64_t*)(&packet->unpack.ip_udp_packet.udp_csum + 1)) = *((uint64_t*)(&packet->unpack.ip_udp_packet.udp_csum + 1)) + 1; //UGH hack :-( pppcount;
	}
}

void update_vlan_packet(eth_vlan_ip_udp_head_t* packet){
	if(likely(options.use_seq)){
		*((uint64_t*)(&packet->unpack.ip_udp_packet.udp_csum + 1)) = *((uint64_t*)(&packet->unpack.ip_udp_packet.udp_csum + 1)) + 1; //pppcount;
	}
}


r2d2_packet go_packet;
struct timeval start, tic, toc, res;
camio_ostream_t* out = NULL;
camio_istream_t* listener = NULL;
camio_istream_t* distaggin = NULL; //Distribute aggregate input listener
size_t packet_len = 0;
void flush_and_close_netmap(){
	if(out){
		gettimeofday(&toc, NULL);
		timersub(&toc, &tic, &res);
		const uint64_t us = res.tv_sec * 1000 * 1000 + res.tv_usec;
		const double pps  = (double)pppcount / (double)us;
		const double gbps  = (double)pppcount * packet_len * 8 / (double)us;
		printf("Status: %08luus, %08lu packets, %lfMpps, %lfMbps\n", us, pppcount, pps, gbps );
		out->delete(out);
	}

}


void term(int signum){
	printf("Terminating...\n");
	flush_and_close_netmap();
	if(listener) { listener->delete(listener); }
	if(distaggin){ distaggin->delete(distaggin); }
	exit(0);
}

static inline void delay(const uint64_t cycles ){
	uint64_t i = 0;
	for(; i < cycles; i++){
		__asm__ __volatile__("nop");
	}
}


static inline void delay2(const uint64_t us)
{
    struct timeval start, now;
    gettimeofday(&start,NULL);
    uint64_t sofar = 0;
    do{
        gettimeofday(&now,NULL);
        sofar = (now.tv_sec - start.tv_sec) * 1000 * 1000 + (now.tv_usec - start.tv_usec);
    }while(sofar < us);
}

static inline double get_delay_estimate(){
	//return 0.8;

	//Spin for 1B cycles to get an idea of our cycles per-second (~bogomips)i
	const uint64_t test_cycles = 100UL * 1000UL * 1000UL; 
	gettimeofday(&tic, NULL);
	delay( test_cycles );
	gettimeofday(&toc, NULL);

	//How fast?
	timersub(&toc, &tic, &res);
	const uint64_t ns = (res.tv_sec * 1000 * 1000 + res.tv_usec) * 1000;
	const double bogoipns = (test_cycles) / (double)ns;

	printf("CPU is spinning at %lf cyles per nanosecond\n", bogoipns );
	return bogoipns;
}




void init_arp_reply() {

	int i = 0;
	for(; i < 6; i++){
		arp_packet.dst_mac[i] = ~0; //Send to the broadcast MAC
		arp_packet.tha[i]     = ~0; //((uint8_t *)&src_mac)[i];
		arp_packet.sha[i]     = ((uint8_t *)&src_mac)[i];
		arp_packet.src_mac[i]  = ((uint8_t *)&src_mac)[i];
	}

	//Set the ethtype field
	arp_packet.eth_type = htons(0x0806);

	arp_packet.htype = htons(1); //Ethernet
	arp_packet.ptype = htons(0x0800); //Ethernet
	arp_packet.plen  = 4;
	arp_packet.hlen  = 6;
	arp_packet.oper  = htons(2); //1 = request, 2 = response
	arp_packet.spa   = htonl(0x0A0A0000UL + options.src_ip);  //eg 10.10.0.x, x = 106
	arp_packet.tpa   = htonl(0x0A0A0000UL + options.src_ip);  //eg 10.10.0.x, x = 106

}




static inline void sync_to_clock(){
	//struct timeval time_now;
	printf("Waiting for PHYs...");
	size_t i = 0;
	for(i = 0; i < 10; i++){
		sleep(1);
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	//Tell the switch that we're here
	printf("Announcing our presence to the switch...");
	fflush(stdout);
	for(i = 0; i < 20; i++){
		init_arp_reply();
		out->assign_write(out,(uint8_t*)&arp_packet,sizeof(arp_packet_t));
		out->end_write(out,sizeof(arp_packet_t));
		out->flush(out);
		printf(".");
		fflush(stdout);
		usleep(100 * 1000);
	}
	out->flush(out);
	printf("\n");


	//    printf("Syncronising...\n");
	//    while(1){
	//        gettimeofday(&time_now, NULL);
	//        const uint64_t ms = (time_now.tv_sec * 1000 * 1000 + time_now.tv_usec) / 1000;
	//        //printf("ms=%lu\n",ms);
	//        if(ms % (2 * 1000) == 0){
	//            break;
	//        }
	//    }

	printf("Done...\n");

}


static int rand_fd = -1;
double get_random(double range ){
	uint32_t rand = 0;
	const uint32_t max = ~0;

	int bytes = sizeof(rand);
	while(bytes > 0){
		bytes -= read(rand_fd, &rand + (sizeof(rand) -bytes), bytes);
	}

	//HACK!
	//if(!options.rdelay){
	rand = ~0;
	//}
	//printf("< Rand=%lf range=%lu, rand=%lu>", (double)rand / max, range, (range * rand) / max);
	return (range * rand) / (double)max;

}

//blatantly hijack the R2D2 control message system
//if we see one, return 0, otherwise return 1
static inline uint64_t read_distagg(){

	uint8_t* da_data;

	//size_t r2d2_len =
	distaggin->start_read(distaggin,&da_data);

	const r2d2_packet* r2d2_control = (r2d2_packet*)da_data;

	//printf("Read message of len=%lu type=0x%04x\n", r2d2_len, ntohs(r2d2_control->ether_type) );
	if(r2d2_control->ether_type == htons(0xFEED)){
		distaggin->end_read(distaggin,NULL);
		return r2d2_control->seq;
	}

	distaggin->end_read(distaggin,NULL);
	return 0;
}


void init_go_packet() {

	int i = 0;
	for(; i < 6; i++){
		go_packet.dst_mac[i] = ~0; //Send to the broadcast MAC
		go_packet.src_mac[i] = ((uint8_t *)&src_mac)[i];
	}

	/*    go_packet.dst_mac[0] = 0x90;
    go_packet.dst_mac[1] = 0xe2;
    go_packet.dst_mac[2] = 0xba;
    go_packet.dst_mac[3] = 0x27;
    go_packet.dst_mac[4] = 0xfb;
    go_packet.dst_mac[5] = 0xc9;*/


	//Set the ethtype field
	go_packet.ether_type = htons(0xFEED);
	go_packet.host_id    = 0;
	go_packet.seq        = 0;

}


double delay_total = 0;
double delay_min = DBL_MAX;
double delay_max = -DBL_MAX;

int main(int argc, char** argv){

	signal(SIGTERM, term);
	signal(SIGINT, term);

	camio_options_short_description("packet_gen");
	camio_options_add(CAMIO_OPTION_REQUIRED, 'i', "interface", "Interface name to generate packets on eg eth0", CAMIO_STRING, &options.iface, "");
	camio_options_add(CAMIO_OPTION_REQUIRED, 's', "src",       "Source trailing IP number eg 106 for 10.10.0.106 ", CAMIO_UINT64, &options.src_ip, 0);
	camio_options_add(CAMIO_OPTION_REQUIRED, 'm', "mac",       "Destination MAC address number as a hex string eg 0x90E2BA27FBE0", CAMIO_UINT64, &options.dst_mac, 0);
	camio_options_add(CAMIO_OPTION_REQUIRED, 'd', "dst",       "Destination trailing IP number eg 112 for 10.10.0.102", CAMIO_UINT64, &options.dst_ip, 0);
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'n', "num-pkts",  "Number of packets to send before stopping. -1=inf [-1]", CAMIO_INT64, &options.num, -1);
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'I', "init-seq",  "Initial sequence number to use [0]", CAMIO_UINT64, &options.init_seq, 0);
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'L', "listener",  "Description of a command lister eg udp:192.168.0.1:2000 [NULL]", CAMIO_STRING, &options.listener, "" );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'u', "use-seq",   "Use sequence numbers in packets [true]", CAMIO_BOOL, &options.use_seq, 1);

	camio_options_add(CAMIO_OPTION_OPTIONAL, 'o', "offset",    "How long in microseconds to sleep before beginning to send", CAMIO_UINT64, &options.offset, 0 );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 't', "timeout",   "time to run for [60s]", CAMIO_UINT64, &options.timeout, 3* 60 * 1000 * 1000 * 1000ULL );

	camio_options_add(CAMIO_OPTION_OPTIONAL, 'w', "wait",      "How long burst window is in nanos minimum of 5000ns, maximum of 70secs [5]", CAMIO_UINT64, &options.burst_window, 5 );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'b', "burst",     "How many packets to send in each burst [1]", CAMIO_UINT64, &options.burst, 1ULL );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'l', "length",    "Length of the entire packet in bytes [64]", CAMIO_UINT64, &options.len, 64 );
	//camio_options_add(CAMIO_OPTION_FLAG,     'r', "rdelay",    "Randomly delay sending inside of the burst window [true]", CAMIO_BOOL, &options.rdelay, 1 );

	camio_options_add(CAMIO_OPTION_OPTIONAL, 'V', "vlan-id",   "VLAN ID, if you wish to generate 802.1Q VLAN tagged packets, -1 if not desired. Must be in the rage of 1-4094 [-1]", CAMIO_INT64, &options.vlan_id, -1LL );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'P', "vlan-pri",  "VLAN priority. Priority if you are generating VLAN tagged packets. Must be in the range 0 - 7 [0]", CAMIO_INT64, &options.vlan_prioirty, 0 );
	camio_options_add(CAMIO_OPTION_OPTIONAL, 'N', "pri-pkts",  "Every N packets a high priority packet will be issued [1], all others are low", CAMIO_INT64, &options.prio_packet, 0LL );

	camio_options_add(CAMIO_OPTION_FLAG,     'C', "distagg-client",  "Run in distribute aggregate in client mode", CAMIO_BOOL, &options.distagg_client, 0 );
	camio_options_add(CAMIO_OPTION_FLAG,     'D', "distagg-server",  "Run in distribute aggregate in server mode", CAMIO_BOOL, &options.distagg_server, 0 );


	camio_options_long_description("Generates packets using netmap with random delays and other features.");
	camio_options_parse(argc, argv);
	source_hwaddr(options.iface,&src_mac);


	if(options.burst_window < 5){
		//eprintf_exit_simple("Minimum burst window size is 5us, you suppled %lu\n", options.burst_window);
	}


//	rand_fd = open("/dev/urandom",O_RDONLY);
//	if(rand_fd < 0){
//		eprintf_exit_simple("Could not open random number generator!\n");
//	}



	//Ready the output stream
	char nm_str[256];
	sprintf(nm_str,"nmap:%s",options.iface);
	camio_ostream_netmap_params_t params = {
			.nm_mem = NULL,
			.nm_mem_size = 0,
			.burst_size = options.burst,
	};
	out = camio_ostream_new(nm_str, &params);

	if(options.distagg_client && options.distagg_server){
		eprintf_exit_simple("Error, cannot be both server and client in distribute aggregate mode\n");
	}

	//Listen for distribute/aggregate input messages
	if(options.distagg_client || options.distagg_server){
		camio_ostream_netmap_t* priv = out->priv;
		camio_istream_netmap_params_t i_params = {
				.nm_mem       = priv->nm_mem,
				.nm_mem_size  = priv->mem_size,
				.fd           = out->fd,
				.nm_offset    = priv->nm_offset,
				.nm_ringid    = priv->nm_ringid,
				.nm_tx_rings  = priv->nm_tx_rings,
				.nm_tx_slots  = priv->nm_tx_slots,
				.nm_rx_rings  = priv->nm_rx_rings,
				.nm_rx_slots  = priv->nm_rx_slots
		};
		distaggin = camio_istream_new(nm_str,&i_params);
	}

	if(options.distagg_server){
		init_go_packet();
	}


	//Prepare an intial packet
	uint8_t pbuff[2 * 1024];
	const size_t buff_len = 2 * 1024; //2kB
	eth_ip_udp_head_t* packet = (eth_ip_udp_head_t*)pbuff;
	eth_vlan_ip_udp_head_t* vlan_packet = (eth_vlan_ip_udp_head_t*)pbuff;
	bzero(pbuff, buff_len);
	packet_len = options.len;
	packet_len = packet_len < 60 ? 60 : packet_len;
	packet_len = packet_len > 1514 ? 1514 : packet_len;
	prepare_packet(options.iface, packet,packet_len);
	if(options.vlan_id > 0){
		printf("Using VLAN mode with VLAN ID=0x%04lx and Priority=%li\n", options.vlan_id, options.vlan_prioirty);
		prepare_vlan_packet(options.iface,vlan_packet,packet_len);
	}


	//Figure out the delay parameters
	double ipns = 0.8; //Instructions per nano second
	ipns = get_delay_estimate();
	ipns = get_delay_estimate();

	printf("Warning fixing ipns to 0.48\n");
	ipns = 0.466;
	sync_to_clock();

	const size_t burst_window = llround(ipns * options.burst_window);
	printf("Burst window delaying for %lu cyles per burst (%lu nanos x %lf cycles/ns)\n", burst_window, options.burst_window, ipns);

	pppcount = 0;



	printf("\nNow generating %lu packets with len=%luB in burst size=%lu, burst window=%lu, offset delay=%lu\n", options.num, packet_len, options.burst, burst_window, options.offset );


	//Apply the offset
	if(options.offset){
		usleep(options.offset);
	}

	size_t bytes = 0;

	//Assume 10bits per byte, x 0.1 bytes per nano (10G) gives nanos for transmission x1000 = micro seconds
	int64_t burst_delay_range_ns = ( ((int)options.burst_window) -  ((int)options.len /* * 10 * 0.1 */ )) - 5;
	burst_delay_range_ns = MAX(burst_delay_range_ns,0);
	printf("Max burst delay = %luns\n", burst_delay_range_ns);
	//Rock and roll,fast path begins here
	gettimeofday(&tic, NULL);
	gettimeofday(&start, NULL);
	//int64_t prev = 0;
	while(1){
		if(options.distagg_client){
			//printf("Waiting for DA message...\n");
			uint64_t time = 0;
			while( !(time = read_distagg())  ); //Spin waiting for a dist-agg control message

			//printf("Found DA message with timestamp=%lu\n", time);
			//Modify the reply now.
			uint64_t* magic = ((uint64_t*)(&vlan_packet->unpack.ip_udp_packet.udp_csum + 1));
			magic += 1;
			uint64_t* ts    = magic + 2;

			*magic = 0xFEEDCAFEDEADCAFEULL;
			*ts = time;


			//struct timeval time_now = {0};
			//gettimeofday(&time_now, NULL);
			//uint64_t mtime = time_now.tv_sec * 1000 * 1000 + time_now.tv_usec;

			//printf("my time=%lu, packet time=%lu, delta=%li gamma=%li\n", mtime , time / 1000, mtime - time / 1000, (mtime - time / 1000) - prev);
			//prev = (mtime - time / 1000);

			//sleep(1);
		}


		//Delay some random amount
		const double rand_burst_delay_ns = burst_delay_range_ns; //HACK XXX TURNS THIS OFF - get_random(burst_delay_range_ns);
		const double cyles = rand_burst_delay_ns * ipns;
		if(options.burst_window < 500 * 1000){
	        delay((uint64_t)round(cyles));
	        //Keep some stats to make sure this is all working
	        delay_max = 0; //rand_burst_delay_ns > delay_max ? rand_burst_delay_ns : delay_max;
	        delay_min = 0; //rand_burst_delay_ns < delay_min ? rand_burst_delay_ns : delay_min;
	        delay_total += rand_burst_delay_ns;
		}
		else{
		    delay2(options.burst_window / 1000 );
		    delay_total += options.burst_window;
            delay_max = 0; //rand_burst_delay_ns > delay_max ? rand_burst_delay_ns : delay_max;
            delay_min = 0; //rand_burst_delay_ns < delay_min ? rand_burst_delay_ns : delay_min;
		}


		if(unlikely(pppcount && pppcount % (1 * 1000  * 1000) == 0)){


			gettimeofday(&toc, NULL);
			timersub(&toc, &tic, &res);
			const uint64_t stats_keeping_us = res.tv_sec * 1000 * 1000 + res.tv_usec;
			const double pps  = (double)pppcount / (double)stats_keeping_us;
			const double gbps  = (double)bytes * 8 / (double)stats_keeping_us;
			timersub(&toc, &start, &res);
			const uint64_t runtime = res.tv_sec * 1000 * 1000 + res.tv_usec;
			printf("Status: %08luus, (%08luus), %08lfus/packet, %08lfus/burst, %08lu packets, %lfMpps, %lfMbps, last delay %lfns, (Total %lf ns/%lu packets), [%lfns,%lfns,%lfns]\n",
			        runtime,
			        stats_keeping_us,
			        (double)stats_keeping_us / pppcount,
			        (double)stats_keeping_us / (pppcount / options.burst),
			        total_count,
			        pps,
			        gbps,
			        rand_burst_delay_ns,
			        delay_total,
			        pppcount,
			        delay_min,
			        delay_total / pppcount,
			        delay_max );
			pppcount = 0;
			bytes    = 0;
			delay_max = -DBL_MAX;
			delay_min = DBL_MAX;
			delay_total = 0;
			gettimeofday(&tic, NULL);
			if(options.timeout && (runtime > options.timeout)){
				printf("Time limit exceeded.\n");
				exit(0);
				break;
			}
		}

		if(options.distagg_server ){
			while(1){
				const uint64_t waittime_ns = 1000 * 1000; //100ms

				struct timeval time_now = {0};
				gettimeofday(&time_now, NULL);
				uint64_t time_now_ns   = time_now.tv_sec * 1000 * 1000 * 1000 + time_now.tv_usec * 1000;
				const uint64_t timefinish_ns = time_now_ns + waittime_ns;
				uint8_t* da_data;

				//printf("%lu - Sending distribute/aggregate broadcast message\n", time_now_ns);

				go_packet.seq = time_now_ns; //Use nano-sec precision on a micro-sec message that that we can have up to 1000 responses without overflow
				out->assign_write(out, (uint8_t*)&go_packet, sizeof(go_packet));
				out->end_write(out, sizeof(go_packet));
				out->flush(out); //We're done so send this immediately


				//Wait up to 100ms for responses to the broadcast message
				while(time_now_ns < timefinish_ns){
					if(distaggin->ready(distaggin)){
						//size_t r2d2_len =
						distaggin->start_read(distaggin,&da_data);
						//printf("Read message of size %lu\n", r2d2_len);

						const eth_ip_udp_head_t* rec_packet = (eth_ip_udp_head_t*)da_data;
						if(rec_packet->unpack.eth_type == htons(ETHERTYPE_IP)) {
							uint64_t* magic = ((uint64_t*)(&rec_packet->unpack.ip_udp_packet.udp_csum + 1));
							magic += 1;
							//uint64_t* timesent = magic + 2;
							if( *magic != 0xFEEDCAFEDEADCAFEULL ){
								printf("Bad magic=0x%16lx\n", *magic);
								continue;
							}

							//printf("Time now=%lu, time_fin=%lu, ns=%lu\n", time_now_ns, timefinish_ns, timefinish_ns - time_now_ns);
							gettimeofday(&time_now, NULL);
							time_now_ns   = time_now.tv_sec * 1000 * 1000 * 1000 + time_now.tv_usec * 1000;

							//printf("%lu %i ",
							//        time_now_ns,
							//rec_packet->unpack.ip_udp_packet.src_ip_raw[0],
							//rec_packet->unpack.ip_udp_packet.src_ip_raw[1],
							//rec_packet->unpack.ip_udp_packet.src_ip_raw[2],
							//        rec_packet->unpack.ip_udp_packet.src_ip_raw[3]);

							//printf( "%lius\n", (time_now_ns - *timesent) / 1000);
							//uint64_t timesent = *((uint64_t*)(&rec_packet->unpack.ip_udp_packet.udp_csum + 1));
							//printf( "delay=%lius\n", (time_now_ns - timesent) / 1000);
						}

						distaggin->end_read(distaggin,NULL);

					}

					//update the time
					gettimeofday(&time_now, NULL);
					time_now_ns   = time_now.tv_sec * 1000 * 1000 * 1000 + time_now.tv_usec * 1000;
				}

			}
		}
		else{

			int i = 0;
			for(i = 0; i < options.burst; i++){
				if(options.vlan_id > 0){
					if(options.prio_packet){
//						if(total_count % options.prio_packet == 0){
							vlan_packet->unpack.vlan_tag.pcp_dei_vid = htons(options.vlan_prioirty << 13 | (0xFFF & options.vlan_id) );
//						}
//						else{
//							vlan_packet->unpack.vlan_tag.pcp_dei_vid = htons(0 << 13 | (0xFFF & options.vlan_id) );
//							vlan_packet->unpack.ip_udp_packet.dst_port = htons(2000);
//							vlan_packet->unpack.ip_udp_packet.src_port = htons(2000);
//						}
					}

					out->assign_write(out,(uint8_t*)vlan_packet, buff_len);
				}
				else{
					out->assign_write(out,(uint8_t*)packet, buff_len);
				}
				//printf("packet_len=%lu\n",packet_len);
				out->end_write(out,packet_len);

				bytes += packet_len;
				pppcount++;
				total_count++;
				if(options.vlan_id > 0){
					update_vlan_packet(vlan_packet);
				}
				else{
					update_packet(packet);
				}

			}

			out->flush(out);
		}


		if(unlikely(options.num != -1 && pppcount >= options.num)){

			printf("Done outputting %lu packets\n", pppcount);
			break;
		}

	}


	term(0);

	//Unreachable
	return 0;
}

