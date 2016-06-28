/*
 * rcp-host.h
 *
 */

#ifndef ns_rcp_h
#define ns_rcp_h

#include "config.h"
#include "object.h"
#include "agent.h"
#include "timer-handler.h"
#include "ip.h"

enum RCP_PKT_T {RCP_OTHER, 
		RCP_SYN, 
		RCP_SYNACK, 
		RCP_REF, 
		RCP_REFACK,
		RCP_DATA,
		RCP_ACK,
		RCP_FIN,
		RCP_FINACK};

enum RCP_HOST_STATE {RCP_INACT,
		     RCP_SYNSENT, 
		     RCP_CONGEST,
		     RCP_RUNNING,
		     RCP_RUNNING_WREF,
		     RCP_FINSENT,
             RCP_RETRANSMIT};

struct hdr_rcp {
	int seqno_;
	
	int RCP_enabled_;
	int RCP_pkt_type_;
	double RCP_rate_;  // in bytes per second

	double rtt_;
	double ts_;

	static int offset_;

    int num_dataPkts_received; // useful for retransmission
    int flowId;
    
	inline static int& offset() { return offset_; }
	inline static hdr_rcp* access(const Packet* p) {
		return (hdr_rcp*) p->access(offset_);
	}

	/* per-field member functions */
	//u_int32_t& srcid() { return (srcid_); }
	inline int& seqno() { return (seqno_); }
	inline double& ts() { return (ts_); }
	inline double& rtt() { return (rtt_); }
	inline void set_RCP_rate(double rate) { RCP_rate_ = rate; }
	inline double& RCP_request_rate() { return RCP_rate_; }
	inline void set_RCP_pkt_type(int type) { RCP_pkt_type_ = type;}
	inline int& RCP_pkt_type() { return RCP_pkt_type_; }
    inline void set_RCP_numPkts_rcvd(int numPkts) { num_dataPkts_received = numPkts; }
    inline int& flowIden() { return(flowId); }
};


class RCPAgent;

class RCPATimer : public TimerHandler {
public: 
        RCPATimer(RCPAgent *a, void (RCPAgent::*call_back)()) : TimerHandler() {a_ = a;call_back_=call_back;}
protected:
        virtual void expire(Event *e);
	void (RCPAgent::*call_back_)();
        RCPAgent *a_;
};

class RCPAgent : public Agent {
 public:
        RCPAgent();
        virtual void timeout();
	    virtual void ref_timeout();

        /* For retransmissions */
        virtual void retrans_timeout();

        virtual void recv(Packet* p, Handler*);
        virtual int command(int argc, const char*const* argv);
        //void advanceby(int delta);
        //virtual void sendmsg(int nbytes, const char *flags = 0);
 protected:
        virtual void sendpkt();
        virtual void sendlast();
        void rate_change();
        virtual void start();
        virtual void stop();
        virtual void pause();
        virtual void reset();  /* Masayoshi */
        virtual void finish();
	/* Nandita */ 
	virtual void sendfile(); 
	virtual double RCP_desired_rate();
	inline double min(double d1, double d2){ if (d1 < d2){return d1;} else {return d2;} }

    double lastpkttime_;
	double rtt_;
	double min_rtt_;
    int seqno_;
    int ref_seqno_; /* Masayoshi */
    int init_refintv_fix_; /* Masayoshi */
    double interval_;
	double numpkts_;
	int num_sent_;
	int RCP_state;
	int numOutRefs_;

    /* for retransmissions */ 
    int num_dataPkts_acked_by_receiver_;   // number of packets acked by receiver 
    int num_dataPkts_received_;            // Receiver keeps track of number of packets it received
    int num_Pkts_to_retransmit_;           // Number of data packets to retransmit
    int num_pkts_resent_;                  // Number retransmitted since last RTO 
    int num_enter_retransmit_mode_;        // Number of times we are entering retransmission mode 

    RCPATimer rcp_timer_;
	RCPATimer ref_timer_;

    /* For retransmissions */ 
    RCPATimer rto_timer_;
};


#endif
