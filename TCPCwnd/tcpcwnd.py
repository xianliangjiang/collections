#!/usr/bin/python

"CS 244 Assingment 3: Increasing initial congestion window"

from mininet.topo import Topo
from mininet.node import CPULimitedHost
from mininet.link import TCLink
from mininet.net import Mininet
from mininet.log import lg
from mininet.util import dumpNodeConnections
from mininet.cli import CLI
from mininet.util import pmonitor
from signal import SIGINT

import subprocess
from subprocess import Popen, PIPE
from time import sleep, time
from multiprocessing import Process
import termcolor as T
from argparse import ArgumentParser
import random

import sys
import os

# Parse arguments
parser = ArgumentParser(description="Cwnd adjusting")

parser.add_argument('-n',
                    dest="n",
                    type=int,
                    action="store",
                    help="Number of nodes in star.  Must be >= 3",
                    required=True)

parser.add_argument('--maxq',
                    dest="maxq",
                    action="store",
                    help="Max buffer size of network interface in packets",
                    default=1000)

parser.add_argument('--cong',
                    dest="cong",
                    help="Congestion control algorithm to use",
                    default="bic")

parser.add_argument('-o', '--out',
                    help="Save plot to output file, e.g.: --out plot.png",
                    dest="out",
                    required=True,
                    default=None)

parser.add_argument('-r', '--response',
                    help="Responsive of http server",
                    dest="resp_size",
                    required=True,
                    default = 9000)

args = parser.parse_args()

# Topology to be instantiated in Mininet
class StarTopo(Topo):
    "Star topology for Buffer Sizing experiment"

    def __init__(self, n=3, cpu=.5, maxq=10000):
        # Add default members to class.
        super(StarTopo, self ).__init__()
        self.n = n
        self.cpu = cpu
        self.maxq = maxq
        self.bwMap = {}     # BW in Mbps
        self.create_topology()
        
    def create_topology(self):

        # add switch
        switch = self.addSwitch('s0')

        # add hosts and links
        for h in range(0, self.n):
            host_name = 'h%s' % h
            host = self.addHost(host_name)
            if h == 0:
                bw_inst = 100 # link to server has 100 Mbps BW
                delay_inst = '%fms' % (70.0/4) # Based on median RTT 70ms
            else:
                bw_inst = getBW()/1000.0    # kbps -> Mbps
            '''
            if bw_inst == 50.0/1000.0:
                rtt = 200
            else:
                rtt = 90
            '''
            delay_inst = '%fms' % (getRTT()/4.)
                        
            linkopts = dict(bw=bw_inst, delay=delay_inst,
                    max_queue_size=10000, htb=True)

            self.addLink(host, switch, **linkopts)
            self.bwMap[host_name] = bw_inst*1000
            # Let h0 be the front-end server

def getBW():
    sample = random.uniform(0, 1)
    if sample < 0.125:
        return 50  # kbps
    elif sample < 0.25:
        return 200
    elif sample < 0.375:
        return 500
    elif sample < 0.5:
        return 900
    elif sample < 0.625:
        return 1600
    elif sample < 0.75:
        return 2500
    elif sample < 0.875:
        return 3500
    else:
        return 5500

def getRTT():
    return 70
    sample = random.uniform(0, 1)
    if sample < 0.25:
        return 31/2.0
    elif sample < 0.5:
        return (31+70)/2.0
    elif sample < 0.75:
        return (70+120)/2.0
    else:
        return (120+1000)/2.0

def set_all_winds(net, num_seg, wind_type):
    for host_id in xrange(0, args.n):
        hostName = 'h%d' % host_id
        if wind_type == "cwnd":
            set_init_cwnd(net, hostName, num_seg)
        elif wind_type == "rwnd":
            set_init_rwnd(net, hostName, num_seg)
        else:
            print "[set_all_winds] Invalid window type!"
            return

def set_init_cwnd(net, hostName, num_seg):
    ''' --How to change initial cwnd--
        ip route show
        sudo ip route change [Paste the current settings for default] initcwnd 10
    '''

    # print "Changing initcwnd of %s to %d..." % (hostName, num_seg)  
    
    h = net.getNodeByName(hostName)

    result = h.cmd('ip route show')
    result = result.rstrip('\n')
    result = h.cmd('ip route change %s initcwnd %d' % (result.rstrip('\n'), num_seg))    
    h.cmd('ip route flush cache')

    # Verify
    result = h.cmd('ip route show')
    result = result.rstrip('\n')
    # print result

def set_init_rwnd(net, hostName, num_seg):
    # print "Changing initrwnd of %s to %d..." % (hostName, num_seg)

    h = net.getNodeByName(hostName)

    result = h.cmd('ip route show')
    result = result.rstrip('\n')
    result = h.cmd('ip route change %s initrwnd %d' % (result.rstrip('\n'), num_seg))
    result = h.cmd('ip route flush cache')

    # Verify
    result = h.cmd('ip route show')
    result = result.rstrip('\n')
    # print result
    # Also make it send sequentially

# Start http server at node h1
def start_http_server(net):
    # print "Starting HTTP server at h0..."
    server = net.getNodeByName("h0")
    server.popen("python httpServer.py %s" % args.resp_size)    

# http request from node <clientName>
def http_request(net, clientName, outputFile):
    print "Requesting HTTP server from node %s of %d" % (clientName[1:], args.n - 1)
    client = net.getNodeByName(clientName)
    server = net.getNodeByName("h0")
    r = client.cmd("time wget -q -O /dev/null %s:8000" % server.IP())
    outputFile.write(r)
    sleep(2)

# Main function
def main():
    "Create network and run Buffer Sizing experiment"

    start = time()
    # Reset to known state
    topo = StarTopo(n=args.n, maxq=args.maxq)
    
    net = Mininet(topo=topo, host=CPULimitedHost, link=TCLink)
    net.start()
    # dumpNodeConnections(net.hosts)
    # net.pingAll()

    # Set initial receive window of all hosts
    set_all_winds(net, 20, "rwnd")

    # Run simple http server on h1
    start_http_server(net)

    # Experiment
    cwnd_list = [3, 10]

    # output file
    f = open(args.out, 'w')

    for host_id in xrange(1, args.n):
        client = "h%d" % host_id
        client_bw = str(topo.bwMap[client])
        f.write("Bandwidth: " + client_bw)
        for cwnd in cwnd_list:
            # Set initial congestion window of server
            set_init_cwnd(net, "h0", cwnd)
            set_init_cwnd(net, client, cwnd)
            # Send request and measure response time
            resp_time = http_request(net, client, f)

    f.close()

if __name__ == '__main__':
    try:
        main()
    except:
        print "-"*80
        print "Caught exception.  Cleaning up..."
        print "-"*80
        import traceback
        traceback.print_exc()
        os.system("killall -9 top bwm-ng tcpdump cat mnexec iperf; mn -c")
