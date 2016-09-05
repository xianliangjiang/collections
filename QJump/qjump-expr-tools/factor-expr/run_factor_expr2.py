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


if len(sys.argv) < 2:
    print "Usage: run_scale_expr.py <epoch time us>"
    sys.exit(1)


#Global settings
port            = 2000
iphead          = "10.10"
dagsamples      = 10 * 1000 * 1000
dagtimeout      = 10 #seconds
sm              = 5
nwepoch         = int(sys.argv[1]) #11 #((3*4000) + (240 * 256 * 8 * .1)) / 1000 #us
nwpsize         = 230 
pwfull          = 65502
dodag           = False
clientsperbox   = 3

#  per server settings
#  (hostname, ip )
servers =[\
    ("quorum205"    ,"1.2"  ),\
    ("quorum205"    ,"0.2"  )\
]

#servers =[\
#    ("backstroke"   ,"0.10"  ),\
#    ("backstroke"   ,"1.10"  ),\
#]



#  per host settings
#  (hostname, ip, seqstart )
clients = [\
    ("quorum206"    ,servers[1][1], dagsamples * sm * 0  ),\
    ("quorum207"    ,servers[1][1], dagsamples * sm * 1  ),\
    ("quorum208"    ,servers[1][1], dagsamples * sm * 4  ),\
    ("quorum301"    ,servers[1][1], dagsamples * sm * 5  ),\
    ("hammerthrow"  ,servers[1][1], dagsamples * sm * 6  ),\
    ("michael"      ,servers[1][1], dagsamples * sm * 7  ),\
    ("backstroke"   ,servers[1][1], dagsamples * sm * 8  ),\
    ("tigger-0"     ,servers[1][1], dagsamples * sm * 9  ),\
    ("uriel"        ,servers[1][1], dagsamples * sm * 10 ),\
    ("freestyle"    ,servers[1][1], dagsamples * sm * 11 ),\
    ("quorum201"    ,servers[1][1], dagsamples * sm * 12 )
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


def run_perf(subs, readme, client_idx, priority, psize , pacing):
    (host, ip, seq_start) = clients[client_idx]
    perf_cmd = "/home/qjump/qjump/qjump-camio-tools/camio_perf/bin/camio_perf_c --ip-port=udp:%s.%s:%s --size=%s --seq-start=%i --pacing=%i" % (iphead,ip,port,psize,seq_start, pacing)

    client_cmd = "cd /home/qjump/qjump-app-util && sudo ./qjau.py -p %s -w 2500000 -c \\\"%s\\\"" % (priority,perf_cmd )
    readme.write("%s\n%s\n" % (msg, client_cmd) )
    readme.flush()
    
    run_remote_cmd(subs, readme, host, client_cmd)


out_path = "/home/qjump/qjump_full_test/"
msg2 = "Making directory %s" % out_path
print msg2
if not os.path.exists(out_path):
    os.makedirs(out_path)

#Setup the log
readme = open(out_path + "README2", "w")
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
    #msg = "[%s] Starting LOW cluster test client [%i] on %s --> %s.%s:%s ..." % (datetime.datetime.now(), client_idx+1,host,iphead,ip,port)
    #print msg
    #sys.stdout.flush()

    #for i in range(0,clientsperbox):
    #run_perf(subs, readme, client_idx, 0, pwfull, 0 )
 

sys.exit(0)

#Wait for everything to stabelize
msg = "[%s] Waiting ..." % (datetime.datetime.now())
print msg
sys.stdout.flush()

for i in range(0,10):
    time.sleep(1)
    print ".",
    sys.stdout.flush()
print "Done."
sys.stdout.flush()
readme.write("%s\n" % (msg) )
readme.flush()

os.wait()

