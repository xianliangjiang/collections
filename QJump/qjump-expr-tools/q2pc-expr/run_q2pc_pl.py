#! /usr/bin/python

# Copyright (c) 2015, Matthew P. Grosvenor
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the project, the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import sys
import os
import datetime
import time
import subprocess
import signal


if len(sys.argv) < 3:
    print "Usage: run_scale_expr.py <burst_count> <protocol>"
    sys.exit(1)


burst_count = int(sys.argv[1])
protocol  = sys.argv[2]

#Global settings
iphead              = "10.10.0"
time_per_run        = 45   #secs


#q2pc settings
q2pc_client_count   = 7
q2pc_wait           = 5 * 1000 * 1000 #us = 5secs


#q2pc protocol settings
q2pc_protocol       = protocol #"udp-qj" # [ tcp-ln, udp-ln, rudp-ln, udp-qj ]
q2pc_rudp_rto       = 200 * 1000 #us = 200ms
q2pc_rudp_bcast_ip  = "255"
q2pc_port           = 7331

#q2pc server settings
q2pc_srv_thread_count    = 1
q2pc_srv_report_int      = 1000 #samples
q2pc_srv_stats_total     = 8 * 2 * 10 * 10000 + 10  #samples
q2pc_srv_msg_size        = 16
q2pc_priority            = 0


#Netmap settings
nm_port            = 2000
nm_packet_size     = 1500 #bytes
nm_burst_count     = burst_count
nm_vlan_id         = 2
nm_vlan_priority   = 0
nm_wait_time_ns    = 25 * 1000 * 1000 #100ms
nm_timeout         = 60 * 1000 * 1000 * 1000 #secs

#qjump settings
qjump_bytesq        = 1518
qjump_timeq         = 30#us
qjump_p7rate        = 1 #Realtime
qjump_p6rate        = 2 
qjump_p5rate        = 4
qjump_p4rate        = 5
qjump_p3rate        = 10
qjump_p2rate        = -1 #NA - undefined
qjump_p1rate        = -1 #NA - less than best effort
qjump_p0rate        = 24 #Best effort
qjump_verbose       = 0


out_path = "../q2pc_data/"

#  per server settings
#  (hostname, ip )
server =  ( "nikola07", "p786p1", "6", "001b21baa592")
#  per host settings
#  (hostname, ethernet, ip, mac)
clients = [\
    ("quorum208"    , "eth4", "5",  "001b21abe238" ),\
    ("quorum207"    , "eth4", "4",  "90e2ba408bc9" ),\
    ("raphael"      , "p2p1", "8",  "000f530c7e74" ),\
    ("tigger-0"     , "p4p1", "11", "001b218f1830" ),\
    ("uriel"        , "p1p1", "12", "000743122d60" ),\
    ("freestyle"    , "rename7", "13", "0002c93ca441" ),\
    ("michael"      , "rename6", "9",  "001b218f1969" )
]

netmaps = [
    ("quorum206"    , "eth5", "3",  "90e2ba27f800" ),\
    ("quorum201"    , "eth6", "1",  "90e2ba27fbc8" ),\
    #("quorum205"    , "eth5", "2",  "90e2ba27fc08" ),\
]

sinks = [
    ("hammerthrow"  , "p4p1", "7",  "90e2ba58cee4" ),\
    ("backstroke"   , "p4p1", "10", "0007431248a0" ),\
]

#######################################################################################################3
def get_time():
    return datetime.datetime.now().strftime("%Y%m%dT%H%M%S.%f")


def out_line(line):
    
    out_line = "[%s] %s\n" % (get_time(), line)

    readme.write(out_line)
    readme.flush()
    print out_line[:-1]
    sys.stdout.flush()


def run_remote_cmd(subs, host, client_cmd):
    sub_cmd = "ssh qjump@%s \"%s\"" %(host,client_cmd)
    out_line(sub_cmd) 
    
    p = subprocess.Popen(sub_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    subs.append((host,p))
    out_line("Started process with pid=%i for host %s" % (p.pid,host))
    time.sleep(.25)


def run_cmd(subs, host, client_cmd):
    sub_cmd = "%s" %(client_cmd)
    out_line(sub_cmd) 
    
    p = subprocess.Popen(sub_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    subs.append((host,p))
    time.sleep(1)


#Start the data sinks
def run_data_sinks():
    i = 0
    for i in range(0,len(sinks)):        
        (host,eth, ip, mac) = sinks[i]
        (nm_host, nm_eth, nm_ip, nm_mac) = netmaps[i]

        msg = "Starting cluster data sink on host %s (%s.%s:%s) ..." %( host, iphead,ip, nm_port ) 
        out_line(msg)
        
        sink_cmd = "nc -u -l %s.%s %i | nc -u %s.%s %i" % (iphead,ip, nm_port, iphead, nm_ip, nm_port )
        #sink_cmd = "nc -u -l %s.%s %i > /dev/null" % (iphead,ip, nm_port )
        run_remote_cmd(subs,host, sink_cmd)



def kill_data_sinks(sig):
    for sink in sinks:
        (host,eth, ip, mac) = sink
        msg = "Killing cluster data sink on host %s (%s.%s:%s) ..." %(host, iphead,ip, nm_port ) 
        sys.stdout.flush()
        kill_cmd = "sudo killall %s nc" % sig
        run_remote_cmd(subs,host, kill_cmd)

        


#Start the data sources
def run_data_sources():
    i = 0
    for i in range(0,len(sinks)):
        (nm_host, nm_eth, nm_ip, nm_mac) = netmaps[i]
        (sk_host, sk_eth, sk_ip, sk_mac) = sinks[i]
        msg = "Starting cluster data source on %s:%s (%s.%s:%s) --> %s:%s (%s.%s:%s) ..." %(nm_host,nm_eth, iphead, nm_ip, nm_port, sk_host, sk_eth, iphead, sk_ip, nm_port)
        out_line(msg)
        if nm_host == "quorum206":
            nm_opts = [\
               "--interface=%s" % nm_eth, \
               "--src=%s" % nm_ip, \
               "--dst=%s" % sk_ip, \
               "--mac=0x%s" % sk_mac, \
               "--init-seq=0", \
               "--timeout=%i" % nm_timeout,\
               "--wait=%i" % nm_wait_time_ns,\
               "--length=%i" % nm_packet_size,\
               "--burst=%i" % nm_burst_count,\
               "--vlan-id=%i" % nm_vlan_id,\
               "--vlan-pri=%i" % nm_vlan_priority,\
            ]
        else:
            nm_opts = [\
               "--interface=%s" % nm_eth, \
               "--src=%s" % nm_ip, \
               "--dst=%s" % sk_ip, \
               "--mac=0x%s" % sk_mac, \
               "--init-seq=0", \
               "--timeout=%i" % nm_timeout,\
               "--wait=%i" % 5400,\
               "--length=%i" % 1500,\
               "--burst=%i" % 5,\
               "--vlan-id=%i" % nm_vlan_id,\
               "--vlan-pri=%i" % nm_vlan_priority,\
            ]
 

        nm_opts_str = " ".join(nm_opts)

        netmap_cmd = "sudo /home/qjump/qjump-camio-tools/packet_gen/bin/packet_gen %s" % (nm_opts_str )
        pin_cmd = "cd /home/qjump/qjump-expr-tools/pin && sudo ./pin 3 \\\"%s\\\"" % ( netmap_cmd )
        run_remote_cmd(subs, nm_host, pin_cmd)

#end data sources
def kill_data_sources(sig):
    for netamp in netmaps:
        (host,eth, ip, mac) = netamp
        msg = "Killing cluster data source on host %s (%s.%s:%s) ..." % (host, iphead,ip, nm_port ) 
        sys.stdout.flush()
        kill_cmd = "sudo killall %s packet_gen" % sig
        run_remote_cmd(subs, host, kill_cmd)


#start q2pc coordinator
def run_q2pc_coord():

    (srv_host, srv_eth, srv_ip, srv_mac) = server

    msg = "Starting cluster q2pc coordinator %s:%s (%s.%s:%s)..." %( srv_host, srv_eth, iphead, srv_ip, q2pc_port)
    out_line(msg)

    q2pc_opts = [\
        "--%s" % q2pc_protocol, \
        "--server=%i" % q2pc_client_count,\
        "--threads=%i" % q2pc_srv_thread_count,\
        "--report-int=%i" % q2pc_srv_report_int,\
        "--stats-len=%i" % q2pc_srv_stats_total,\
        "--message-size=%i" % q2pc_srv_msg_size,\
        "--rto=%i" % q2pc_rudp_rto,\
        "--wait=%i" % q2pc_wait,\
        "--broadcast=%s.%s" % (iphead,q2pc_rudp_bcast_ip),\
        "--port=%i" % q2pc_port,\
        "--iface=%s.2" % (srv_eth), \
        "-v 3",\
        #"--log-file=/tmp/q2pc_srv_out",\
    ]


    q2pc_opts_str = " ".join(q2pc_opts)

    cmd = "sudo /home/qjump/qjump-q2pc/bin/q2pc %s " % q2pc_opts_str
    priority_cmd = "cd /home/qjump/qjump-app-util/ && sudo ./qjau.py -p %s -w 25000000 -c \\\"%s\\\"" % (q2pc_priority, cmd )

    run_remote_cmd(subs,srv_host, priority_cmd)

def get_q2pc_coord():
     (srv_host, srv_eth, srv_ip, srv_mac) = server

     msg = "Getting data file from coordinator %s:%s (%s.%s:%s)..." %( srv_host, srv_eth, iphead, srv_ip, q2pc_port)
     out_line(msg)

     cmd = "sudo chown qjump:qjump /tmp/q2pc_stats" 
     run_remote_cmd(subs,srv_host, cmd)


     cmd = "scp qjump@%s:/tmp/q2pc_stats %s" % (srv_host, stats_file)
     run_cmd(subs,srv_host, cmd)


    


#end coordinator 
def kill_q2pc_coord(sig):
    (host,eth, ip, mac) = server
    msg = "Killing q2pc coordinator on host %s (%s.%s:%s) ..." %(host, iphead,ip, nm_port ) 
    kill_cmd = "sudo killall %s q2pc" % sig
    run_remote_cmd(subs,host, kill_cmd)

 
#start q2pc clients
def run_q2pc_clients():

    (srv_host, srv_eth, srv_ip, srv_mac) = server
    
    for i in range(0,q2pc_client_count):
    
        (cnt_host, cnt_eth, cnt_ip, cnt_mac) = clients[i]

        msg = "Starting q2pc client on %s:%s (%s.%s:%s) --> %s:%s (%s.%s:%s) ..." %(cnt_host, cnt_eth, iphead, cnt_ip, q2pc_port, srv_host, srv_eth, iphead, srv_ip, q2pc_port)
        out_line(msg)
 
        q2pc_opts = [\
            "--%s" % q2pc_protocol, \
            "--client=%s.%s" % (iphead, srv_ip),\
            "--message-size=%i" % q2pc_srv_msg_size,\
            "--rto=%i" % q2pc_rudp_rto,\
            "--wait=%i" % q2pc_wait,\
            "--broadcast=%s.%s" % (iphead,q2pc_rudp_bcast_ip),\
            "--port=%i" % q2pc_port,\
            "--iface=%s.2" % (cnt_eth), \
            "--id=%i" % (i + 1),\
            "-v 7", \
            #"--log-file=/tmp/q2pc_cnt_out",\
        ]

        q2pc_opts_str = " ".join(q2pc_opts)

        cmd = "sudo /home/qjump/q2pc/bin/q2pc %s " % q2pc_opts_str
        priority_cmd ="cd /home/qjump/qjump-app-util/ && sudo ./qjay.py -p %s -w 25000000 -c \\\"%s\\\"" % (q2pc_priority, cmd )

        run_remote_cmd(subs,cnt_host, priority_cmd)

#end clients
def kill_q2pc_clients(sig):
    for client in clients:
        (host,eth, ip, mac) = client
        msg = "Killing q2pc coordinator on host %s (%s.%s:%s) ..." %(host, iphead,ip, nm_port ) 
        kill_cmd = "sudo killall %s q2pc" % sig
        run_remote_cmd(subs,host, kill_cmd)


#insert qjump module and configure
def insert_qjump():

    boxes = clients + [server]       
    for i in range(0,q2pc_client_count + 1):
        (host, eth, ip, mac) = boxes[i]

        msg = "Inserting q2pc module machine on %s:%s (%s.%s)" %(host, eth, iphead, ip )
        out_line(msg)
 
        module_opts = [\
            "bytesq=%i" % qjump_bytesq,\
            "timeq=%i"  % qjump_timeq,\
            "p7rate=%i" % qjump_p0rate,\
            #"p6rate=%i" % qjump_p1rate,\
            #"p5rate=%i" % qjump_p2rate,\
            "p3rate=%i" % qjump_p3rate,\
            "p3rate=%i" % qjump_p4rate,\
            "p2rate=%i" % qjump_p5rate,\
            "p2rate=%i" % qjump_p6rate,\
            "p0rate=%i" % qjump_p7rate,\
            "verbose=%i" % qjump_verbose,\
        ]

        module_opts_str = " ".join(module_opts)

        cmds =  [\
            "sudo insmod /home/qjump/qjump-tc/sch_qjump.ko %s " % module_opts_str, \
            "sudo tc qdisc add root dev %s qjump " % (eth), \
#            "dmesg | tail -n 50 ", \
        ]

        cmds_str = "; ".join(cmds)

        run_remote_cmd(subs,host, cmds_str)

#end clients
def remove_qjump():
    boxes = clients + [server]       
    for i in range(0,q2pc_client_count + 1):
        (host, eth, ip, mac) = boxes[i]

        msg = "Removing qjump and qdisc host %s (%s.%s) ..." %(host, iphead,ip ) 

        cmds =  [\
            "sudo tc qdisc del root dev %s qjump " % (eth), \
            "sudo rmmod sch_qjump", \
#            "dmesg | tail -n 100 ", \
        ]

        cmds_str = "&& ".join(cmds)

        run_remote_cmd(subs,host, cmds_str)



def get_output():
    for sub in subs:
        (host,proc) = sub
        if proc.poll() != None:
            out_line("Process with pid=%i on host %s has ended" % (proc.pid,host))
            out_line(host + "-- STDOUT:")
            (stdo, stde) = proc.communicate()
            out_line(stdo)
            out_line(stde)
            subs.remove(sub)

    #for sub in subs_extra:
    #    (host,proc) = sub
    #    if proc.poll() != None:
    #        out_line("Process with pid=%i on host %s has ended" % (proc.pid,host))
    #        out_line(host + "-- STDOUT:")
    #        (stdo, stde) = proc.communicate()
    #        out_line(stdo)
    #        out_line(stde)
    #        subs_extra.remove(sub)



##############################################################################################################
#Hold subprocess classes until we destroy everything
subs = []
 

global_id = get_time()

msg = "Making directory %s" % out_path
if not os.path.exists(out_path):
    os.makedirs(out_path)


if q2pc_protocol == "rdp-ln":
    proto =  "%s.%s" % ( q2pc_protocol, q2pc_rudp_rto)
else :
    proto =  q2pc_protocol

readme = open(out_path + "README_%s_%s_%s" % (proto,nm_burst_count, global_id), "w")
out_line(msg)
stats_file = out_path + "q2pc_stats_%s_%s_%s" % (proto,nm_burst_count, global_id)
out_line("Saving stats to %s" % stats_file) 

#Put settings in the log
out_line("iphead=%s" % iphead)
out_line("time_per_run=%s" % time_per_run)
out_line("q2pc_client_count=%s" % q2pc_client_count)
out_line("q2pc_wait=%s" % q2pc_wait)
out_line("q2pc_protocol=%s" % q2pc_protocol)
out_line("q2pc_rudp_rto=%s" % q2pc_rudp_rto)
out_line("q2pc_rudp_bcast_ip=%s" % q2pc_rudp_bcast_ip) 
out_line("q2pc_port=%s" % q2pc_port)
out_line("q2pc_srv_thread_count=%s" % q2pc_srv_thread_count)
out_line("q2pc_srv_report_int=%s" % q2pc_srv_report_int)
out_line("q2pc_srv_stats_total=%s" % q2pc_srv_stats_total) 
out_line("q2pc_srv_msg_size=%s" % q2pc_srv_msg_size)
out_line("q2pc_priority=%s" % q2pc_priority) 
out_line("nm_port=%s" % nm_port)
out_line("nm_packet_size=%s" % nm_packet_size) 
out_line("nm_burst_count=%s" % nm_burst_count)
out_line("nm_vlan_id=%s" % nm_vlan_id)
out_line("nm_vlan_priority=%s" % nm_vlan_priority)
out_line("nm_wait_time_ns=%s" % nm_wait_time_ns) 
out_line("nm_timeout=%s" % nm_timeout) 
out_line("qjump_bytesq=%i" % qjump_bytesq)
out_line("qjump_timeq=%i" % qjump_timeq)
out_line("qjump_p7rate=%i" % qjump_p7rate) 
out_line("qjump_p6rate=%i" % qjump_p6rate)
out_line("qjump_p5rate=%i" % qjump_p5rate) 
out_line("qjump_p4rate=%i" % qjump_p4rate) 
out_line("qjump_p3rate=%i" % qjump_p3rate)
out_line("qjump_p2rate=%i" % qjump_p2rate)
out_line("qjump_p1rate=%i" % qjump_p1rate)
out_line("qjump_p0rate=%i" % qjump_p0rate)


#Kill everything before we get going
kill_q2pc_clients("-9")
kill_q2pc_coord("-9")
kill_data_sources("-9")
kill_data_sinks("-9")
remove_qjump()

time.sleep(2)
get_output()
    
run_data_sinks()
run_data_sources()


if q2pc_protocol == "udp-qj":
    #need to install and configure the qjump module
    insert_qjump()

    #also change the vlan prioity 
    q2pc_priority = 7
    out_line("q2pc_priority=%s" % q2pc_priority) 

get_output()

#Wait for everything to stabelize
msg = "Waiting ..." 
out_line(msg)

for i in range(0,25):
    time.sleep(1)
    print ".",
    sys.stdout.flush()

out_line("\n")

run_q2pc_coord()
for i in range(0,12):
    time.sleep(1)
    print ".",
    sys.stdout.flush()
run_q2pc_clients()

i = 0
end = time_per_run
early_exit = False
while(i < end):
    coord_exists = 0
    for sub in subs:        
        (host,proc) = sub
        if proc.poll() != None:
            out_line("Process with pid=%i on host %s has ended" % (proc.pid,host))
            (stdo, stde) = proc.communicate()
            out_line(host + "-- STDOUT:")
            out_line(stdo)
            out_line(host + "-- STDERR:")
            out_line(stde)
            subs.remove(sub)
            if(subs == []):
                i = end
                early_exit = True
                break

            continue
            
        if host == server[0]:
            coord_exists = 1


    time.sleep(1)
    print ".",
    sys.stdout.flush()
    i += 1

print "\n"
sys.stdout.flush()

if early_exit:
    out_line("**************   Exited due to all subs empty\n")
    out_line("Waiting for things to cool down\n")
    time.sleep(20)
    out_line("Waiting for things to cool down. Done\n")
else:
    out_line("**************   Exited due to time out\n")

get_output()


kill_q2pc_clients("")
kill_q2pc_coord("")
kill_data_sources("")
kill_data_sinks("")
remove_qjump()

time.sleep(5)
get_output()
get_q2pc_coord()
get_output()



kill_q2pc_clients("-9")
kill_q2pc_coord("-9")
kill_data_sources("-9")
kill_data_sinks("-9")
remove_qjump()


out_line("Finished.")

