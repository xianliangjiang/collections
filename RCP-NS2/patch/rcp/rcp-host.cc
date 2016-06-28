/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the MASH Research
 * 	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Research Group may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* rcp-host5.cc
 * -------
 * rtp1.cc: rate is fixed at the beginning and does not change during flow duration.
 * rtp2.cc: rate is reset every round trip time. (Too much probing)
 * rtp3.cc: rate is reset every (RTT + SYN_DELAY). To disable subsequent SYNs, set SYN_DELAY larger than all flow durations.
 * rtp4.cc: SYN(ACK) packets are of different size from data packets. Rate is reset every (RTT + SYN_DELAY).
 * rtp5.cc: Rate is reset every SYN_DELAY time.
 */


//#define DELETE_AGENTS  //xxxxrui

// #define MASAYOSHI_DEBUG 1

#include <stdlib.h>

#include "config.h"
#include "agent.h"
#include "random.h"
#include "rcp-host.h"
#include "ip.h" /* Nandita Rui */

//#define SYN_DELAY 0.05
#define SYN_DELAY 0.5
#define REF_FACT 4  //Number of REFs per RTT
#define RCP_HDR_BYTES 40  //same as TCP for fair comparison
#define QUIT_PROB 0.1  //probability of quitting if assigned rate is zero
#define REF_INTVAL 1.0 //

int hdr_rcp::offset_;

class RCPHeaderClass : public PacketHeaderClass {
public: 
	RCPHeaderClass() : PacketHeaderClass("PacketHeader/RCP", sizeof(hdr_rcp)) {
		bind_offset(&hdr_rcp::offset_);
	}
} class_rcphdr;

static class RCPAgentClass : public TclClass {
public:
	RCPAgentClass() : TclClass("Agent/RCP") {}
	TclObject* create(int, const char*const*) {
		return (new RCPAgent());
	}
} class_rcp_agent;

RCPAgent::RCPAgent() : Agent(PT_RCP), lastpkttime_(-1e6), 
	rcp_timer_(this, &RCPAgent::timeout), num_sent_(0),
	ref_timer_(this, &RCPAgent::ref_timeout), rtt_(0),
	RCP_state(RCP_INACT), numOutRefs_(0),
    min_rtt_(SYN_DELAY*REF_FACT*10), ref_seqno_(0),
    num_dataPkts_acked_by_receiver_(0), num_dataPkts_received_(0),
    num_Pkts_to_retransmit_(0), num_pkts_resent_(0),
    num_enter_retransmit_mode_(0), rto_timer_(this, &RCPAgent::retrans_timeout)
{
	bind("seqno_", &seqno_);
	bind("packetSize_", &size_);
	/* numpkts_ has file size, need not be an integer */
	bind("numpkts_",&numpkts_);
	bind("init_refintv_fix_",&init_refintv_fix_);
    bind("fid_",&fid_);
}


void RCPAgent::reset() /* reset() is added by Masayoshi */
{
	lastpkttime_ = -1e6;
	num_sent_ = 0;
	seqno_ = 0;
	ref_seqno_ = 0;
	rtt_ = 0;
	RCP_state = RCP_INACT;
	numOutRefs_ = 0;
	min_rtt_ = SYN_DELAY * REF_FACT * 10;

    num_dataPkts_acked_by_receiver_ = 0;
    num_dataPkts_received_          = 0;
    num_Pkts_to_retransmit_         = 0;
    num_pkts_resent_                = 0;
    num_enter_retransmit_mode_      = 0;
}

void RCPAgent::start()
{
	double now = Scheduler::instance().clock();

    RCP_state = RCP_RUNNING;
	Tcl::instance().evalf("%s begin-datasend", this->name()); /* Added by Masayoshi */

	// rcp_timer_.resched(interval_); // Masayosi. 
	// // Always start sending after SYN-ACK receive 
	// // is not good when gamma is very very small.

	if( interval_ > REF_INTVAL * min_rtt_ ){  // At this momoent min_rtt_ has been set (by SYNACK)
		ref_timer_.resched(REF_INTVAL * min_rtt_);
		RCP_state = RCP_RUNNING_WREF;
#ifdef MASAYOSHI_DEBUG
		fprintf(stdout,"MASA %lf RCP_RUNNING -> RCP_RUNNING_WREF at start %s\n",now,this->name());
#endif
	} else {
		RCP_state = RCP_RUNNING;
		ref_timer_.force_cancel();
#ifdef MASAYOSHI_DEBUG
		fprintf(stdout,"MASA %lf RCP_RUNNING at start %s\n",now,this->name());
#endif
	}

	// timeout should be later than the above RCP_state change.
	// This is because if numpkts_ is one (1 pkt file transfer),
	// the above state change may harmfully overwrite
	// the change to RCP_FINSENT in timeout().
	timeout();  
}

void RCPAgent::stop()
{
    rcp_timer_.force_cancel();
	ref_timer_.force_cancel();
    rto_timer_.force_cancel();
    finish();
}

void RCPAgent::pause()
{
    rcp_timer_.force_cancel();
	ref_timer_.force_cancel();
    rto_timer_.force_cancel();
	RCP_state = RCP_INACT;
}

/* Nandita: RCP_desired_rate sets the senders initial desired rate
 */
double RCPAgent::RCP_desired_rate()
{
     double RCP_rate_ = -1; 
         return(RCP_rate_);
}

/* Nandita: sendfile() 
 * Sends a SYN packet and enters RCPS_LISTEN state
 * On receiving the SYN_ACK packet (with the rate) from
 * RCP receiver, the recv() function calls start() and
 * packets are sent at the desired rate
 */
void RCPAgent::sendfile()
{
	// Sending SYN packet: 
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);

	// SYN and SYNACK packets should be smaller than data packets.
	hdr_cmn *cmnh = hdr_cmn::access(p);

	cmnh->size_ = RCP_HDR_BYTES;
	
	rh->seqno() = ref_seqno_++; /* Masayoshi */

	rh->ts_ = Scheduler::instance().clock();
	rh->rtt_ = rtt_;
       
	rh->set_RCP_pkt_type(RCP_SYN);
	rh->set_RCP_rate(RCP_desired_rate());

    rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
    rh->flowIden() = fid_;

    fprintf(stdout,"Starting RCP flow fid_ %d \n", fid_);

 	target_->recv(p, (Handler*)0);

	// Change state of sender: Sender is in listening state 
	RCP_state = RCP_SYNSENT;

	ref_timer_.resched(SYN_DELAY);
	lastpkttime_ =  Scheduler::instance().clock();  // Masayoshi

	Tcl::instance().evalf("%s syn-sent", this->name()); /* Added by Masayoshi */
}


/* Nandita Rui
 * This function has been changed.
 */
void RCPAgent::timeout() 
{
	if (RCP_state == RCP_RUNNING || RCP_state == RCP_RUNNING_WREF) {
		if (num_sent_ < numpkts_ - 1) {
			sendpkt();
			rcp_timer_.resched(interval_);

		} else {

			double now = Scheduler::instance().clock();
			sendlast();
			Tcl::instance().evalf("%s finish-datasend", this->name()); /* Added by Masayoshi */
			RCP_state = RCP_FINSENT;
			
#ifdef MASAYOSHI_DEBUG			
			fprintf(stdout,"MASA %lf  .. -> RCP_FINSENT %s\n",now,this->name());
#endif
			rcp_timer_.force_cancel();
			ref_timer_.force_cancel();
            rto_timer_.resched(2*rtt_);
		}

	} else if (RCP_state == RCP_RETRANSMIT) {

          if (num_pkts_resent_ < num_Pkts_to_retransmit_ - 1) {
              sendpkt();
              rcp_timer_.resched(interval_);
          } else if (num_pkts_resent_ == num_Pkts_to_retransmit_ - 1) {
              sendpkt();
              rcp_timer_.force_cancel();
              ref_timer_.force_cancel();
              rto_timer_.resched(2*rtt_);
          }
    }
}

void RCPAgent::retrans_timeout()
{ 
    RCP_state = RCP_RETRANSMIT;
    num_enter_retransmit_mode_++; 
    
    num_pkts_resent_ = 0;
    num_Pkts_to_retransmit_ = numpkts_ - num_dataPkts_acked_by_receiver_;
   // fprintf(stdout, "%lf %s Entered retransmission mode %d, num_Pkts_to_retransmit_ %d \n", Scheduler::instance().clock(), this->name(), num_enter_retransmit_mode_, num_Pkts_to_retransmit_);

    if (num_Pkts_to_retransmit_ > 0)
        rcp_timer_.resched(interval_);
}


/* Rui
 * This function 
 */
void RCPAgent::ref_timeout() 
{
	if (RCP_state==RCP_SYNSENT || RCP_state==RCP_RUNNING || RCP_state==RCP_CONGEST || RCP_state == RCP_RUNNING_WREF){
		Packet* send_p = allocpkt();
		hdr_rcp *rh = hdr_rcp::access(send_p);

		// SYN and SYNACK packets should be smaller than data packets.
		hdr_cmn *cmnh = hdr_cmn::access(send_p);
		cmnh->size_ = RCP_HDR_BYTES;
		// cmnh->size_ = 1;
		
		//		rh->seqno() = ref_seqno_ ++;  /* added by Masayoshi */
		rh->seqno() = seqno_;
		ref_seqno_ ++;  /* added by Masayoshi */
		rh->ts_ = Scheduler::instance().clock();
		rh->rtt_ = rtt_;
	
        if (RCP_state == RCP_SYNSENT) {
            rh->set_RCP_pkt_type(RCP_SYN);
           // fprintf(stdout, "%lf %s Sending SYN packet again... \n", Scheduler::instance().clock(), this->name());
        } else {
		    rh->set_RCP_pkt_type(RCP_REF);
            numOutRefs_++;
        }

		rh->set_RCP_rate(RCP_desired_rate());
        rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        rh->flowIden() = fid_;
		
		target_->recv(send_p, (Handler*)0);

		// ref_timer_.resched(min(SYN_DELAY, min_rtt_/REF_FACT));
		ref_timer_.resched(min(SYN_DELAY, REF_INTVAL * min_rtt_));
	}
}

/*
 * finish() is called when we must stop (either by request or because
 * we're out of packets to send.
 */
void RCPAgent::finish()
{
	double now = Scheduler::instance().clock(); 

	RCP_state = RCP_INACT;
    fid_      = 0; 

#ifdef MASAYOSHI_DEBUG
	fprintf(stdout,"MASA %lf  .. -> RCP_INACT %s\n",now,this->name());
#endif

	Tcl::instance().evalf("%s done", this->name()); /* Added by Masayoshi */

#ifdef DELETE_AGENTS
	Tcl::instance().evalf("delete %s", this->name());
#endif
}


/* Nandita Rui
 * This function has been changed.
 */
void RCPAgent::recv(Packet* p, Handler*)
{
	hdr_rcp* rh = hdr_rcp::access(p);

   if ( (rh->RCP_pkt_type() == RCP_SYN) || ((RCP_state != RCP_INACT) && (rh->flowIden() == fid_)) ) {
//    if ((rh->RCP_pkt_type() == RCP_SYN) || (rh->flowIden() == fid_)) {

	switch (rh->RCP_pkt_type()) {
	case RCP_SYNACK:
		rtt_ = Scheduler::instance().clock() - rh->ts();

		if (min_rtt_ > rtt_)
			min_rtt_ = rtt_;

		if (rh->RCP_request_rate() > 0) {
			double now = Scheduler::instance().clock();
			interval_ = (size_+RCP_HDR_BYTES)/(rh->RCP_request_rate());

#ifdef MASAYOSHI_DEBUG
			fprintf(stdout,"%lf recv_synack..rate_change_1st %s %lf %lf\n",now,this->name(),interval_,((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));
#endif
			
            if (RCP_state == RCP_SYNSENT)
			    start();

		}
		else {
			if (rh->RCP_request_rate() < 0) 
				fprintf(stderr, "Error: RCP rate < 0: %f\n",rh->RCP_request_rate());

			if (Random::uniform(0,1)<QUIT_PROB) { //sender decides to stop
				//				RCP_state = RCP_DONE;
				// RCP_state = RCP_QUITTING; // Masayoshi
				pause();
				double now = Scheduler::instance().clock();
				fprintf(stdout,"LOG: %lf quit by QUIT_PROB\n",now);
			}
			else {
				RCP_state = RCP_CONGEST;
				//can do exponential backoff or probalistic stopping here.
			}

		}
		break;

	case RCP_REFACK:
		//		if ( rh->seqno() < ref_seqno_ && RCP_state != RCP_INACT) /* Added  by Masayoshi */
		// if ( rh->seqno() < seqno_ && RCP_state != RCP_INACT) /* Added  by Masayoshi */
		if ( (rh->seqno() <= seqno_)  && RCP_state != RCP_INACT){ /* Added  by Masayoshi */
			numOutRefs_--;
			if (numOutRefs_ < 0) {
				fprintf(stderr, "Extra REF_ACK received! \n");
				{
					if(RCP_state == RCP_INACT)
						fprintf(stderr,"RCP_INACT\n");
					if(RCP_state == RCP_SYNSENT)
						fprintf(stderr,"RCP_SYNSENT\n");
					if(RCP_state == RCP_RUNNING)
						fprintf(stderr,"RCP_RUNNING\n");
					if(RCP_state == RCP_RUNNING_WREF)
						fprintf(stderr,"RCP_RUNNING_WREF\n");
					if(RCP_state == RCP_CONGEST)
						fprintf(stderr,"RCP_CONGEST\n");
				}
				exit(1);
			}
		
			rtt_ = Scheduler::instance().clock() - rh->ts();
			if (min_rtt_ > rtt_)
				min_rtt_ = rtt_;

			if (rh->RCP_request_rate() > 0) {
				double new_interval = (size_+RCP_HDR_BYTES)/(rh->RCP_request_rate());
				if( new_interval != interval_ ){
					interval_ = new_interval;
					if (RCP_state == RCP_CONGEST)
						start();
					else
						rate_change();
				}
				
			}
			else {
				if (rh->RCP_request_rate() < 0) 
				fprintf(stderr, "Error: RCP rate < 0: %f\n",rh->RCP_request_rate());
				rcp_timer_.force_cancel();
				RCP_state = RCP_CONGEST; //can do exponential backoff or probalistic stopping here.
			}
		}
		break;

	case RCP_ACK:

         num_dataPkts_acked_by_receiver_ = rh->num_dataPkts_received; 
        if (num_dataPkts_acked_by_receiver_ == numpkts_) {
            // fprintf(stdout, "%lf %d RCP_ACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
            stop();
        }

		rtt_ = Scheduler::instance().clock() - rh->ts();
		if (min_rtt_ > rtt_)
			min_rtt_ = rtt_;

		if (rh->RCP_request_rate() > 0) {
			double new_interval = (size_+RCP_HDR_BYTES)/(rh->RCP_request_rate());
			if( new_interval != interval_ ){
				interval_ = new_interval;
				if (RCP_state == RCP_CONGEST)
					start();
				else
					rate_change();
			}
		}
		else {
			fprintf(stderr, "Error: RCP rate < 0: %f\n",rh->RCP_request_rate());
			RCP_state = RCP_CONGEST; //can do exponential backoff or probalistic stopping here.
		}
		break;

	case RCP_FIN:
		{double copy_rate;
        num_dataPkts_received_++; // because RCP_FIN is piggybacked on the last packet of flow
		Packet* send_pkt = allocpkt();
		hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
		hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
		cmnh->size_ = RCP_HDR_BYTES;
		
		copy_rate = rh->RCP_request_rate();
		// Can modify the rate here.
		send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_FINACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();
			
		target_->recv(send_pkt, (Handler*)0);
		Tcl::instance().evalf("%s fin-received", this->name()); /* Added by Masayoshi */
		break;
		}


	case RCP_SYN:
		{double copy_rate;
		
		Packet* send_pkt = allocpkt();
		hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
		hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
		cmnh->size_ = RCP_HDR_BYTES;
		
		copy_rate = rh->RCP_request_rate();
		// Can modify the rate here.
		send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_SYNACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();
			
		target_->recv(send_pkt, (Handler*)0);
        RCP_state = RCP_RUNNING; // Only the receiver changes state here

		break;}

	case RCP_FINACK:
        num_dataPkts_acked_by_receiver_ = rh->num_dataPkts_received;

	    if (num_dataPkts_acked_by_receiver_ == numpkts_){
           // fprintf(stdout, "%lf %d RCP_FINACK: Time to stop \n", Scheduler::instance().clock(), rh->flowIden());
            stop();
        }
		break;

	case RCP_REF:
		{
		double copy_rate;
			
		Packet* send_pkt = allocpkt();
		hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
		hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
		cmnh->size_ = RCP_HDR_BYTES;
		
		copy_rate = rh->RCP_request_rate();
		// Can modify the rate here.
		send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_REFACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();
		
		target_->recv(send_pkt, (Handler*)0);
		break;}
		
	case RCP_DATA:
		{
		double copy_rate;
        num_dataPkts_received_++;
			
		Packet* send_pkt = allocpkt();
		hdr_rcp *send_rh = hdr_rcp::access(send_pkt);
		hdr_cmn *cmnh = hdr_cmn::access(send_pkt);
		cmnh->size_ = RCP_HDR_BYTES;
		
		copy_rate = rh->RCP_request_rate();
		// Can modify the rate here.
		send_rh->seqno() = rh->seqno();
		send_rh->ts() = rh->ts();
		send_rh->rtt() = rh->rtt();
		send_rh->set_RCP_pkt_type(RCP_ACK);
		send_rh->set_RCP_rate(copy_rate);
        send_rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
        send_rh->flowIden() = rh->flowIden();
		
		target_->recv(send_pkt, (Handler*)0);
		break;}

	case RCP_OTHER:
		fprintf(stderr, "received RCP_OTHER\n");
		exit(1);
		break;

	default:
		fprintf(stderr, "Unknown RCP packet type!\n");
		exit(1);
		break;
    }
  }

	Packet::free(p);
}

int RCPAgent::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "rate-change") == 0) {
			rate_change();
			return (TCL_OK);
		} else 
			if (strcmp(argv[1], "start") == 0) {
                        start();
                        return (TCL_OK);
                } else if (strcmp(argv[1], "stop") == 0) {
                        stop();
                        return (TCL_OK);
                } else if (strcmp(argv[1], "pause") == 0) {
                        pause();
                        return (TCL_OK);
		} else if (strcmp(argv[1], "sendfile") == 0) {
			sendfile();
			return(TCL_OK);
		} else if (strcmp(argv[1], "reset") == 0) { /* Masayoshi */
			reset();
			return(TCL_OK);
		}
		
	} 
// 	else if (argc == 3) {
// 		// if (strcmp(argv[1], "session") == 0) {
//  			session_ = (RCPSession*)TclObject::lookup(argv[2]);
//  			return (TCL_OK);
//  		} else 
// 			if (strcmp(argv[1], "advance") == 0) {
//                         int newseq = atoi(argv[2]);
//                         advanceby(newseq - seqno_);
//                         return (TCL_OK); 
//                 } else if (strcmp(argv[1], "advanceby") == 0) {
//                         advanceby(atoi(argv[2]));
//                         return (TCL_OK);
//                 }
// 	}
	return (Agent::command(argc, argv));
}

/* 
 * We modify the rate in this way to get a faster reaction to the a rate
 * change since a rate change from a very low rate to a very fast rate may 
 * take an undesireably long time if we have to wait for timeout at the old
 * rate before we can send at the new (faster) rate.
 */
void RCPAgent::rate_change()
{
	if (RCP_state == RCP_RUNNING || RCP_state == RCP_RUNNING_WREF) {
		rcp_timer_.force_cancel();
		
		double t = lastpkttime_ + interval_;
		
		double now = Scheduler::instance().clock();

		if ( t > now) {
#ifdef MASAYOSHI_DEBUG
			fprintf(stdout,"%lf rate_change %s %lf %lf\n",now,this->name(),interval_,((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));
#endif
			rcp_timer_.resched(t - now);

			if( (t - lastpkttime_) > REF_INTVAL * min_rtt_ && RCP_state != RCP_RUNNING_WREF ){ 
			// the inter-packet time > min_rtt and not in REF mode. Enter REF MODE.
				RCP_state = RCP_RUNNING_WREF;
#ifdef MASAYOSHI_DEBUG
				fprintf(stdout,"MASA %lf RCP_RUNNING -> RCP_RUNNING_WREF at start %s\n",now,this->name());
#endif
				if( lastpkttime_ + REF_INTVAL * min_rtt_ > now ){
					ref_timer_.resched(lastpkttime_ + REF_INTVAL * min_rtt_ - now);
				} else {
					ref_timeout();  // send ref packet now.
				}
			}else if ((t-lastpkttime_)<= REF_INTVAL * min_rtt_ && 
				  RCP_state == RCP_RUNNING_WREF ){ 
			// the inter-packet time <= min_rtt and in REF mode.  Exit REF MODE
				RCP_state = RCP_RUNNING;
#ifdef MASAYOSHI_DEBUG
				fprintf(stdout,"MASA %lf RCP_RUNNING_WREF -> RCP_RUNNING at start %s\n",now,this->name());
#endif
				ref_timer_.force_cancel();
			}

		} else {
#ifdef MASAYOSHI_DEBUG
			fprintf(stdout,"%lf rate_change_sync %s %lf %lf\n",now,this->name(),interval_,((size_+RCP_HDR_BYTES)/interval_)/(150000000.0 / 8.0));
#endif

			// sendpkt();
			// rcp_timer_.resched(interval_);
            timeout(); // send a packet immediately and reschedule timer 


			if( interval_ > REF_INTVAL * min_rtt_ && RCP_state != RCP_RUNNING_WREF ){ 
			// the next packet sendingtime > min_rtt and not in REF mode. Enter REF MODE.
				RCP_state = RCP_RUNNING_WREF;
#ifdef MASAYOSHI_DEBUG
				fprintf(stdout,"MASA %lf RCP_RUNNINGF -> RCP_RUNNING_WREF at start %s\n",now,this->name());
#endif
				ref_timer_.resched(REF_INTVAL * min_rtt_);
			}else if ( interval_ <= REF_INTVAL * min_rtt_ && RCP_state == RCP_RUNNING_WREF ){ 
			// the next packet sending time <= min_rtt and in REF mode.  Exit REF MODE
				RCP_state = RCP_RUNNING;
#ifdef MASAYOSHI_DEBUG
				fprintf(stdout,"MASA %lf RCP_RUNNINGF_WREF -> RCP_RUNNING at start %s\n",now,this->name());
#endif
				ref_timer_.force_cancel();
			}
		}
	}
}

void RCPAgent::sendpkt()
{
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);
	rh->set_RCP_rate(0);
	rh->seqno() = seqno_++;

	hdr_cmn *cmnh = hdr_cmn::access(p);
	cmnh->size_ += RCP_HDR_BYTES;

	// To ADD REF Info //////////
	rh->ts_ = Scheduler::instance().clock();
	rh->rtt_ = rtt_;
	rh->set_RCP_pkt_type(RCP_DATA);
	rh->set_RCP_rate(RCP_desired_rate());
    rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
    rh->flowIden() = fid_;
	/////////////////////////////


	lastpkttime_ = Scheduler::instance().clock();
	target_->recv(p, (Handler*)0);

    if (RCP_state == RCP_RETRANSMIT)
        num_pkts_resent_++;
    else
    	num_sent_++;

	// ref_timer_.resched(min(SYN_DELAY, min_rtt_/REF_FACT));
	//
	//
	// WRITECODE HERE: If interval > RTT
	// Then schedule REF packet 
	// If not, cancel REF packet sending.
}

void RCPAgent::sendlast()
{
	Packet* p = allocpkt();
	hdr_rcp *rh = hdr_rcp::access(p);
	rh->set_RCP_rate(0);
	rh->seqno() = seqno_++;
	rh->set_RCP_pkt_type(RCP_FIN);
    rh->set_RCP_numPkts_rcvd(num_dataPkts_received_);
    rh->flowIden() = fid_;

	hdr_cmn *cmnh = hdr_cmn::access(p);
	cmnh->size_ += RCP_HDR_BYTES;

	lastpkttime_ = Scheduler::instance().clock();
	target_->recv(p, (Handler*)0);
	num_sent_++;
}

void RCPATimer::expire(Event* /*e*/) {
	(*a_.*call_back_)();
}

// void RCPAgent::makepkt(Packet* p)
// {
// 	hdr_rcp *rh = hdr_rcp::access(p);

// 	rh->set_RCP_rate(0);
// 	/* Fill in srcid_ and seqno */
// 	rh->seqno() = seqno_++;
// }

// void RCPAgent::sendmsg(int nbytes, const char* /*flags*/)
// {
//         Packet *p;
//         int n;

//         //if (++seqno_ < maxpkts_) {
//                 if (size_)
//                         n = nbytes / size_;
//                 else
//                         printf("Error: RCPAgent size = 0\n");

//                 if (nbytes == -1) {
//                         start();
//                         return;
//                 }
//                 while (n-- > 0) {
//                         p = allocpkt();
//                         hdr_rcp* rh = hdr_rcp::access(p);
//                         rh->seqno() = seqno_;
//                         target_->recv(p);
//                 }
//                 n = nbytes % size_;
//                 if (n > 0) {
//                         p = allocpkt();
//                         hdr_rcp* rh = hdr_rcp::access(p);
//                         rh->seqno() = seqno_;
//                         target_->recv(p);
//                 }
//                 idle();
// //         } else {
// //                 finish();
// //                 // xxx: should we deschedule the timer here? */
// //         }
// }
// void RCPAgent::advanceby(int delta)
// {
//         maxpkts_ += delta;
//         if (seqno_ < maxpkts_ && !running_)
//                 start();
// }               

