/* 
 * Author: Rui Zhang and Nandita Dukkipati
 * This file will implement the RCP router.
 * Part of the code is taken from Dina Katabi's XCP implementation.
 */

/* rcp.cc.v4
 * --------
 * R = R * { 1 + (T/rtt)*[alpha*(C - input_tr/T) - beta*Q/rtt]/link_capacity};
 * where the input_tr is measured over a time interval of 'T'
 */

#include <math.h>

#include "red.h"
#include "drop-tail.h"
#include "tcp.h"
#include "random.h"
#include "ip.h"
#include <string.h>
#include "rcp-host.h"

// #define ALPHA 0.4
// #define BETA 0.4
#define PHI 0.95
#define PARENT DropTail
// #define INIT_RATE_FACT 0.05 
#define RTT 0.2
#define INIT_NUMFLOWS 50 
// #define TIMESLOT 0.01

#define RTT_GAIN 0.02 //Should be on the order of 1/(number of co-existing flows)

static unsigned int next_router = 0; //Rui: really should be next_queue

class RCPQueue;


class RCPQTimer : public TimerHandler { 
public:
  RCPQTimer(RCPQueue *a, void (RCPQueue::*call_back)() ) : a_(a), call_back_(call_back) {};
protected:
  virtual void expire (Event *e);
  void (RCPQueue::*call_back_)();
  RCPQueue *a_;
}; 



class RCPQueue : public PARENT {
  friend class RCPQTimer;
public:
  RCPQueue();
  int command(int argc, const char*const* argv);
  void queue_timeout();    // timeout every Tq_ for load stats update
protected:
  // Modified functions
  virtual void enque(Packet* pkt);
  virtual Packet* deque();

  // ------------ Utility Functions
  inline double now()  { return  Scheduler::instance().clock(); }

  double running_avg(double var_sample, double var_last_avg, double gain);
  inline double max(double d1, double d2){ if (d1 > d2){return d1;} else {return d2;} }
  inline double min(double d1, double d2){ if (d1 < d2){return d1;} else {return d2;} }

  // ------------- Estimation & Control Helpers
  void init_vars() {
    routerId_ = next_router++; // Rui: should be queueId_ instead
    link_capacity_ = -1;
    input_traffic_ = 0.0;
    act_input_traffic_ = 0.0;
    output_traffic_ = 0.0;
    last_load_ = 0;
    traffic_spill_ = 0;
    num_flows_ = INIT_NUMFLOWS;
    avg_rtt_ = RTT;
    this_Tq_rtt_sum_ = 0;
    this_Tq_rtt_     = 0;
    this_Tq_rtt_numPkts_ = 0;
    input_traffic_rtt_   = 0;
    rtt_moving_gain_ = RTT_GAIN;
//    Tq_ = RTT;
//    Tq_ = min(RTT, TIMESLOT);
    Q_ = 0;
    Q_last = 0;
  }

  virtual void do_on_packet_arrival(Packet* pkt); // called in enque(), but packet may be dropped
                                                  // used for updating the estimation helping vars 
                                                  // such as counting the offered_load_
  virtual void do_before_packet_departure(Packet* p); // called in deque(), before packet leaves
                                                      // used for writing the feedback in the packet
  virtual void fill_in_feedback(Packet* p); // called in do_before_packet_departure()
                                            // used for writing the feedback in the packet

  inline double packet_time(Packet* pkt);

  //int RCPQueue::timeslot(double time);


 /* ------------ Variables ----------------- */
  unsigned int    routerId_;
  RCPQTimer        queue_timer_;
  double          Tq_;

  // Rui: link_capacity_ is set by tcl script.
  // Rui: must call set-link-capacity after building topology and before running simulation
  double link_capacity_;
  double input_traffic_;       // traffic in Tq_
  double act_input_traffic_;
  double output_traffic_;
  double traffic_spill_;  // parts of packets that should fall in next slot
  double last_load_; 
  double end_slot_; // end time of the current time slot
  int num_flows_;
  double avg_rtt_;
  double this_Tq_rtt_sum_;
  double this_Tq_rtt_;
  double this_Tq_rtt_numPkts_;
  int input_traffic_rtt_;
  double rtt_moving_gain_;
  int Q_;
  int Q_last;
  double flow_rate_;
  double alpha_;  // Masayoshi
  double beta_;   // Masayoshi
  double gamma_;
  double min_pprtt_;   // Masayoshi minimum packet per rtt
  double init_rate_fact_;    // Masayoshi
  int    print_status_;      // Masayoshi
  int    rate_fact_mode_;    // Masayoshi
  double fixed_rate_fact_;   // Masayoshi
  double propag_rtt_ ;       // Masayoshi (experimental, used with rate_fact_mode_ = 3)
  double upd_timeslot_ ;       // Masayoshi 

  Tcl_Channel   channel_;      // Masayoshi

};


/*--------------------- Code -------------------------------------*/

static class RCPClass : public TclClass {
public:
  RCPClass() : TclClass("Queue/DropTail/RCP") {}
  TclObject* create(int, const char*const*) {
    return (new RCPQueue);
  }
} class_rcp_queue;


RCPQueue::RCPQueue(): PARENT(), queue_timer_(this, &RCPQueue::queue_timeout),
		      channel_(NULL)
{
  double T;
  init_vars();

  //bind("Tq_", &timeout_);

  bind("alpha_", &alpha_);  // Masayoshi
  bind("beta_", &beta_);    // Masayoshi
  bind("gamma_", &gamma_); 
  bind("min_pprtt_", &min_pprtt_);    // Masayoshi
  bind("init_rate_fact_", &init_rate_fact_);    // Masayoshi
  bind("print_status_", &print_status_);    // Masayoshi
  bind("rate_fact_mode_", &rate_fact_mode_);    // Masayoshi
  bind("fixed_rate_fact_", &fixed_rate_fact_);    // Masayoshi
  bind("propag_rtt_", &propag_rtt_);    // Masayoshi
  bind("upd_timeslot_", &upd_timeslot_);    // Masayoshi
  Tq_ = min(RTT, upd_timeslot_);  // Tq_ has to be initialized  after binding of upd_timeslot_ 
  //
  // fprintf(stdout,"LOG-RCPQueue: alpha_ %f beta_ %f\n",alpha_,beta_);

  // Scheduling queue_timer randommly so that routers are not synchronized
  T = Random::normal(Tq_, 0.2*Tq_);
  //if (T < 0.004) { T = 0.004; } // Not sure why Dina did this...

  end_slot_ = T;
  queue_timer_.sched(T);
}


Packet* RCPQueue::deque()
{
	Packet *p;

	p = PARENT::deque();
	if (p != NULL)
	  do_before_packet_departure(p);
	return (p);
}

void RCPQueue::enque(Packet* pkt)
{
  do_on_packet_arrival(pkt);

  PARENT::enque(pkt);
}

void RCPQueue::do_on_packet_arrival(Packet* pkt){
  // Taking input traffic statistics
  int size = hdr_cmn::access(pkt)->size();
  hdr_rcp * hdr = hdr_rcp::access(pkt);
  double pkt_time_ = packet_time(pkt);
  double end_time = now() + pkt_time_;
  double part1, part2;

  // update avg_rtt_ here
  double this_rtt = hdr->rtt();

  if (this_rtt > 0) {

       this_Tq_rtt_sum_ += (this_rtt * size);
       input_traffic_rtt_ += size;
       this_Tq_rtt_ = running_avg(this_rtt, this_Tq_rtt_, flow_rate_/link_capacity_);    

//     rtt_moving_gain_ = flow_rate_/link_capacity_;
//     avg_rtt_ = running_avg(this_rtt, avg_rtt_, rtt_moving_gain_);
  }

  if (end_time <= end_slot_)
    act_input_traffic_ += size;
  else {
    part2 = size * (end_time - end_slot_)/pkt_time_;
    part1 = size - part2;
   act_input_traffic_ += part1;
    traffic_spill_ += part2;
  }

  // Can do some measurement of queue length here
  // length() in packets and byteLength() in bytes

  /* Can read the flow size from a last packet here */
}


void RCPQueue::do_before_packet_departure(Packet* p){
  hdr_rcp * hdr = hdr_rcp::access(p);
  int size = hdr_cmn::access(p)->size();
  output_traffic_ += size;  
  
  if ( hdr->RCP_pkt_type() == RCP_SYN )
  {
//	  num_flows_++;
	  fill_in_feedback(p);
  }
  else if (hdr->RCP_pkt_type() == RCP_FIN )
  {
//	  num_flows_--;
  }
  else if ( hdr->RCP_pkt_type() == RCP_REF || hdr->RCP_pkt_type() == RCP_DATA )
  {
	  fill_in_feedback(p);
  }
		  
}


void RCPQueue::queue_timeout()
{
  double temp;
  double datarate_fact;
  double estN1;
  double estN2;
  int Q_pkts;
  char clip;
  int Q_target_;

  double ratio;
  double input_traffic_devider_;
  double queueing_delay_;

  double virtual_link_capacity; // bytes per second
  
  last_load_ = act_input_traffic_/Tq_; // bytes per second

  Q_ = byteLength();
  Q_pkts = length();
 
  input_traffic_ = last_load_;
  if (input_traffic_rtt_ > 0)
    this_Tq_rtt_numPkts_ = this_Tq_rtt_sum_/input_traffic_rtt_; 

  /*
  if (this_Tq_rtt_numPkts_ >= avg_rtt_)
      rtt_moving_gain_ = (flow_rate_/link_capacity_);
  else 
      rtt_moving_gain_ = (flow_rate_/link_capacity_)*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);
   */
   if (this_Tq_rtt_numPkts_ >= avg_rtt_)
        rtt_moving_gain_ = (Tq_/avg_rtt_);
   else 
        rtt_moving_gain_ = (flow_rate_/link_capacity_)*(this_Tq_rtt_numPkts_/avg_rtt_)*(Tq_/avg_rtt_);

  avg_rtt_ = running_avg(this_Tq_rtt_numPkts_, avg_rtt_, rtt_moving_gain_);
 
//  if (Q_ == 0)
//	  input_traffic_ = PHI*link_capacity_;
//  else
//	 input_traffic_ = link_capacity_ + (Q_ - Q_last)/Tq_; 

//  queueing_delay_ = (Q_ ) / (link_capacity_ );
//  if ( avg_rtt_ > queueing_delay_ ){
//    propag_rtt_ = avg_rtt_ - queueing_delay_;
//  } else {
//    propag_rtt_ = avg_rtt_;
//  } 


  estN1 = input_traffic_ / flow_rate_;
  estN2 = link_capacity_ / flow_rate_;

  if ( rate_fact_mode_ == 0) { // Masayoshi .. for Nandita's RCP

   virtual_link_capacity = gamma_ * link_capacity_;

    /* Estimate # of active flows with  estN2 = (link_capacity_/flow_rate_) */
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(virtual_link_capacity - input_traffic_) - beta_*(Q_/avg_rtt_)))/virtual_link_capacity);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 1) { // Masayoshi .. for fixed rate fact
    /* Fixed Rate Mode */
    temp = link_capacity_ * fixed_rate_fact_;

  } else if ( rate_fact_mode_ == 2) { 

    /* Estimate # of active flows with  estN1 = (input_traffic_/flow_rate_) */

    if (input_traffic_ == 0.0 ){
      input_traffic_devider_ = link_capacity_/1000000.0;
    } else {
      input_traffic_devider_ = input_traffic_;
    }
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/avg_rtt_)))/input_traffic_devider_);
    temp = flow_rate_ * ratio;

  } else if ( rate_fact_mode_ == 3) { 
    //if (input_traffic_ == 0.0 ){
    //input_traffic_devider_ = link_capacity_/1000000.0;
    //    } else {
    //input_traffic_devider_ = input_traffic_;
    //}
    ratio =  (1 + ((Tq_/propag_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/propag_rtt_)))/link_capacity_);
    temp = flow_rate_ * ratio;
  } else  if ( rate_fact_mode_ == 4) { // Masayoshi .. Experimental
    ratio = (1 + ((Tq_/avg_rtt_)*(alpha_*(link_capacity_ - input_traffic_) - beta_*(Q_/avg_rtt_)))/link_capacity_);
    temp = flow_rate_ * ratio;
    // link_capacity_ : byte/sec
  } else  if ( rate_fact_mode_ == 5) {
    // temp = - link_capacity_ * ( Q_/(link_capacity_* avg_rtt_) - 1.0);
    //temp = flow_rate_ +  link_capacity_ * (alpha_ * (1.0 - input_traffic_/link_capacity_) - beta_ * ( Q_/(avg_rtt_*link_capacity_) )) * Tq_;
    temp = link_capacity_ * exp ( - Q_/(link_capacity_* avg_rtt_));
  } else  if ( rate_fact_mode_ == 6) {
    temp = link_capacity_ * exp ( - Q_/(link_capacity_* propag_rtt_));
  } else  if ( rate_fact_mode_ == 7) {
     temp = - link_capacity_ * ( Q_/(link_capacity_* propag_rtt_) - 1.0);
  } else  if ( rate_fact_mode_ == 8) {
    temp = flow_rate_ +  link_capacity_ * (alpha_ * (1.0 - input_traffic_/link_capacity_) - beta_/avg_rtt_ * ( Q_/(propag_rtt_*link_capacity_) - 0.8 )) * Tq_;
  }


  if ( rate_fact_mode_ != 4) { // Masayoshi .. Experimental
    if (temp < min_pprtt_ * (1000/avg_rtt_) ){     // Masayoshi
      flow_rate_ = min_pprtt_ * (1000/avg_rtt_) ; // min pkt per rtt 
      clip  = 'L';
    } else if (temp > virtual_link_capacity){
      flow_rate_ = virtual_link_capacity;
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
  }else{
    if (temp < 16000.0 ){    // Masayoshi 16 KB/sec = 128 Kbps
      flow_rate_ = 16000.0;
      clip  = 'L';
    } else if (temp > link_capacity_){
      flow_rate_ = link_capacity_;
      clip = 'U';
    } else {
      flow_rate_ = temp;
      clip = 'M';
    }
  }


//  else if (temp < 0 )  
//	 flow_rate_ = 1000/avg_rtt_; // 1 pkt per rtt 

  datarate_fact = flow_rate_/link_capacity_;

//   if (print_status_ == 1) 
//   if (routerId_ == 0) 
//      	   fprintf(stdout, "%f %d %f %f %f\n", now(), length(), datarate_fact, output_traffic_/(link_capacity_*Tq_), avg_rtt_);
//	fprintf(stdout, "%s %f %d %d %f %f %f\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_);
     //	fprintf(stdout, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(link_capacity_ - input_traffic_)/link_capacity_, (Q_/propag_rtt_)/link_capacity_,ratio,estN1,estN2,clip);

     if(channel_ != NULL){
       char buf[2048];
       sprintf(buf, "%s %f %d %d %.10lf %f %f %f %f %f %f %f %c\n", this->name(),now(), byteLength(), Q_pkts, datarate_fact, last_load_, avg_rtt_,(link_capacity_ - input_traffic_)/link_capacity_, (Q_/avg_rtt_)/link_capacity_,ratio,estN1,estN2,clip);
	   // sprintf(buf, "%f %d %.5lf %f %f %f %f %f %c\n", now(), byteLength(), datarate_fact, avg_rtt_, this_Tq_rtt_, this_Tq_rtt_numPkts_, last_load_, (link_capacity_ - input_traffic_)/link_capacity_, clip);
//       sprintf(buf, "%f r_ %f estN1_ %f estN2_ %f \n", now(), datarate_fact, estN1, estN2); 
	   (void)Tcl_Write(channel_, buf, strlen(buf));
     }

// fflush(stdout);

  Tq_ = min(avg_rtt_, upd_timeslot_);
  this_Tq_rtt_ = 0;
  this_Tq_rtt_sum_ = 0;
  input_traffic_rtt_ = 0;
  Q_last = Q_;
  act_input_traffic_ = traffic_spill_;
  traffic_spill_ = 0;  
  output_traffic_ = 0.0;
  end_slot_ = now() + Tq_;
  queue_timer_.resched(Tq_);
}


/* Rui: current scheme:  */
void RCPQueue::fill_in_feedback(Packet* p){

  hdr_rcp * hdr = hdr_rcp::access(p);
  double request = hdr->RCP_request_rate();
  
  // update avg_rtt_ here
  // double this_rtt = hdr->rtt();
  
  /*
  if (this_rtt > 0) {
    avg_rtt_ = running_avg(this_rtt, avg_rtt_, RTT_GAIN);
  }
  */

  if (request < 0 || request > flow_rate_)
    hdr->set_RCP_rate(flow_rate_);
}

int RCPQueue::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

  if (argc == 2) {
    ;
  }
  else if (argc == 3) {
    if (strcmp(argv[1], "set-link-capacity") == 0) {
      link_capacity_ = strtod(argv[2],0);
      if (link_capacity_ < 0.0) {printf("Error: BW < 0\n"); abort();}
      // Rui: Link capacity is in bytes.
      flow_rate_ = link_capacity_ * init_rate_fact_;

      if ( rate_fact_mode_ == 1) { // Masayoshi
	flow_rate_ = link_capacity_ * fixed_rate_fact_;
      }

      // Initializing the PARENT 
#ifdef RED_PARENT
      edp_.th_min = 0.4 * limit();		    // minthresh
      edp_.th_max = 0.8 * limit();	            // maxthresh
      edp_.q_w = 0.01 / limit();		    // for EWMA
      edp_.max_p_inv = 3;
	      
      // If you have an old version of ns and you are getting a compilation 
      // error then comments the two lines below
      edp_.th_min_pkts = 0.6 * limit();		    // minthresh
      edp_.th_max_pkts = 0.8 * limit();	            // maxthresh

      //printf("RED Dropping Policy, Min %g, Max %g, W %g, inv_p %g \n",
      //edp_.th_min,edp_.th_max,edp_.q_w,edp_.max_p_inv);
#endif   
      return (TCL_OK);

    } else if (strcmp(argv[1], "set-rate-fact-mode") == 0) { // Masayoshi
      rate_fact_mode_ = atoi(argv[2]);
      if (rate_fact_mode_ != 0 && rate_fact_mode_ != 1){
	printf("Error: (rcp) rate_fact_mode_ should be 1 or 0\n"); 
      }
      return (TCL_OK);
    } else if (strcmp(argv[1], "set-fixed-rate-fact") == 0) { // Masayoshi
      fixed_rate_fact_ = strtod(argv[2],0);
      if (fixed_rate_fact_ < 0.0 || fixed_rate_fact_ > 1.0){
	printf("Error: (rcp) fixed_rate_fact_ < 0 or >1.0\n"); 
	abort();
      }
      return (TCL_OK);
    } else if (strcmp(argv[1], "set-flow-rate") == 0) { // Masayoshi
      flow_rate_ = strtod(argv[2],0);
      return (TCL_OK);
    }
    if (strcmp(argv[1], "attach") == 0) {
      int mode;
      const char* id = argv[2];
      channel_ = Tcl_GetChannel(tcl.interp(),
                                (char*) id, &mode);
      if (channel_ == NULL) {
	tcl.resultf("Tagger (%s): can't attach %s for writing",
		    name(), id);
	return (TCL_ERROR);
      }
      return (TCL_OK);
    }
  }
  return PARENT::command(argc, argv);
}


inline double RCPQueue::packet_time(Packet* pkt){
  return (hdr_cmn::access(pkt)->size()/link_capacity_);
}

void RCPQTimer::expire(Event *) { 
  (*a_.*call_back_)();
}

double RCPQueue::running_avg(double var_sample, double var_last_avg, double gain)
{
	double avg;
	if (gain < 0)
	  exit(3);
	avg = gain * var_sample + ( 1 - gain) * var_last_avg;
	return avg;
}
