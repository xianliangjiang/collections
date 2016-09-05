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
# * Neither the name of the project, the name of copyright holder nor the names 
#   of its contributors may be used to endorse or promote products derived from
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


#Global settings
port            = 2000
iphead          = "10.10"
dagsamples      = 10 * 1000 * 1000
dagtimeout      = 120 #seconds
sm              = 5
nwepoch         = int(sys.argv[1]) #11 #((3*4000) + (240 * 256 * 8 * .1)) / 1000 #us
nwpsize         = 230 
pwfull          = 65502
dodag           = True
clientsperbox   = 3

#  per server settings
#  (hostname, ip )
servers =[\
    ("quorum205"    ,"1.2"  ),\
#]    ("quorum205"    ,"0.2"  ),\
#    ("quorum205"    ,"1.2"  )\
]

#servers =[\
#    ("backstroke"   ,"0.10"  ),\
#    ("backstroke"   ,"1.10"  ),\
#]



#  per host settings
#  (hostname, ip, seqstart )
clients = [\
    ("quorum206"    ,servers[0][1], dagsamples * sm * 0  ),\
    ("quorum207"    ,servers[0][1], dagsamples * sm * 1  ),\
    ("quorum208"    ,servers[0][1], dagsamples * sm * 4  ),\
    ("quorum301"    ,servers[0][1], dagsamples * sm * 5  ),\
    ("hammerthrow"  ,servers[0][1], dagsamples * sm * 6  ),\
    ("michael"      ,servers[0][1], dagsamples * sm * 7  ),\
    ("backstroke"   ,servers[0][1], dagsamples * sm * 8  ),\
    ("tigger-0"     ,servers[0][1], dagsamples * sm * 9  ),\
    ("uriel"        ,servers[0][1], dagsamples * sm * 10 ),\
    ("freestyle"    ,servers[0][1], dagsamples * sm * 11 ),\
    ("quorum201"    ,servers[0][1], dagsamples * sm * 5  )\
]




#Hold subprocess classes until we destroy everything
subs = []
msg1 = "[%s] Starting..." % (datetime.datetime.now())
print msg1 



def run_remote_cmd(subs, readme, host, client_cmd):
    sub_cmd = "ssh qjump@%s \"%s\"" %(host,client_cmd)
    readme.write("%s\n" % (sub_cmd) )
    readme.flush()
#    print sub_cmd
    
    p = subprocess.Popen(sub_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    subs.append((host,p))
#    print "Done."


def run_perf(subs, readme, client_idx, priority, psize, pacing ):
    (host, ip, seq_start) = clients[client_idx]
    perf_cmd = "/home/qjump/qjump/qjump_fe2p_aps/fe2p_perf/bin/fe2p_perf_c --ip-port=udp:%s.%s:%s --size=%s --seq-start=%i --pacing=%i" % (iphead,ip,port,psize,seq_start,pacing)

    client_cmd = "cd /home/qjump/qjump/set_sock_priority && sudo ./set_sock_priority -p %s -w 2500000 -c \\\"%s\\\"" % (priority,perf_cmd )
    readme.write("%s\n%s\n" % (msg, client_cmd) )
    readme.flush()
    
    run_remote_cmd(subs, readme, host, client_cmd)


out_path = "/home/qjump/qjump_full_test/"
msg2 = "Making directory %s" % out_path
print msg2
if not os.path.exists(out_path):
    os.makedirs(out_path)

#Setup the log
readme = open(out_path + "README", "w")
readme.write("%s\n%s\nStarted: %s\n" % (msg1,msg2,str(datetime.datetime.now())) )
readme.flush()

#Start the servers
for server in servers:
    (host,ip) = server
    msg = "[%s] Starting cluster test server on %s.%s.%s:%s ..." %(datetime.datetime.now(), host, iphead,ip, port)
    print msg
    sys.stdout.flush()
    
    srv_cmd = "nc -u -l %s.%s %i > /dev/null" % (iphead,ip, port )
    run_remote_cmd(subs,readme, host, srv_cmd)

#Start up the clients 
for client_idx in range(0,len(clients)):
    (host, ip, seq) = clients[client_idx]
    
    #Set up the high priority client 
    msg = "[%s] Starting HIGH cluster test client [%i] on %s --> %s.%s:%s ..." % (datetime.datetime.now(), client_idx+1,host,iphead,ip,port)
    print msg
    sys.stdout.flush()

    for i in range(0,clientsperbox):
        run_perf(subs, readme, client_idx, 7, nwpsize, nwepoch)
    
    
    #Set up the low priority client - nasty should be a function
    msg = "[%s] Starting LOW cluster test client [%i] on %s --> %s.%s:%s ..." % (datetime.datetime.now(), client_idx+1,host,iphead,ip,port)
    print msg
    sys.stdout.flush()

    #for i in range(0,clientsperbox):
    run_perf(subs, readme, client_idx, 0, pwfull, 0 )
 

#Wait for everything to stabelize
msg = "[%s] Waiting ..." % (datetime.datetime.now())
print msg
sys.stdout.flush()

for i in range(0,5):
    time.sleep(1)
    print ".",
    sys.stdout.flush()
print "Done."
sys.stdout.flush()
readme.write("%s\n" % (msg) )
readme.flush()

 
if dodag :
    #Start up the DAG capture
    msg = "[%s] Starting DAG capture on %s ..." %(datetime.datetime.now(), "quorum208")
    print msg
    sys.stdout.flush()
    dag_cmd = "sudo dagconfig -s; sleep 2; sudo dagconfig -s; sleep 1; sudo ~/qjump/qjump-expr-tools/pin/pin 7 \\\"qjump/qjump-camio-tools/dag_captur2/bin/dag_capture -s %i -t 900\\\"" % (dagsamples)
    sub_cmd = "ssh qjump@%s \"%s\"" %("quorum208",dag_cmd)
    readme.write("%s\n%s\n" % (msg, sub_cmd) )
    readme.flush()
    dag_sub = subprocess.Popen(sub_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    print "Done."

    #Wait for the capture to complete
    print "[%s] Waiting for DAG capture to finish..." %(datetime.datetime.now()),
    sys.stdout.flush()
    (stdout, stderr) = dag_sub.communicate()
    readme.write(stdout + "\n")
    readme.write(stderr + "\n")
    readme.flush()
    print "Done." 

    #Copy the DAG output
    msg = "[%s] Copy the DAG output..." % (datetime.datetime.now())

    sys.stdout.flush()
    out_cmd = "scp qjump@%s:/tmp/dag_cap_* %s" % ("quorum208",out_path)
    readme.write(out_cmd + "\n")
    readme.flush()
    out_sub = subprocess.Popen(out_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (stdout, stderr) = out_sub.communicate()
    readme.write(stdout + "\n")
    readme.write( stderr + "\n")
    readme.flush()
    print "Done." 

#kill all the clients
#for sub in subs:
for client in clients:
    #Kill process on the remote box
#    (host,sub) = sub
    host = client[0]
    print "[%s] Killing remote process on %s.." % (datetime.datetime.now(),host) ,
    sys.stdout.flush()
    kill_cmd = "sudo killall -9 fe2p_perf_c"
    sub_cmd = "ssh qjump@%s \"%s\"" %(host,kill_cmd)
    readme.write(sub_cmd + "\n")
    readme.flush()

    #Collect the ouput
    #readme += sub.stdout.read()
    #readme += sub.stderr.read()
    subprocess.call(sub_cmd, shell=True)

    print "Done." 

for server in servers:
    #Kill process on the remote box
#    (host,sub) = sub
    host = server[0]
    print "[%s] Killing remote process on %s.." % (datetime.datetime.now(),host) ,
    sys.stdout.flush()
    kill_cmd = "sudo killall -9 nc"
    sub_cmd = "ssh qjump@%s \"%s\"" %(host,kill_cmd)
    readme.write(sub_cmd + "\n")
    readme.flush()

    #Collect the ouput
    #readme += sub.stdout.read()
    #readme += sub.stderr.read()
    subprocess.call(sub_cmd, shell=True)

    print "Done." 



#All done
readme.write("Finished %s\n" % str(datetime.datetime.now()))
readme.flush()

readme.close()
print "[%s] Finished!\n\n" % (datetime.datetime.now())


